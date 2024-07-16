#pragma once
#include <xbyak/xbyak.h>

namespace Fixes::PipboyLightInvFix
{
	namespace detail
	{
		struct Patch : Xbyak::CodeGenerator
		{
			explicit Patch(std::uintptr_t a_dest, std::uintptr_t a_rtn, std::uintptr_t a_rbx_offset)
			{
				Xbyak::Label contLab;
				Xbyak::Label retLab;

				test(rbx, rbx);
				jz("returnFunc");
				mov(rcx, dword[rbx + a_rbx_offset]);
				test(rcx, rcx);
				jz("returnFunc");
				test(rax, rax);
				jz("returnFunc");
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
		const auto base = REL::RelocationID(566261, 2233255).address();

		REL::Relocation<std::uintptr_t> target{ base + REL::Relocate(0xcb2, 0xc92, 0xD21) };
		REL::Relocation<std::uintptr_t> resume{ target.address() + 0x7 };
		REL::Relocation<std::uintptr_t> returnAddr{ base + REL::Relocate(0xda7, 0xd87, 0xE16) };

		const auto instructionBytes = resume.address() - target.address();
		for (std::size_t i = 0; i < instructionBytes; i++) {
			REL::safe_write(target.address() + i, REL::NOP);
		}

		detail::Patch p{ resume.address(), returnAddr.address(), REL::Relocate<std::uintptr_t>(0xc40, 0xc40, 0x10b0) };
		p.ready();

		auto& trampoline = F4SE::GetTrampoline();
		trampoline.write_branch<5>(
			target.address(),
			trampoline.allocate(p));

		logger::info("installed PipboyLightInvFix Swap fix: {0:x}"sv, target.address());
	}
}
