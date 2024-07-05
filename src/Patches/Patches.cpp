#include "Patches/Patches.h"

#include "Patches/AchievementsPatch.h"
#include "Patches/BSMTAManagerPatch.h"
#include "Patches/BSPreCulledObjectsPatch.h"
#include "Patches/BSTextureStreamerLocalHeapPatch.h"
#include "Patches/HavokMemorySystemPatch.h"
#include "Patches/INISettingCollectionPatch.h"
#include "Patches/InputSwitchPatch.h"
#include "Patches/MaxStdIOPatch.h"
#include "Patches/MemoryManagerPatch.h"
#include "Patches/ScaleformAllocatorPatch.h"
#include "Patches/SmallBlockAllocatorPatch.h"
#include "Patches/WorkshopMenuPatch.h"

namespace Patches
{
	void PreLoad()
	{
		if (*Settings::Achievements) {
			AchievementsPatch::Install();
		}

		// BSMTAManager::RegisterObjectTask::Execute(RegisterObjectTask *this) inlined in vr/ng
		if (REL::Module::IsF4() && *Settings::BSMTAManager) {
			BSMTAManagerPatch::Install();
		}

		if (REL::Module::IsF4() && *Settings::BSPreCulledObjects) {
			BSPreCulledObjectsPatch::Install();
		}
		if (!REL::Module::IsNG() && *Settings::BSTextureStreamerLocalHeap) {
			BSTextureStreamerLocalHeapPatch::Install();
		}

		if (!REL::Module::IsNG() && *Settings::HavokMemorySystem) {  // ctd in NG, more re needed
			HavokMemorySystemPatch::Install();
		}

		if (*Settings::INISettingCollection) {
			INISettingCollectionPatch::Install();
		}

		if (REL::Module::IsF4() && *Settings::InputSwitch) {  // TODO: NG
			InputSwitchPatch::PreLoad();
		}
		if (*Settings::MaxStdIO != -1) {
			MaxStdIOPatch::Install();
		}

		if (!REL::Module::IsNG() && (*Settings::MemoryManager || *Settings::MemoryManagerDebug)) {  // TODO: NG
			MemoryManagerPatch::Install();
		}

		if (!REL::Module::IsNG() && *Settings::ScaleformAllocator) {  // TODO: NG
			ScaleformAllocatorPatch::Install();
		}

		if (!REL::Module::IsNG() && *Settings::SmallBlockAllocator) {  // TODO: NG
			SmallBlockAllocatorPatch::Install();
		}

		if (REL::Module::IsF4() && *Settings::WorkshopMenu) {  // TODO: NG
			WorkshopMenuPatch::Install();
		}
	}

	void PostInit()
	{
		if (!REL::Module::IsNG() && *Settings::InputSwitch) {  // TODO: NG
			InputSwitchPatch::PostInit();
		}
	}
}
