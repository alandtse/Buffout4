#include "Patches/FasterWorkshopPatch.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <xbyak/xbyak.h>

#include <unordered_map>

namespace Patches::FasterWorkshopPatch
{
	namespace detail
	{
		// Keyword → recipes that reference that keyword as a recipe filter.
		// Built lazily on first workshop menu open after GameDataReady clears the map.
		using CobjMap = std::unordered_map<const RE::BGSKeyword*, std::vector<const RE::BGSConstructibleObject*>>;
		static CobjMap g_cobjMap;

		static void TryBuildMap() noexcept
		{
			if (!g_cobjMap.empty()) {
				return;
			}

			const auto dataHandler = RE::TESDataHandler::GetSingleton();
			if (!dataHandler) {
				return;
			}

			auto& objectArray = dataHandler->GetFormArray<RE::BGSConstructibleObject>();
			if (objectArray.empty()) {
				return;
			}

			g_cobjMap.reserve(3000);

			for (const auto* object : objectArray) {
				if (!object || !object->createdItem) {
					continue;
				}

				// Clamp to 1 keyword if the array looks corrupt (same defensive check Addictol uses).
				std::uint32_t count = object->filterKeywords.size;
				if (count > 32) {
					count = 1;
				}
				if (count == 0 || !object->filterKeywords.array) {
					continue;
				}

				for (std::uint32_t i = 0; i < count; ++i) {
					const auto keywordIndex = object->filterKeywords.array[i].keywordIndex;
					const auto* keyword = RE::BGSKeyword::GetTypedKeywordByIndex(
						RE::KeywordType::kRecipeFilter,
						keywordIndex);
					if (keyword) {
						g_cobjMap[keyword].push_back(object);
					}
				}
			}

			std::size_t total = 0;
			for (const auto& [key, value] : g_cobjMap) {
				total += value.size();
			}
			logger::info("FasterWorkshop: built COBJ map ({} keywords, {} entries)"sv,
				g_cobjMap.size(),
				total);
		}

		// Guards against a malformed FormList that (transitively) contains
		// itself — a self-referential FLST would otherwise recurse until
		// the stack overflows. Valid game data is acyclic and nests only a
		// few levels deep, so a generous cap costs nothing on real data.
		static constexpr std::uint32_t kMaxFormListDepth = 64;

		// Recursive worker. Kept separate from the detour entry point so the
		// depth parameter never leaks into the engine-facing ABI (the engine
		// calls the 2-arg entry below; a default arg would surface r8 garbage
		// as the depth).
		static bool CheckForValidChildrenImpl(void* a_ctx, const RE::BGSListForm* a_formList, std::uint32_t a_depth) noexcept
		{
			if (!a_formList || a_depth >= kMaxFormListDepth) {
				return false;
			}

			for (auto* form : a_formList->arrayOfForms) {
				if (!form) {
					continue;
				}

				if (form->Is(RE::ENUM_FORM_ID::kFLST)) {
					if (CheckForValidChildrenImpl(a_ctx, static_cast<RE::BGSListForm*>(form), a_depth + 1)) {
						return true;
					}
				} else if (form->Is(RE::ENUM_FORM_ID::kKYWD)) {
					if (form == a_ctx) {
						continue;
					}

					TryBuildMap();

					auto* keyword = static_cast<RE::BGSKeyword*>(form);
					const auto it = g_cobjMap.find(keyword);
					if (it != g_cobjMap.end()) {
						for (const auto* cobj : it->second) {
							if (RE::Workshop::WorkshopCanShowRecipe(
									const_cast<RE::BGSConstructibleObject*>(cobj),
									keyword)) {
								return true;
							}
						}
					}
				}
			}

			return false;
		}

		// Drop-in replacement for the game's CheckForValidChildren. Signature
		// must match the engine's (void*, BGSListForm*) — see worker above.
		static bool CachedCheckForValidChildren(void* a_ctx, const RE::BGSListForm* a_formList) noexcept
		{
			return CheckForValidChildrenImpl(a_ctx, a_formList, 0);
		}

