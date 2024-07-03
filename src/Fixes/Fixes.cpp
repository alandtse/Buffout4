#include "Fixes/Fixes.h"

#include "Fixes/ActorIsHostileToActorFix.h"
#include "Fixes/BSLightingShaderMaterialGlowmapFix.h"
#include "Fixes/BakaMaxPapyrusOps.h"
#include "Fixes/CellInitFix.h"
#include "Fixes/CreateD3DAndSwapChainFix.h"
#include "Fixes/EncounterZoneResetFix.h"
#include "Fixes/GreyMoviesFix.h"
#include "Fixes/InteriorNavCutFix.h"
#include "Fixes/MagicEffectApplyEventFix.h"
#include "Fixes/MovementPlannerFix.h"
#include "Fixes/PackageAllocateLocationFix.h"
#include "Fixes/PipboyLightInvFix.h"
#include "Fixes/SafeExitFix.h"
#include "Fixes/TESObjectREFRGetEncounterZoneFix.h"
#include "Fixes/UnalignedLoadFix.h"
#include "Fixes/UtilityShaderFix.h"
#include "Fixes/WorkBenchSwapFix.h"

namespace Fixes
{
	void PreLoad()
	{
		if (*Settings::ActorIsHostileToActor) {
			ActorIsHostileToActorFix::Install();
		}

		if (REL::Module::IsVR() && *Settings::BSLightingShaderMaterialGlowmap) {
			BSLightingShaderMaterialGlowmapFix::Install();
		}

		if (*Settings::CellInit) {
			CellInitFix::Install();
		}

		if (*Settings::CreateD3DAndSwapChain) {
			CreateD3DAndSwapChainFix::Install();
		}

		if (*Settings::GreyMovies) {
			GreyMoviesFix::Install();
		}

		if (*Settings::MagicEffectApplyEvent) {
			MagicEffectApplyEventFix::Install();
		}

		if (*Settings::MovementPlanner) {
			MovementPlannerFix::Install();
		}

		if (*Settings::PackageAllocateLocation) {
			PackageAllocateLocationFix::Install();
		}

		if (*Settings::SafeExit) {
			SafeExitFix::Install();
		}

		if (*Settings::TESObjectREFRGetEncounterZone) {
			TESObjectREFRGetEncounterZoneFix::Install();
		}

		if (*Settings::UnalignedLoad) {
			UnalignedLoadFix::Install();
		}

		if (REL::Module::IsVR() && *Settings::WorkBenchSwap) {
			WorkBenchSwapFix::Install();
		}

		if (REL::Module::IsVR() && *Settings::PipboyLightInvFix) {
			PipboyLightInvFix::Install();
		}

		// BakaMaxPapyrusOps
		// https://github.com/shad0wshayd3-FO4/BakaMaxPapyrusOps
		if (REX::W32::GetModuleHandleW(L"BakaMaxPapyrusOps.dll")) {
			logger::info("Detected BakaMaxPapyusOps, disabling redundant fixes/tweaks.");
		} else {
			if (*Settings::FixScriptPageAllocation) {
				BakaMaxPapyrusOpsFixes::FixScriptPageAllocation::Install();
			}

			if (*Settings::FixToggleScriptsCommand) {
				BakaMaxPapyrusOpsFixes::FixToggleScriptsCommand::Install();
			}

			if (*Settings::MaxPapyrusOpsPerFrame > 0) {
				BakaMaxPapyrusOpsTweaks::MaxPapyrusOpsPerFrame::Update(*Settings::MaxPapyrusOpsPerFrame);
				BakaMaxPapyrusOpsTweaks::MaxPapyrusOpsPerFrame::Install();
				logger::info("MaxPapyrusOpsPerFrame set to {}", *Settings::MaxPapyrusOpsPerFrame);
			}
		}
	}

	void PostInit()
	{
		if (*Settings::EncounterZoneReset) {
			EncounterZoneResetFix::Install();
		}

		if (REL::Module::IsF4() && *Settings::UtilityShader) {
			UtilityShaderFix::Install();
		}
	}

	void PostLoadGame()
	{
		if (*Settings::InteriorNavCut) {
			if (REX::W32::GetModuleHandleW(L"Interior-NavCut-Fix.dll")) {
				logger::info("Detected Interior-NavCut-Fix, disabling redundant fixes.");
			} else if (REL::Module::IsVR() && F4SE::GetF4SEVersion() < REL::Version{ 0, 6, 21, 0 } && !REX::W32::GetModuleHandleW(L"f4ee.dll")) {
				// This check is necessary to fix a bug in F4SEVR 0.6.20.0 where GameDataReady will not fire.
				logger::warn("LooksMenu VR f4ee.dll not detected for F4SEVR 0.6.20.0; Upgrade to 0.6.21.0 or please install https://github.com/peteben/F4SEPlugins/releases. LooksMenu.esp from original is optional for this fix.");
			} else {
				InteriorNavCutFix::ForceNavMeshUpdate();
			}
		}
	}

	void GameDataReady()
	{
		// VR requires F4SEVR 0.6.21 and newer to fire (or f4ee.dll VR) to fire
		if (*Settings::InteriorNavCut) {
			if (REX::W32::GetModuleHandleW(L"Interior-NavCut-Fix.dll")) {
				logger::info("Detected Interior-NavCut-Fix, disabling redundant fixes.");
			} else {
				InteriorNavCutFix::RegisterNavMeshUpdateListener();
			}
		}
	}
}
