#pragma once

namespace Fixes::PipboyLightInvFix
{
	namespace detail
	{
		struct Patch : Xbyak::CodeGenerator
		{
			explicit Patch(std::uintptr_t a_dest)
			{
				Xbyak::Label retLab;

				and_(dword[rdi + 0x4], 0xFFFFFFF);
				mov(rcx, 1);
				jmp(ptr[rip + retLab]);

				L(retLab);
				dq(a_dest);
			}
		};

	}

	inline void Install()
	{
		REL::Relocation<std::uintptr_t> target{ REL::Offset(0xf2d240), 0x48 };
		REL::Relocation<std::uintptr_t> resume{ REL::Offset(0xf2d240), 0x4D };

		detail::Patch p{ resume.address() };
		p.ready();

		auto& trampoline = F4SE::GetTrampoline();
		trampoline.write_branch<5>(
			target.address(),
			trampoline.allocate(p));

		logger::info("installed PipboyLightInvFix Swap fix"sv);
	}
}
