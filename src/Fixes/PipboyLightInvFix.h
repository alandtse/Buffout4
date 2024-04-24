#pragma once
#include <xbyak/xbyak.h>

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
				mov(rcx, dword[rbx + 0x10b0]);
				mov(rdx, rax);
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
		const auto base = REL::Offset(0xf2d240).address();
		REL::Relocation<std::uintptr_t> target{ base + 0xD21 };
		REL::Relocation<std::uintptr_t> resume{ base + 0xD2b };
		REL::Relocation<std::uintptr_t> returnAddr{ base + 0xE4A };

		const auto instructionBytes = resume.address() - target.address();
		for (std::size_t i = 0; i < instructionBytes; i++) {
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
