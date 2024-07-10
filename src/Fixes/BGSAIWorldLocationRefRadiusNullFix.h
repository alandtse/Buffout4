#pragma once
#include <xbyak/xbyak.h>

namespace Fixes::BGSAIWorldLocationRefRadiusNullFix
{
	namespace detail
	{
		struct Patch : Xbyak::CodeGenerator
		{
			explicit Patch(std::uintptr_t a_dest, std::uintptr_t a_rtn)
			{
				Xbyak::Label contLab;
				Xbyak::Label retLab;

				// code clobbered at target is placed here
				movss(qword[rbx + 0x10], REL::Relocate(xmm7, xmm0));
				// end clobbered code
				test(rsi, rsi);    // nullptr check on rsi
				jz("returnFunc");  // jump to returnFunc if rsi is null
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
		const auto base = REL::RelocationID(964254, 2188379).address();

		REL::Relocation<std::uintptr_t> target{ base + REL::Relocate(0x52, 0x4e) };
		REL::Relocation<std::uintptr_t> resume{ target.address() + 0x5 };
		REL::Relocation<std::uintptr_t> returnAddr{ base + REL::Relocate(0x104, 0xf8) };

		const auto instructionBytes = resume.address() - target.address();
		for (std::size_t i = 0; i < instructionBytes; i++) {
			REL::safe_write(target.address() + i, REL::NOP);
		}
		logger::debug("clearing {:x} bytes"sv, instructionBytes);
		detail::Patch p{ resume.address(), returnAddr.address() };

		p.ready();

		auto& trampoline = F4SE::GetTrampoline();
		trampoline.write_branch<5>(
			target.address(),
			trampoline.allocate(p));

		logger::info("installed BGSAIWorldLocationRefRadiusNull fix: {0:x}"sv, target.address());
	}
}
