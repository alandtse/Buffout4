#pragma once

namespace Fixes::PackageAllocateLocationFix
{
	namespace detail
	{
		struct GetPrimitive
		{
			static RE::BGSPrimitive* thunk(const RE::ExtraDataList* a_this)
			{
				return a_this ? func(a_this) : nullptr;
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};
	}

	inline void Install()
	{
		REL::Relocation<std::uintptr_t> target{ REL::ID(1248203), 0x141 };  // NG is lacking entirely
		stl::write_thunk_call<5, detail::GetPrimitive>(target.address());
		logger::info("installed PackageAllocateLocation fix"sv);
	}
}
