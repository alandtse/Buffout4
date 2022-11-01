#pragma once

namespace Fixes::PipboyLightInvFix
{
	namespace detail
	{
		struct Patch : Xbyak::CodeGenerator
		{
			explicit Patch(std::uintptr_t a_dest, std::uintptr_t a_rtn)
			{
				Xbyak::Label contLab;
				Xbyak::Label retLab;

				test(rbx, rbx);
				jz("returnFunc");
				mov(rdx, rax);
				movss(xmm2, dword[rax + 0xa0]);
				jmp(ptr[rip + contLab]);

				L("returnFunc");
				jmp(ptr[rip + retLab]);

				L(contLab);
				dq(a_dest);

				L(retLab);
				dq(a_rtn);
			}
		};

	}

	inline void Install()
	{
		REL::Relocation<std::uintptr_t> target{ REL::Offset(0xf2d240).address() + 0xD28 };
		REL::Relocation<std::uintptr_t> resume{ REL::Offset(0xf2d240).address() + 0xD33 };
		REL::Relocation<std::uintptr_t> returnAddr{ REL::Offset(0xf2d240).address() + 0xE16 };

		for (std::size_t i = 0; i < 11; i++) {
			REL::safe_write(target.address() + i, std::uint32_t{ 0x90 });
		}

		detail::Patch p{ resume.address(), returnAddr.address() };
		p.ready();

		auto& trampoline = F4SE::GetTrampoline();
		trampoline.write_branch<5>(
			target.address(),
			trampoline.allocate(p));

		logger::info("installed PipboyLightInvFix Swap fix"sv);
	}
}
