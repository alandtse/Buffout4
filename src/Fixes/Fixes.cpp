#include "Fixes/Fixes.h"

#include "Fixes/ActorIsHostileToActorFix.h"
#include "Fixes/CellInitFix.h"
#include "Fixes/CreateD3DAndSwapChainFix.h"
#include "Fixes/EncounterZoneResetFix.h"
#include "Fixes/GreyMoviesFix.h"
#include "Fixes/MagicEffectApplyEventFix.h"
#include "Fixes/MovementPlannerFix.h"
#include "Fixes/PackageAllocateLocationFix.h"
#include "Fixes/SafeExitFix.h"
#include "Fixes/TESObjectREFRGetEncounterZoneFix.h"
#include "Fixes/UnalignedLoadFix.h"
#include "Fixes/UtilityShaderFix.h"
#include "Fixes/WorkBenchSwapFix.h"
#include "Fixes/PipboyLightInvFix.h"

namespace Fixes
{
	void PreLoad()
	{
		if (*Settings::ActorIsHostileToActor) {
			ActorIsHostileToActorFix::Install();
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

#ifdef FALLOUTVR
		if (*Settings::WorkBenchSwap) {
			WorkBenchSwapFix::Install();
		}	
		
		if (*Settings::PipboyLightInvFix) {
			PipboyLightInvFix::Install();
		}

#endif
	}

	void PostInit()
	{
		if (*Settings::EncounterZoneReset) {
			EncounterZoneResetFix::Install();
		}

#ifndef FALLOUTVR
		if (*Settings::UtilityShader) {
			UtilityShaderFix::Install();
		}

#endif
	}
}
