#pragma once

namespace Fixes::UnalignedLoadFix
{
	namespace detail
	{
		inline void ApplySkinningToGeometry()
		{
#ifndef FALLOUTVR
			const auto offset = 0x172;
#else
			const auto offset = 0x179;
#endif
			REL::Relocation<std::uintptr_t> target{ REL::ID(44611), offset + 0x2 };
			REL::safe_write(target.address(), std::uint32_t{ 0x10 });
		}

		inline void CreateCommandBuffer()
		{
			constexpr std::array offsets{
				0x320,
				0x339,
				0x341 + 0x1,  // rex prefix
				0x353,
			};

			REL::Relocation<std::uintptr_t> base{ REL::ID(768994) };
			for (const auto off : offsets) {
				REL::safe_write(base.address() + off + 0x1, std::uint8_t{ 0x11 });  // movaps -> movups
			}
		}
	}

	inline void Install()
	{
		detail::ApplySkinningToGeometry();
#ifndef FALLOUTVR // function doesnt exist in VR>   need to RE more
		detail::CreateCommandBuffer();
#endif
		logger::info("installed UnalignedLoad fix"sv);
	}
}
