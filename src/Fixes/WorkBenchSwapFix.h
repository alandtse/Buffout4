#pragma once

#include <xbyak/xbyak.h>

namespace Fixes::WorkBenchSwapFix
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
		REL::Relocation<std::uintptr_t> target{ REL::RelocationID(1573164, 2267897), 0x48 };  // identical in all versions
		REL::Relocation<std::uintptr_t> resume{ REL::RelocationID(1573164, 2267897), 0x4D };

		detail::Patch p{ resume.address() };
		p.ready();

		auto& trampoline = F4SE::GetTrampoline();
		trampoline.write_branch<5>(
			target.address(),
			trampoline.allocate(p));

		logger::info("installed WorkBench Swap fix"sv);
	}
}
