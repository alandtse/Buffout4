#pragma once
// based on details from https://www.nexusmods.com/fallout4/mods/75216?tab=description
// essentially, this will manually reset a lock on a threshold of 8.
#include <atomic>

namespace Fixes::EscapeFreeze
{
	struct lockPatchAddresses
	{
		const std::string name;
		const int id;
		const int offset;
	};

	struct unlockPatchAddresses
	{
		const std::string name;
		const int id;
		const int startOffset;
		const int endOffset;
	};

	const std::vector<std::pair<lockPatchAddresses, unlockPatchAddresses>> addresses = {
		//{ { "BGSEntryPointPerkEntry::CheckConditionFilters", 1055546, 0x84 }, { "BGSEntryPointPerkEntry::CheckConditionFilters", 1055546, 0x2a3, 0x2d6 } },  // https://www.nexusmods.com/fallout4/mods/75216?tab=description
		// BGSEntryPoint currently causes a CTD on load game.
		{ { "VATS::GetCurrentAction", 711090, 0x26 }, { "VATS::GetCurrentAction", 711090, 0x6c, 0x86 } }  // VATS freeze
	};

	class Spinlock  // to replace bad bsspinlock
	{
	private:
		std::atomic_flag atomic_flag = ATOMIC_FLAG_INIT;

	public:
		void lock()
		{
			_lockCount++;
			while (atomic_flag.test_and_set(std::memory_order_acquire)) {
				Sleep(1);
			}
			_owningThread = REX::W32::GetCurrentThreadId();
		}
		void unlock()
		{
			if (_owningThread == REX::W32::GetCurrentThreadId()) {
				atomic_flag.clear(std::memory_order_release);
				_owningThread = 0;
				_lockCount--;
			}
		}

		// members
		std::atomic<std::uint32_t> _owningThread{ 0 };        // 0
		volatile std::atomic<std::uint32_t> _lockCount{ 0 };  // 4
	};

	static std::vector<Spinlock*> spinlocks{};

	struct detail
	{
		struct Patch :
			Xbyak::CodeGenerator
		{
			Patch(uintptr_t a_dst, uintptr_t a_newlock)
			{
				Xbyak::Label dst;

				mov(rcx, a_newlock);  // prepare new spinlock as first arg
				jmp(ptr[rip + dst]);  // jump to new function

				L(dst);
				dq(a_dst);
			}
		};

		static void lock(Spinlock* a_sl)
		{
			a_sl->lock();
		};

		static void unlock(Spinlock* a_sl)
		{
			a_sl->unlock();
		};
	};

	void Install()
	{
		auto& trampoline = F4SE::GetTrampoline();
		for (const auto& pair : addresses) {
			auto& lockAddress = pair.first;
			auto& unlockAddress = pair.second;
			auto sl = new Spinlock();
			spinlocks.push_back(sl);

			//lock
			REL::Relocation<uintptr_t> lockTarget{ REL::ID(lockAddress.id), lockAddress.offset };
			detail::Patch lockP{ reinterpret_cast<uintptr_t>(&EscapeFreeze::detail::lock), reinterpret_cast<uintptr_t>(sl) };
			lockP.ready();
			trampoline.write_call<5>(lockTarget.address(), trampoline.allocate(lockP));
			logger::info("installed EscapeFreeze lock fix: {0:x}", lockTarget.address());

			//unlock
			REL::Relocation<uintptr_t> unlockTarget{ REL::ID(unlockAddress.id), unlockAddress.startOffset };
			REL::Relocation<uintptr_t> end{ REL::ID(unlockAddress.id), unlockAddress.endOffset };
			const auto instructionBytes = end.address() - unlockTarget.address();
			for (std::size_t i = 0; i < instructionBytes; i++) {
				REL::safe_write(unlockTarget.address() + i, REL::NOP);
			}
			detail::Patch unlockP{ reinterpret_cast<uintptr_t>(&EscapeFreeze::detail::unlock), reinterpret_cast<uintptr_t>(sl) };
			unlockP.ready();
			trampoline.write_call<5>(unlockTarget.address(), trampoline.allocate(unlockP));
			logger::info("installed EscapeFreeze unlock fix: {0:x}", unlockTarget.address());
		}
		logger::info("installed {} locks for EscapeFreeze fix", spinlocks.size());
		for (const auto& spinlock : spinlocks) {
			logger::debug("{:x}", reinterpret_cast<uintptr_t>(spinlock));
		}
	}

}
