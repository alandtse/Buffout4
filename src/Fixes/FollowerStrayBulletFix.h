// based on MIT code from LarannKiar https://www.nexusmods.com/fallout4/mods/81122

namespace Fixes::FollowerStrayBulletFix
{
	inline static std::chrono::steady_clock::time_point iLastAllowedPatchTime;

	inline static void SaveLastCombatExitPatchTime(bool bSuppressLog = false)
	{
		auto now = std::chrono::high_resolution_clock::now();
		auto nowLog = std::chrono::system_clock::now();
		iLastAllowedPatchTime = now;
		if (!bSuppressLog) {
			std::stringstream ss;
			ss << nowLog;
			logger::info("SaveLastCombatExitPatchTime: saved time [{}].", ss.str());
		}
	}

	inline static long GetAllowedCombatExitPatchTime()
	{
		auto now = std::chrono::high_resolution_clock::now();
		std::chrono::duration<long, std::milli> elapsedTime = duration_cast<std::chrono::milliseconds>(now - iLastAllowedPatchTime);
		auto elapsed = elapsedTime.count();
		return elapsed;
	}

	inline std::string IntToHexLocal(uint32_t param)
	{
		if (param < 0 || param > UINT32_MAX)
			return "";
		//std::string hex(uint32_t param);
		return std::format("0x{:x}", param);
	}

	int IsPlayerTeammate(RE::Actor* akActor)
	{
		if (!akActor)
			return -1;
		if ((akActor->niFlags.flags >> 0x1a & 1) != 0)
			return 1;
		return 0;
	}

	std::vector<RE::Actor*> GetPlayerTeammates()
	{
		std::vector<RE::Actor*> Teammates;
		const auto ProcessLists = RE::ProcessLists::GetSingleton();
		if (ProcessLists) {
			auto HighActors = ProcessLists->highActorHandles;
			if (HighActors.size() > 0) {
				for (uint32_t i = 0; i < HighActors.size(); i++) {
					auto LoopActor = HighActors.at(i).get().get();
					if (LoopActor && IsPlayerTeammate(LoopActor) == 1)
						Teammates.push_back(LoopActor);
				}
			}
		}
		if (Teammates.size() > 0) {
			std::sort(Teammates.begin(), Teammates.end());
			Teammates.erase(std::unique(Teammates.begin(), Teammates.end()), Teammates.end());
		}
		return Teammates;
	}

	RE::Actor* GetCurrentCrosshairActor()
	{
		REL::Relocation<uint32_t> handle{ REL::ID(568440), -0xE00 };  // in BSTValueEventSource<PlayerActivatePickRefEvent>
		if (handle.address()) {
			if (auto handleIDptr = reinterpret_cast<uint32_t*>(handle.address())) {
				uint32_t handleID = *handleIDptr;
				if (handleID > 0 || handleID < UINT32_MAX) {
					RE::TESObjectREFR* emptyRef = nullptr;
					RE::NiPointer<RE::TESObjectREFR> refPtr = RE::NiPointer<RE::TESObjectREFR>(emptyRef);
					RE::LookupReferenceByHandle(handleID, refPtr);
					if (refPtr) {
						RE::TESObjectREFR* foundRef = refPtr.get();
						if (foundRef) {
							auto formID = foundRef->formID;
							if (formID && formID > 0 && formID <= UINT32_MAX) {
								RE::TESObjectREFR* crosshairRef = reinterpret_cast<RE::TESObjectREFR*>(foundRef);
								if (crosshairRef && crosshairRef->GetSavedFormType() == RE::ENUM_FORM_ID::kACHR) {
									RE::Actor* crosshairActor = (RE::Actor*)crosshairRef;
									if (crosshairActor) {
										//logger::info("GetCurrentCrosshairActor: found = {}", IntToHexLocal(crosshairActor->formID));
										return crosshairActor;
									}
								}
							}
						}
					}
				}
			}
		}
		return nullptr;
	}

	struct HandleOnCommandEnter
	{
		static void HandleOnCommandEnterFunc(RE::Actor* akActor, char abCommandState, BYTE unk);
		static inline REL::Relocation<decltype(HandleOnCommandEnterFunc)> func;
	};

