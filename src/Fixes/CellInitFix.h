#pragma once

namespace Fixes::CellInitFix
{
	namespace detail
	{
		inline RE::BGSLocation* GetLocation(const RE::TESObjectCELL* a_cell)
		{
			const auto xLoc =
				a_cell && a_cell->extraList ?
					a_cell->extraList->GetByType<RE::ExtraLocation>() :
					nullptr;
			auto loc = xLoc ? xLoc->location : nullptr;

			if (loc && a_cell && !a_cell->IsInitialized()) {
				auto id =
					static_cast<std::uint32_t>(
						reinterpret_cast<std::uintptr_t>(a_cell));
				const auto file = a_cell->GetFile();
				RE::TESForm::AddCompileIndex(id, file);
				loc = RE::TESForm::GetFormByID<RE::BGSLocation>(id);
			}

			return loc;
		}
	}

	void Install();
}