		// Called by the leaf-node trampoline. Replaces an O(N) iteration over the game's
		// internal recipe list with an O(K) lookup against the cached map keyed by the
		// current filter keyword.
		using TryAddLeafNode_t = void(void* a_ctx, const RE::BGSConstructibleObject* a_cobj);
		static REL::Relocation<TryAddLeafNode_t> TryAddLeafNode{ REL::ID(281929) };

		extern "C" void HandlerLeafNode(void* a_ctx, const RE::BGSKeyword* a_keyword) noexcept
		{
			TryBuildMap();

			const auto it = g_cobjMap.find(a_keyword);
			if (it == g_cobjMap.end()) {
				return;
			}

			for (const auto* cobj : it->second) {
				TryAddLeafNode(a_ctx, cobj);
			}
		}

		// Target function that contains the leaf-node loop and the CheckForValidChildren call.
		// VR + OG share this ID and offsets; NG uses a different codepath handled via separate IDs.
		static REL::Relocation<std::uintptr_t> LeafNodeTarget{ REL::ID(934716), REL::Offset{ 0x1EF } };

		// Replaces the game's per-element loop:
		//
		//     rcx = &[rbp-0x28]       ; context argument for TryAddLeafNode
		//     rdx = *([rbp-0x28])     ; current filter keyword (stored at [rbp+0x30])
		//     call HandlerLeafNode    ; iterates our cached map and calls TryAddLeafNode
		//     jmp  continue           ; skip past the original loop (+0x212)
		//
		// The blob is copied verbatim into F4SE trampoline space; RIP-relative offsets to the
		// trailing data words stay valid because the blob moves as a single unit.
		struct LeafNodePatch :
			Xbyak::CodeGenerator
		{
			LeafNodePatch(std::uintptr_t a_continue, std::uintptr_t a_handler)
			{
				Xbyak::Label continueLabel, handlerLabel;

				lea(rcx, ptr[rbp - 0x28]);
				mov(rdx, ptr[rbp - 0x28]);
				mov(rdx, ptr[rdx]);
				call(ptr[rip + handlerLabel]);
				jmp(ptr[rip + continueLabel]);

				L(continueLabel);
				dq(a_continue);

				L(handlerLabel);
				dq(a_handler);
			}
		};

		// The original call at +0x51 inside the same function calls CheckForValidChildren.
		// We retarget it to our cached version.
		static REL::Relocation<std::uintptr_t> CheckForValidChildrenCall{ REL::ID(934716), REL::Offset{ 0x51 } };
	}

	void Install()
	{
		auto& trampoline = F4SE::GetTrampoline();

		// 1. CheckForValidChildren detour — cheap redirect of an existing 5-byte call.
		trampoline.write_call<5>(
			detail::CheckForValidChildrenCall.address(),
			reinterpret_cast<std::uintptr_t>(&detail::CachedCheckForValidChildren));
		logger::info("FasterWorkshop: installed CheckForValidChildren detour at {:x}"sv,
			detail::CheckForValidChildrenCall.address());

		// 2. Leaf-node loop replacement — allocate trampoline, copy xbyak blob, JMP to it.
		detail::LeafNodePatch patch{
			detail::LeafNodeTarget.address() + 0x23,  // post-loop continuation at +0x212
			reinterpret_cast<std::uintptr_t>(&detail::HandlerLeafNode)
		};
		patch.ready();
		trampoline.write_branch<5>(
			detail::LeafNodeTarget.address(),
			reinterpret_cast<std::uintptr_t>(trampoline.allocate(patch)));
		logger::info("FasterWorkshop: installed leaf-node trampoline at {:x}"sv,
			detail::LeafNodeTarget.address());
	}

	void ClearMap()
	{
		detail::g_cobjMap.clear();
	}
}
