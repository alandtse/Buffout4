#pragma once

namespace Fixes::SafeExitFix
{
	namespace detail
	{
		inline void Shutdown()
		{
			REX::W32::TerminateProcess(REX::W32::GetCurrentProcess(), EXIT_SUCCESS);
		}
	}

	inline void Install()
	{
		auto& trampoline = F4SE::GetTrampoline();
		REL::Relocation<std::uintptr_t> target{ REL::ID(668528), 0x20 };
		trampoline.write_call<5>(target.address(), detail::Shutdown);
		logger::info("installed SafeExit fix"sv);
	}
}
