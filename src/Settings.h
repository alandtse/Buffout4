#pragma once

namespace Settings
{
	template <class T>
	class Setting
	{
	public:
		using value_type = T;

		Setting(
			std::string_view a_group,
			std::string_view a_key,
			value_type a_default) noexcept :
			_group(a_group),
			_key(a_key),
			_value(a_default)
		{}

		[[nodiscard]] auto group() const noexcept -> std::string_view { return this->_group; }
		[[nodiscard]] auto key() const noexcept -> std::string_view { return this->_key; }

		template <class Self>
		[[nodiscard]] auto&& get(this Self&& a_self) noexcept
		{
			return std::forward<Self>(a_self)._value;
		}

		template <class Self>
		[[nodiscard]] auto&& operator*(this Self&& a_self) noexcept
		{
			return std::forward<Self>(a_self).get();
		}

	private:
		std::string_view _group;
		std::string_view _key;
		value_type _value;
	};

	using bSetting = Setting<bool>;
	using iSetting = Setting<std::int64_t>;
	using sSetting = Setting<std::string>;

#define MAKE_SETTING(a_type, a_group, a_key, a_default) \
	inline auto a_key = a_type(a_group##sv, #a_key##sv, a_default)

	MAKE_SETTING(bSetting, "Fixes", ActorIsHostileToActor, true);
	MAKE_SETTING(bSetting, "Fixes", BackportedBA2Support, true);
	MAKE_SETTING(bSetting, "Fixes", BGSAIWorldLocationRefRadiusNull, true);
	MAKE_SETTING(bSetting, "Fixes", BSLightingShaderMaterialGlowmap, true);
	MAKE_SETTING(bSetting, "Fixes", CellInit, true);
	MAKE_SETTING(bSetting, "Fixes", CreateD3DAndSwapChain, true);
	MAKE_SETTING(bSetting, "Fixes", EncounterZoneReset, true);
	MAKE_SETTING(bSetting, "Fixes", EscapeFreeze, true);
	MAKE_SETTING(bSetting, "Fixes", FixScriptPageAllocation, true);
	MAKE_SETTING(bSetting, "Fixes", FixToggleScriptsCommand, true);
	MAKE_SETTING(bSetting, "Fixes", FollowerStrayBullet, true);
	MAKE_SETTING(bSetting, "Fixes", GreyMovies, true);
	MAKE_SETTING(bSetting, "Fixes", InteriorNavCut, true);
	MAKE_SETTING(bSetting, "Fixes", MagicEffectApplyEvent, true);
	MAKE_SETTING(bSetting, "Fixes", MovementPlanner, true);
	MAKE_SETTING(bSetting, "Fixes", PackageAllocateLocation, true);
	MAKE_SETTING(bSetting, "Fixes", SafeExit, true);
	MAKE_SETTING(bSetting, "Fixes", TESObjectREFRGetEncounterZone, true);
	MAKE_SETTING(bSetting, "Fixes", UnalignedLoad, true);
	MAKE_SETTING(bSetting, "Fixes", UtilityShader, true);
	MAKE_SETTING(bSetting, "Fixes", WorkBenchSwap, true);
	MAKE_SETTING(bSetting, "Fixes", PipboyLightInvFix, false);

	MAKE_SETTING(bSetting, "Patches", Achievements, true);
	MAKE_SETTING(bSetting, "Patches", BSMTAManager, true);
	MAKE_SETTING(bSetting, "Patches", BSPreCulledObjects, true);
	MAKE_SETTING(bSetting, "Patches", BSTextureStreamerLocalHeap, true);
	MAKE_SETTING(bSetting, "Patches", HavokMemorySystem, true);
	MAKE_SETTING(bSetting, "Patches", INISettingCollection, true);
	MAKE_SETTING(bSetting, "Patches", InputSwitch, false);
	MAKE_SETTING(iSetting, "Patches", MaxStdIO, -1);
	MAKE_SETTING(bSetting, "Patches", MemoryManager, true);
	MAKE_SETTING(bSetting, "Patches", MemoryManagerDebug, false);
	MAKE_SETTING(bSetting, "Patches", ScaleformAllocator, true);
	MAKE_SETTING(bSetting, "Patches", SmallBlockAllocator, true);
	MAKE_SETTING(bSetting, "Patches", WorkshopMenu, true);

	MAKE_SETTING(iSetting, "Tweaks", MaxPapyrusOpsPerFrame, 100);

	MAKE_SETTING(bSetting, "Warnings", CreateTexture2D, true);
	MAKE_SETTING(bSetting, "Warnings", ImageSpaceAdapter, true);

	MAKE_SETTING(bSetting, "Compatibility", F4EE, false);

	MAKE_SETTING(sSetting, "Debug", Symcache, "c:\symcache");
	MAKE_SETTING(bSetting, "Debug", WaitForDebugger, false);

#undef MAKE_SETTING

	inline std::vector<
		std::variant<
			std::reference_wrapper<bSetting>,
			std::reference_wrapper<iSetting>,
			std::reference_wrapper<sSetting>>>
		settings;

	inline void load()
	{
		const auto config = toml::parse_file("Data/F4SE/Plugins/Buffout4.toml"sv);

#define LOAD(a_setting)                                                              \
	settings.push_back(std::ref(a_setting));                                         \
	if (const auto tweak = config[a_setting.group()][a_setting.key()]; tweak) {      \
		if (const auto value = tweak.as<decltype(a_setting)::value_type>(); value) { \
			*a_setting = value->get();                                               \
		} else {                                                                     \
			throw std::runtime_error(                                                \
				fmt::format(                                                         \
					"setting '{}.{}' is not of the correct type: expected '{}'"sv,   \
					a_setting.group(),                                               \
					a_setting.key(),                                                 \
					typeid(decltype(a_setting)::value_type).name()));                \
		}                                                                            \
	}

		LOAD(ActorIsHostileToActor);
		LOAD(BackportedBA2Support);
		LOAD(BGSAIWorldLocationRefRadiusNull)
		LOAD(BSLightingShaderMaterialGlowmap);
		LOAD(CellInit);
		LOAD(CreateD3DAndSwapChain);
		LOAD(EncounterZoneReset);
		LOAD(EscapeFreeze);
		LOAD(FixScriptPageAllocation);
		LOAD(FixToggleScriptsCommand);

		LOAD(FollowerStrayBullet);
		LOAD(GreyMovies);
		LOAD(InteriorNavCut);
		LOAD(MagicEffectApplyEvent);
		LOAD(MovementPlanner);
		LOAD(PackageAllocateLocation);
		LOAD(SafeExit);
		LOAD(TESObjectREFRGetEncounterZone);
		LOAD(UnalignedLoad);
		LOAD(UtilityShader);
		LOAD(WorkBenchSwap);
		LOAD(PipboyLightInvFix);

		LOAD(Achievements);
		LOAD(BSMTAManager);
		LOAD(BSPreCulledObjects);
		LOAD(BSTextureStreamerLocalHeap);
		LOAD(HavokMemorySystem);
		LOAD(INISettingCollection);
		LOAD(InputSwitch);
		LOAD(MaxStdIO);
		LOAD(MemoryManager);
		LOAD(MemoryManagerDebug);
		LOAD(ScaleformAllocator);
		LOAD(SmallBlockAllocator);
		LOAD(WorkshopMenu);

		LOAD(MaxPapyrusOpsPerFrame);

		LOAD(CreateTexture2D);
		LOAD(ImageSpaceAdapter);

		LOAD(F4EE);

		LOAD(Symcache);
		LOAD(WaitForDebugger);

		std::sort(
			settings.begin(),
			settings.end(),
			[](auto&& a_lhs, auto&& a_rhs) {
				const auto get = [](auto&& a_val) {
					return std::make_pair(
						a_val.get().group(),
						a_val.get().key());
				};

				const auto [lgroup, lkey] = std::visit(get, a_lhs);
				const auto [rgroup, rkey] = std::visit(get, a_rhs);
				if (lgroup != rgroup) {
					return lgroup < rgroup;
				} else {
					return lkey < rkey;
				}
			});
	}
}
