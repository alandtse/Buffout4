#include "Warnings/CreateTexture2DWarning.h"

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
//#define NOUSER
#define NONLS
#define NOMB
#define NOMEMMGR
#define NOMETAFILE
#define NOMINMAX
//#define NOMSG
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

#include <d3d11.h>
#include <xbyak/xbyak.h>

namespace Warnings::CreateTexture2DWarning
{
	namespace
	{
		::HRESULT CreateTexture2D(
			::ID3D11Device* a_this,
			const ::D3D11_TEXTURE2D_DESC* a_desc,
			const ::D3D11_SUBRESOURCE_DATA* a_initialData,
			::ID3D11Texture2D** a_texture2D)
		{
			const auto result = a_this->CreateTexture2D(a_desc, a_initialData, a_texture2D);
			if (result != S_OK) {
				stl::report_and_fail(
					fmt::format(
						"A call to ID3D11Device::CreateTexture2D failed with error code 0x{:08X}. "
						"This will crash the game.",
						static_cast<std::uint32_t>(result)));
			} else {
				return result;
			}
		}

		template <class T>
		void WritePatch(std::uintptr_t a_dst, std::size_t a_size)
		{
			REL::safe_fill(a_dst, REL::NOP, a_size);
			T p{ reinterpret_cast<std::uintptr_t>(&CreateTexture2D) };
			p.ready();
			auto& trampoline = F4SE::GetTrampoline();
			assert(a_size >= 6);
			trampoline.write_call<6>(
				a_dst,
				trampoline.allocate(p));
		}
	}

	void Install()
	{
		{  // BSD3DResourceCreator::ThreadProc; case 3
			struct Patch :
				Xbyak::CodeGenerator
			{
				Patch(std::uintptr_t a_dst)
				{
					lea(r9, ptr[rbp + 0x8]);
					mov(rax, a_dst);
					jmp(rax);
				}
			};

			REL::Relocation<std::uintptr_t> target{ REL::ID(678241), 0x147 };  // Missing in VR, instead calls CreateTextureRequested
			if (REL::Module::IsF4())
				WritePatch<Patch>(target.address(), 0x7);
		}

		{  // BSD3DResourceCreator::CreateTextureRequested
			struct Patch :
				Xbyak::CodeGenerator
			{
				Patch(std::uintptr_t a_dst)
				{
					lea(r9, ptr[rdi + 0x8]);
					mov(rax, a_dst);
					jmp(rax);
				}
			};
			REL::Relocation<std::uintptr_t> target{ REL::ID(1165600), REL::Relocate(0x4c, 0x5f) };
			WritePatch<Patch>(target.address(), 0x7);
		}

		{  // BSD3DResourceCreator::CreateDeferredTextureCopyRequested
			struct Patch :
				Xbyak::CodeGenerator
			{
				Patch(std::uintptr_t a_dst)
				{
					lea(rdx, ptr[rsp + (0x78 - 0x40) + 0x8]);
					mov(rax, a_dst);
					jmp(rax);
				}
			};

			REL::Relocation<std::uintptr_t> target{ REL::ID(367479), REL::Relocate(0xAA, 0xbf) };
			WritePatch<Patch>(target.address(), 0x8);
		}

		logger::info("installed CreateTexture2D Warning"sv);
	}
}
