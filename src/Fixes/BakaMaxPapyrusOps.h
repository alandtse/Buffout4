// based on MIT code from https://github.com/shad0wshayd3-FO4/BakaMaxPapyrusOps/commit/525706f609906d2eae59084854c81306b9b19743
// which was ultimately based on https://www.nexusmods.com/skyrimspecialedition/mods/77779
class BakaMaxPapyrusOpsFixes
{
public:
	class FixScriptPageAllocation
	{
	public:
		static void Install()
		{
			static REL::Relocation<std::uintptr_t> target{ RE::BSScript::SimpleAllocMemoryPagePolicy::VTABLE[0] };
			_GetLargestAvailablePage = target.write_vfunc(0x04, GetLargestAvailablePage);
			logger::info("Installed FixScriptPageAllocation"sv);
		}

	private:
		static RE::BSScript::SimpleAllocMemoryPagePolicy::AllocationStatus GetLargestAvailablePage(
			RE::BSScript::SimpleAllocMemoryPagePolicy* a_this,
			RE::BSTSmartPointer<RE::BSScript::MemoryPage, RE::BSTSmartPointerAutoPtr>& a_newPage)
		{
			const RE::BSAutoLock lock{ a_this->dataLock };

			auto maxPageSize = a_this->maxAllocatedMemory - a_this->currentMemorySize;
			auto currentMemorySize = a_this->currentMemorySize;
			if (maxPageSize < 0) {
				a_this->currentMemorySize = a_this->maxAllocatedMemory;
			}

			auto result = _GetLargestAvailablePage(a_this, a_newPage);
			if (maxPageSize < 0) {
				a_this->currentMemorySize = currentMemorySize;
			}

			return result;
		}

		inline static REL::Relocation<decltype(&GetLargestAvailablePage)> _GetLargestAvailablePage;
	};

	class FixToggleScriptsCommand
	{
	public:
		static void Install()
		{
			{
				static REL::Relocation<std::uintptr_t> target{ REL::ID(1321787), 0x5A };
				auto& trampoline = F4SE::GetTrampoline();
				trampoline.write_call<5>(target.address(), EndSaveLoad);
			}

			{
				static REL::Relocation<std::uintptr_t> target{ REL::ID(1014572), 0x5CF };
				auto& trampoline = F4SE::GetTrampoline();
				trampoline.write_call<5>(target.address(), EndSaveLoad);
			}

			{
				static REL::Relocation<std::uintptr_t> target{ REL::ID(124452), REL::Relocate(0x32F, 0x347) };  // VR 0x347
				auto& trampoline = F4SE::GetTrampoline();
				trampoline.write_call<5>(target.address(), EndSaveLoad);
			}

			{
				static REL::Relocation<std::uintptr_t> target{ REL::ID(371005), 0x478 };
				auto& trampoline = F4SE::GetTrampoline();
				trampoline.write_call<5>(target.address(), EndSaveLoad);
			}

			{
				static REL::Relocation<std::uintptr_t> target{ REL::ID(1238017), 0x370 };
				auto& trampoline = F4SE::GetTrampoline();
				_Freeze = trampoline.write_call<5>(target.address(), Freeze);
			}
			logger::info("Installed FixToggleScriptsCommand"sv);
		}

	private:
		static void EndSaveLoad(RE::GameVM* a_this)
		{
			if (a_this->saveLoadInterface) {
				a_this->saveLoadInterface->CleanupLoad();
				a_this->saveLoadInterface->CleanupSave();
			}

			a_this->RegisterForAllGameEvents();
			a_this->saveLoad = false;

			a_this->handlePolicy.DropSaveLoadRemapData();
			a_this->objectBindPolicy.EndSaveLoad();
			a_this->handlePolicy.UpdatePersistence();

			if (RE::Script::GetProcessScripts()) {
				const RE::BSAutoLock lock{ a_this->freezeLock };
				a_this->frozen = false;
			}
		}

		static void Freeze(RE::GameVM* a_this, bool a_freeze)
		{
			if (RE::Script::GetProcessScripts()) {
				return _Freeze(a_this, a_freeze);
			}
		}

		inline static REL::Relocation<decltype(&Freeze)> _Freeze;
	};
};

class BakaMaxPapyrusOpsTweaks
{
public:
	class MaxPapyrusOpsPerFrame
	{
	public:
		static void Install()
		{
			struct PapyrusOpsPerFrame : Xbyak::CodeGenerator
			{
				PapyrusOpsPerFrame(std::uintptr_t a_begin, std::uintptr_t a_end)
				{
					inc(r14d);
					cmp(r14d, MaxOpsPerFrame);
					jb("Loop");
					mov(rcx, a_end);
					jmp(rcx);
					L("Loop");
					mov(rcx, a_begin);
					jmp(rcx);
				}
			};

			REL::Relocation<std::uintptr_t> target{ REL::ID(614585), 0x4F0 };
			REL::Relocation<std::uintptr_t> loopBeg{ REL::ID(614585), 0x0A0 };
			REL::Relocation<std::uintptr_t> loopEnd{ REL::ID(614585), 0x4FD };

			auto code = PapyrusOpsPerFrame(loopBeg.address(), loopEnd.address());
			REL::safe_fill(target.address(), REL::NOP, 0x0D);

			auto& trampoline = F4SE::GetTrampoline();
			auto result = trampoline.allocate(code);
			trampoline.write_branch<5>(target.address(), reinterpret_cast<std::uintptr_t>(result));
			logger::info("Installed MaxPapyrusOpsPerFrame {0:x}"sv, target.address());
		}

		template <typename T>
		static void Update(T a_value)
		{
			MaxOpsPerFrame = static_cast<std::int32_t>(a_value);
		}

	private:
		inline static std::int32_t MaxOpsPerFrame{ 100 };  // vanilla is 100
	};
};
