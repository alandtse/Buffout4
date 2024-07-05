#pragma once

namespace Fixes::UnalignedLoadFix
{
	namespace detail
	{
		inline void ApplySkinningToGeometry()
		{
			REL::Relocation<std::uintptr_t> target{ REL::RelocationID(44611, 2277131), REL::Relocate(0x172, 0x190, 0x179) };
			REL::safe_write(target.address() + 0x2, std::uint32_t{ 0x10 });
			logger::debug("ApplySkinningToGeometry: writing to {:x}"sv, target.address() + 0x2);
		}

		inline void CreateCommandBuffer()
		{
			constexpr std::array offsets{
				0x320,
				0x339,
				0x341 + 0x1,  // rex prefix
				0x353,
			};

			REL::Relocation<std::uintptr_t> base{ REL::RelocationID(768994, 2319078) };
			// NG already patched to movups
			for (const auto off : offsets) {
				REL::safe_write(base.address() + off + 0x1, std::uint8_t{ 0x11 });  // movaps -> movups
			}
		}
	}

	inline void Install()
	{
		detail::ApplySkinningToGeometry();
		if (REL::Module::IsF4()) {  // function doesnt exist in VR>   need to RE more
			// NG already has it patched
			detail::CreateCommandBuffer();
		}
		logger::info("installed UnalignedLoad fix"sv);
	}
}
