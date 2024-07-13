#pragma once

namespace Fixes::SafeExitFix
{
	namespace detail
	{
		inline void Shutdown()
		{
			spdlog::default_logger()->flush();
			REX::W32::TerminateProcess(REX::W32::GetCurrentProcess(), EXIT_SUCCESS);
		}
	}

	inline void Install()
	{
		auto& trampoline = F4SE::GetTrampoline();
		REL::Relocation<std::uintptr_t> target{ REL::RelocationID(668528, 2718225), REL::VariantOffset(0x20, 0x20b, 0x20) };  // NG is a different call function location
		trampoline.write_call<5>(target.address(), detail::Shutdown);
		logger::info("installed SafeExit fix"sv);
	}
}
