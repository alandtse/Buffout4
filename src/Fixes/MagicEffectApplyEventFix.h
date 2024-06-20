#pragma once

namespace Fixes::MagicEffectApplyEventFix
{
	namespace detail
	{
		struct ProcessEvent
		{
			static RE::BSEventNotifyControl thunk(
				RE::GameScript::CombatEventHandler& a_self,
				const RE::TESMagicEffectApplyEvent& a_event,
				RE::BSTEventSource<RE::TESMagicEffectApplyEvent>* a_source)
			{
				return a_event.target ?
				           func(a_self, a_event, a_source) :
				           RE::BSEventNotifyControl::kContinue;
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};
	}

	inline void Install()
	{
		REL::Relocation<std::uintptr_t> target{ REL::ID(RE::GameScript::CombatEventHandler::VTABLE[1]) };
		detail::ProcessEvent::func = target.write_vfunc(0x1, detail::ProcessEvent::thunk);
		logger::info("installed Magic Effect Apply Event fix"sv);
	}
}
