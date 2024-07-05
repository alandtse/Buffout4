#pragma once

namespace Fixes::MovementPlannerFix
{
	namespace detail
	{
		struct CanWarpOnPathFailure
		{
			static bool thunk(const RE::Actor* a_actor)
			{
				return a_actor ? func(a_actor) : true;
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};
	}

	inline void Install()
	{
		REL::Relocation<std::uintptr_t> target{ REL::RelocationID(1403049, 2234683), 0x30 };
		stl::write_thunk_call<5, detail::CanWarpOnPathFailure>(target.address());
		logger::info("installed MovementPlanner fix"sv);
	}
}
