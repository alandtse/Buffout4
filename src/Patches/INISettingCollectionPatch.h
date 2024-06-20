#pragma once

namespace Patches::INISettingCollectionPatch
{
	namespace detail
	{
		struct Open
		{
			static bool thunk(RE::INISettingCollection& a_self, bool a_write)
			{
				return std::filesystem::exists(a_self.settingFile) ?
				           func(a_self, a_write) :
				           false;
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};
	}

	inline void Install()
	{
		REL::Relocation<std::uintptr_t> vtable{ RE::INISettingCollection::VTABLE[0] };
		detail::Open::func = vtable.write_vfunc(0x5, detail::Open::thunk);

		logger::info("installed INISettingCollection patch"sv);
	}
}
