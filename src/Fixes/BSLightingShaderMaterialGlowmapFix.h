#pragma once
#include <xbyak/xbyak.h>

namespace Fixes::BSLightingShaderMaterialGlowmapFix
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
				mov(qword[rsp + 0x60], rbx);
				// end clobbered code
				test(rbx, rbx);    // nullptr check on rbx
				jz("returnFunc");  // jump to returnFunc if rbx is null
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
		const auto base = REL::Offset(0x1d97d84).address();
		REL::Relocation<std::uintptr_t> target{ base };
		REL::Relocation<std::uintptr_t> resume{ base + 0x5 };
		REL::Relocation<std::uintptr_t> returnAddr{ base + 0xBD5 };  // 141d98959
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

		logger::info("installed BSLightingShaderMaterialGlowmapFix: {:x}"sv, target.address());
	}
}
