#pragma once

namespace TESObjectREFRGetEncounterZoneFix
{
	namespace detail
	{
		template <class T>
		struct GetEncounterZone
		{
			static T* thunk(const RE::BSTSmartPointer<RE::ExtraDataList>& a_in)
			{
				const auto& ref = *stl::adjust_pointer<RE::TESObjectREFR>(
					&a_in,
					-static_cast<std::ptrdiff_t>(offsetof(RE::TESObjectREFR, RE::TESObjectREFR::extraList)));
				auto ptr = ref.extraList ? func(*ref.extraList) : nullptr;

				const auto addr = reinterpret_cast<std::uintptr_t>(ptr);
				if (!ref.IsInitialized() &&
					((addr & 0xFFFF'FFFF'0000'0000) == 0) &&
					((addr & 0x0000'0000'FFFF'FFFF) != 0)) {
					auto id = static_cast<std::uint32_t>(addr);
					RE::TESForm::AddCompileIndex(id, ref.GetFile());
					ptr = RE::TESForm::GetFormByID<T>(id);
				}

				return ptr;
			}

			static inline REL::Relocation<T*(const RE::ExtraDataList&)> func;
		};
	}

	inline void Install()
	{
		const auto root = REL::RelocationID(1413642, 2202627).address();

		REL::safe_write(root + 0xD + 0x1, static_cast<std::uint8_t>(0x8D));  // mov -> lea
		stl::write_thunk_call<5, detail::GetEncounterZone<RE::BGSEncounterZone>>(root + 0x14);

		REL::safe_write(root + 0x91 + 0x1, static_cast<std::uint8_t>(0x8D));  // mov -> lea
		stl::write_thunk_call<5, detail::GetEncounterZone<RE::BGSLocation>>(root + 0x98);

		logger::info("installed TESObjectREFRGetEncounterZone fix"sv);
	}
}
