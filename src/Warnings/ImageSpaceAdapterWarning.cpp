#include "Warnings/ImageSpaceAdapterWarning.h"

#define WIN32_LEAN_AND_MEAN

#define NOGDICAPMASKS
#define NOVIRTUALKEYCODES
#define NOWINMESSAGES
#define NOWINSTYLES
#define NOSYSMETRICS
#define NOMENUS
#define NOICONS
#define NOKEYSTATES
#define NOSYSCOMMANDS
#define NORASTEROPS
#define NOSHOWWINDOW
#define OEMRESOURCE
#define NOATOM
#define NOCLIPBOARD
#define NOCOLOR
#define NOCTLMGR
#define NODRAWTEXT
#define NOGDI
#define NOKERNEL
#define NOUSER
#define NONLS
#define NOMB
#define NOMEMMGR
#define NOMETAFILE
#define NOMINMAX
#define NOMSG
#define NOOPENFILE
#define NOSCROLL
#define NOSERVICE
#define NOSOUND
#define NOTEXTMETRIC
#define NOWH
#define NOWINOFFSETS
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX

#include <xbyak/xbyak.h>

namespace Warnings::ImageSpaceAdapterWarning
{
	namespace
	{
		struct Patch :
			Xbyak::CodeGenerator
		{
			Patch(std::uintptr_t a_dst)
			{
				Xbyak::Label dst;

				if (!REL::Module::IsNG())
					mov(r8, r14);
				else
					mov(r8, r15);  // r15 in NG
				jmp(ptr[rip + dst]);

				L(dst);
				dq(a_dst);
			}
		};

		using func_t = RE::NiFloatInterpolator*(RE::TESFile*, std::uint32_t&, bool, float);
		REL::Relocation<func_t> _original;

		RE::NiFloatInterpolator* LoadChunk(RE::TESFile* a_file, std::uint32_t& a_size, RE::TESImageSpaceModifier* a_imad, float a_default)
		{
			if (a_size == 0) {
				auto ichunk = a_file->GetTESChunk();
				const auto cchunk = reinterpret_cast<char*>(std::addressof(ichunk));
				if (cchunk[0] < '@') {
					cchunk[0] += 'a';
				}
				const std::string_view schunk{ cchunk, 4 };
				stl::report_and_fail(
					fmt::format(
						"IMAD with ID: [0x{:08X}] from \"{}\" with subrecord \"{}\" has an invalid key size of zero. "
						"This will result in memory corruption. "
						"Please open the form in xEdit and correct it or remove the mod from your load order."sv,
						a_imad->GetFormID(),
						a_file->GetFilename(),
						schunk));
			} else {
				return _original(a_file, a_size, a_imad->data.animatable, a_default);
			}
		}
	}

	void Install()
	{
		auto& trampoline = F4SE::GetTrampoline();
		REL::Relocation<std::uintptr_t> target{ REL::RelocationID(231868, 2199987), REL::VariantOffset(0x57F, 0x573, 0x57f) };
		Patch p{ reinterpret_cast<std::uintptr_t>(&LoadChunk) };
		p.ready();
		_original = trampoline.write_call<5>(
			target.address(),
			trampoline.allocate(p));
		logger::info("installed ImageSpaceAdapter Warning"sv);
	}
}
