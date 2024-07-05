#pragma once

namespace Fixes::ActorIsHostileToActorFix
{
	namespace detail
	{
		inline bool IsHostileToActor(RE::BSScript::IVirtualMachine* a_vm, std::uint32_t a_stackID, RE::Actor* a_self, RE::Actor* a_actor)
		{
			if (!a_actor) {
				RE::GameScript::LogFormError(
					a_actor,
					"Cannot call IsHostileToActor with a None actor",
					a_vm,
					a_stackID);
				return false;
			} else {
				return a_self->GetHostileToActor(a_actor);
			}
		}
	}

	inline void Install()
	{
		constexpr std::size_t size = 0x10;  // minimum function size
		REL::Relocation<std::uintptr_t> target{ REL::ID(1022223) };
		REL::Relocation<std::uintptr_t> targetNG{ REL::Offset(0x1081b20) };

		const auto address = (REL::Module::IsNG() ? targetNG : target).address();
		REL::safe_fill(address, REL::INT3, size);
		stl::asm_jump(address, size, reinterpret_cast<std::uintptr_t>(detail::IsHostileToActor));

		logger::info("installed ActorIsHostileToActor fix"sv);
	}
}
