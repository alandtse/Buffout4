#pragma once

namespace Fixes::GreyMoviesFix
{
	namespace detail
	{
		inline void SetBackgroundAlpha(RE::Scaleform::GFx::Movie& a_self, float)
		{
			RE::Scaleform::GFx::Value alpha;
			if (!a_self.GetVariable(std::addressof(alpha), "BackgroundAlpha")) {
				alpha = 0.0;
			}

			a_self.SetBackgroundAlpha(static_cast<float>(alpha.GetNumber()));
		}
	}

	inline void Install()
	{
		const REL::Relocation<std::uintptr_t> target{ REL::RelocationID(1526234, 2287422), REL::VariantOffset(0x216, 0x2a7, 0x216) };
		// NG has a different 2 byte call at this location so patch must be rewritten
		// perhaps the original call at +0x26e can be replaced instead
		// Scaleform::GFx::Movie::GetVariable((Movie *)this_00,(Value *)&local_1e8,"BackgroundAlpha");
		auto& trampoline = F4SE::GetTrampoline();
		trampoline.write_call<6>(target.address(), detail::SetBackgroundAlpha);
		logger::info("installed GreyMovies fix"sv);
	}
}
