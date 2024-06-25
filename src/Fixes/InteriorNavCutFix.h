// Based off MIT code from https://github.com/Deweh/EngineFixesF4/tree/master/EngineFixesF4/src
// used in Interior NavCut Fix https://www.nexusmods.com/fallout4/mods/72904?tab=description
// Fixes the engine bug that causes workshop navmesh cuts to persist throughout all interior cells. 
// https://simsettlements.com/site/index.php?threads/major-new-fallout-4-bug-discovered-shared-interior-cell-navcutting.26755/
#ifndef NO_PPL
#	include <ppl.h>
#endif

namespace Fixes::InteriorNavCutFix
{
	inline static bool PerfCounterFreqAcquired = false;
	inline static double PerfCounterFreq = 0.0;

	static void GetPerfCounterFreq()
	{
		if (!PerfCounterFreqAcquired) {
			LARGE_INTEGER li;
			if (QueryPerformanceFrequency(&li)) {
				PerfCounterFreq = double(li.QuadPart) / 1000.0;
				PerfCounterFreqAcquired = true;
			} else {
				logger::warn("QueryPerformanceFrequency failed!");
			}
		}
	}

	static int64_t StartPerfCounter()
	{
		GetPerfCounterFreq();
		LARGE_INTEGER li;
		QueryPerformanceCounter(&li);
		return li.QuadPart;
	}

	static double GetPerfCounterMS(int64_t& counter)
	{
		LARGE_INTEGER li;
		QueryPerformanceCounter(&li);
		auto result = (double(li.QuadPart - (counter)) / PerfCounterFreq);
		counter = li.QuadPart;
		return result;
	}

	void DoUpdateNavmesh(RE::TESObjectREFR* refr, bool attaching)
	{
		if (refr->parentCell->cellFlags.any(RE::TESObjectCELL::Flag::kInterior) &&
			RE::Workshop::IsWorkshopItem(refr)) {
			if (auto enctZone = refr->parentCell->GetEncounterZone(); enctZone != nullptr && enctZone->IsWorkshop()) {
				refr->UpdateDynamicNavmesh((refr->IsDeleted() || refr->IsDisabled()) ? false : attaching);
			}
		}
	}

	class CellAttachDetachListener : public RE::BSTEventSink<RE::TESCellAttachDetachEvent>
	{
	public:
		inline static std::atomic<bool> updateTaskQueued = false;

		static CellAttachDetachListener* GetSingleton()
		{
			static CellAttachDetachListener instance;
			return &instance;
		}

		virtual RE::BSEventNotifyControl ProcessEvent(const RE::TESCellAttachDetachEvent& a_event, RE::BSTEventSource<RE::TESCellAttachDetachEvent>*) override
		{
			if (a_event.reference != nullptr && a_event.reference->parentCell != nullptr) {
				DoUpdateNavmesh(a_event.reference.get(), a_event.attached);
				if (!updateTaskQueued) {
					updateTaskQueued = true;
					F4SE::GetTaskInterface()->AddTask([]() {
						//auto perfTimer = StartPerfCounter();
						RE::DynamicNavmesh::GetSingleton()->ForceUpdate();
						//always ends up being <1ms
						//logger::debug("Finished detach/attach update in {:.0f}ms", GetPerfCounterMS(perfTimer));
						updateTaskQueued = false;
					});
				}
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	void RegisterNavMeshUpdateListener()
	{
		RegisterForCellAttachDetach(CellAttachDetachListener::GetSingleton());
		logger::info("Registered cell attach/detach listener.");
	}

	void ForceNavMeshUpdate()
	{
		auto perfTimer = StartPerfCounter();

		auto playerCell = RE::PlayerCharacter::GetSingleton()->parentCell;
		//logger::info("Player cell is: {:08X}", playerCell->formID);

		const auto& [map, lock] = RE::TESForm::GetAllForms();
		RE::BSAutoReadLock l{ lock };

#ifdef NO_PPL
		for (const RE::BSTTuple<const uint32_t, RE::TESForm*>& ele : *map) {
#else
				concurrency::parallel_for_each(map->begin(), map->end(), [&](RE::BSTTuple<const uint32_t, RE::TESForm*> ele) {
#endif
			RE::TESObjectCELL* cell = ele.second->As<RE::TESObjectCELL>();

			if (cell) {
				std::vector<RE::NiPointer<RE::TESObjectREFR>> refs;
				cell->spinLock.lock();
				auto& references = cell->references;

				for (uint32_t i = 0; i < references.size(); i++) {
					refs.push_back(references[i]);
				}
				cell->spinLock.unlock();

				for (auto& ref : refs) {
					if (ref != nullptr && ref->parentCell != nullptr) {
						DoUpdateNavmesh(ref.get(), ref->parentCell == playerCell);
					}
				}
			}
		}
#ifndef NO_PPL
				);
#endif

				RE::DynamicNavmesh::GetSingleton()->ForceUpdate();

				logger::info("Finished load-time navmesh updates in {:.0f}ms", GetPerfCounterMS(perfTimer));
	}

}
