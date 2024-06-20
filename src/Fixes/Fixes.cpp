#include "Fixes/Fixes.h"

#include "Fixes/ActorIsHostileToActorFix.h"
#include "Fixes/BSLightingShaderMaterialGlowmapFix.h"
#include "Fixes/CellInitFix.h"
#include "Fixes/CreateD3DAndSwapChainFix.h"
#include "Fixes/EncounterZoneResetFix.h"
#include "Fixes/GreyMoviesFix.h"
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
}