	void HandleOnCommandEnter::HandleOnCommandEnterFunc(RE::Actor* akPlayer, char abCommandState, BYTE unk)
	{
		RE::Actor* CrosshairActor = GetCurrentCrosshairActor();
		std::string sLog = "";
		auto exit = false;
		if (!akPlayer) {
			logger::debug("HandleOnCommandEnter: akPlayer is nullptr.");
			exit = true;
		}
		if (!CrosshairActor) {
			logger::debug("HandleOnCommandEnter: CrosshairActor is nullptr.");
			exit = true;
		}
		if (CrosshairActor && CrosshairActor->weaponState == RE::WEAPON_STATE::kSheathed) {
			logger::debug("HandleOnCommandEnter: CrosshairActor.WeaponState == Sheathed.");
			exit = true;
		}
		if (CrosshairActor && CrosshairActor->gunState == RE::GUN_STATE::kRelaxed) {
			logger::debug("HandleOnCommandEnter: CrosshairActor.GunState == Relaxed.");
			exit = true;
		}
		if (CrosshairActor && CrosshairActor->DoGetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal) {
			logger::debug("HandleOnCommandEnter: CrosshairActor SitSleepState != 0.");
			exit = true;
		}
		if (akPlayer && !akPlayer->IsInCombat()) {
			sLog = "akPlayer [" + IntToHexLocal(akPlayer->formID) + "] IsInCombat == false.";
			logger::debug("HandleOnCommandEnter: {}", sLog);
			exit = true;
		}

		if (!exit) {
			sLog = "CrosshairActor [" + IntToHexLocal(CrosshairActor->formID) + "]. Calling HandleItemEquip...";
			logger::info("HandleOnCommandEnter: {}", sLog);
			CrosshairActor->HandleItemEquip(true);
		}

		{
			func(akPlayer, abCommandState, unk);
		}
	}

	struct HandleOnCombatExit
	{
		static BYTE HandleOnCombatExitFunc(uintptr_t unk1, uintptr_t unk2, uint32_t unk3, uint32_t unk4, BYTE unk5, uint32_t unk6);
		static inline REL::Relocation<decltype(HandleOnCombatExitFunc)> func;
	};

	BYTE HandleOnCombatExit::HandleOnCombatExitFunc(uintptr_t unk1, uintptr_t unk2, uint32_t unk3, uint32_t unk4, BYTE unk5, uint32_t unk6)
	{
		if (unk4 != 0x46)
			return func(unk1, unk2, unk3, unk4, unk5, unk6);
		//logger::info("HandleOnCombatExit: called. Subtype = {}", IntToHexLocal(unk4));
		if (GetAllowedCombatExitPatchTime() > 300) {
			SaveLastCombatExitPatchTime();
			auto PlayerTeammates = GetPlayerTeammates();
			if (PlayerTeammates.size() > 0) {
				int32_t TeammateCount = 0;
				for (uint32_t i = 0; i < PlayerTeammates.size(); i++) {
					auto LoopTeammateActor = PlayerTeammates.at(i);
					if (!LoopTeammateActor)
						continue;
					LoopTeammateActor->HandleItemEquip(true);
					TeammateCount = TeammateCount + 1;
				}
				std::string sLog = "HandleItemEquip was called for [" + std::to_string(TeammateCount) + "] teammates";
				logger::info("HandleOnCombatExit: {}", sLog);
			}
		}
		return func(unk1, unk2, unk3, unk4, unk5, unk6);
	}

	void Install()
	{
		FollowerStrayBulletFix::SaveLastCombatExitPatchTime();
		auto& Trampoline = F4SE::GetTrampoline();

		const REL::Relocation<std::uintptr_t> EnterCommandState{ REL::ID(1512511), 0x118 };  // Fallout4.exe + 0xD8A3B0 (v1.10.163) + 0x118  ==>   CALL Fallout4.exe + 0xD8A480
		FollowerStrayBulletFix::HandleOnCommandEnter::func = Trampoline.write_call<5>(EnterCommandState.address(), Fixes::FollowerStrayBulletFix::HandleOnCommandEnter::HandleOnCommandEnterFunc);
		logger::info("HandleMessagingInterface: patched HandleOnCommandEnter.");

		const REL::Relocation<std::uintptr_t> OnCombatExit{ REL::ID(369646), 0x1B8 };  // Fallout4.exe + 0x89E7D0 (v1.10.163) - 0x1D8  ==>   CALL Fallout4.exe + 0x1022D00
		FollowerStrayBulletFix::HandleOnCombatExit::func = Trampoline.write_call<5>(OnCombatExit.address(), Fixes::FollowerStrayBulletFix::HandleOnCombatExit::HandleOnCombatExitFunc);
		logger::info("HandleMessagingInterface: patched HandleOnCombatExit.");
	}

}
