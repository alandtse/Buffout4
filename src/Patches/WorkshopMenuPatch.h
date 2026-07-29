#pragma once
#undef min
namespace Patches::WorkshopMenuPatch
{
	namespace detail
	{
		class CompareFactory
		{
		private:
			struct eq_t
			{
				template <class T>
				[[nodiscard]] bool operator()(
					const T& a_lhs,
					const T& a_rhs) const noexcept  //
					requires(std::same_as<std::remove_cv_t<typename std::pointer_traits<T>::element_type>, RE::Workshop::WorkshopMenuNode>)
				{
					assert(a_lhs && a_rhs);
					return a_lhs->filterKeyword == a_rhs->filterKeyword &&
					       a_lhs->recipe == a_rhs->recipe &&
					       a_lhs->sourceFormListRecipe == a_rhs->sourceFormListRecipe &&
					       a_lhs->form == a_rhs->form;
				}
			};

			struct less_t
			{
				template <class T>
				[[nodiscard]] bool operator()(
					const T& a_lhs,
					const T& a_rhs) const noexcept  //
					requires(std::same_as<std::remove_cv_t<typename std::pointer_traits<T>::element_type>, RE::Workshop::WorkshopMenuNode>)
				{
					assert(a_lhs && a_rhs);

					const auto compNames = [this](const RE::TESForm* a_lhs, const RE::TESForm* a_rhs) noexcept {
						const auto lname = get_name(a_lhs);
						const auto rname = get_name(a_rhs);
						const auto cmp = !lname.empty() && !rname.empty() ?
						                     _strnicmp(lname.data(), rname.data(), std::min(lname.size(), rname.size())) :
						                     0;
						return cmp == 0 && lname.size() != rname.size() ?
						           lname.size() < rname.size() ? -1 : 1 :
						           cmp;
					};

					if (!!a_lhs->children.empty() != !!a_rhs->children.empty()) {
						// categories before items
						return !a_lhs->children.empty();
					} else if (!a_lhs->children.empty()) {
						// catgories
						return compNames(a_lhs->filterKeyword, a_rhs->filterKeyword) < 0;
					} else if (a_lhs->sourceFormListRecipe && a_lhs->sourceFormListRecipe == a_rhs->sourceFormListRecipe) {
						// sort using form list order
						assert(a_lhs->form && a_rhs->form);
						assert(a_lhs->sourceFormListRecipe->createdItem);
						assert(a_lhs->sourceFormListRecipe->createdItem->Is<RE::BGSListForm>());

						const auto& flist = static_cast<RE::BGSListForm&>(*a_lhs->sourceFormListRecipe->createdItem);
						const auto lpos = flist.GetItemIndex(*a_lhs->form);
						const auto rpos = flist.GetItemIndex(*a_rhs->form);

						assert(lpos && rpos);
						return *lpos < *rpos;
					} else {
						// items
						assert(a_lhs->recipe && a_rhs->recipe);
						const auto& lcobj = *a_lhs->recipe;
						const auto& rcobj = *a_rhs->recipe;

						const auto lprio = lcobj.GetWorkshopPriority();
						const auto rprio = rcobj.GetWorkshopPriority();
						if (lprio != rprio) {
							return lprio < rprio;
						}

						const auto naming = compNames(lcobj.createdItem, rcobj.createdItem);
						if (naming != 0) {
							return naming < 0;
						}

						return lcobj.formID < rcobj.formID;
					}
				}

				[[nodiscard]] std::string_view get_name(const RE::TESForm* a_form) const noexcept
				{
					if (a_form) {
						if (const auto fullname = a_form->As<RE::TESFullName>(); fullname) {
							return fullname->fullName;
						} else if (const auto it = _names.find(a_form); it != _names.end()) {
							return static_cast<std::string_view>(it->second);
						}
					}

					return ""sv;
				}

				const RE::BSTHashMap<const RE::TESForm*, RE::BGSLocalizedString>& _names;
			};

		public:
			[[nodiscard]] eq_t equal_to() const noexcept { return {}; }
			[[nodiscard]] less_t less() const noexcept { return { _names }; }

		public:
			const RE::BSTHashMap<const RE::TESForm*, RE::BGSLocalizedString>& _names{ RE::TESFullName::GetSparseFullNameMap() };
		};

		class NodeFactory
		{
		public:
			NodeFactory(RE::Workshop::WorkshopMenuNode& a_parent, std::uint32_t& a_uniqueID) noexcept :
				_parent(a_parent),
				_uniqueID(a_uniqueID)
			{}

			[[nodiscard]] RE::Workshop::WorkshopMenuNode& operator()()
			{
				const auto mem = _allocate(_mm, sizeof(RE::Workshop::WorkshopMenuNode), 0, false);
				assert(mem != nullptr);
				const auto& child =
					_parent.children.emplace_back(
						std::construct_at(
							std::launder(static_cast<RE::Workshop::WorkshopMenuNode*>(mem))));
				child->parent = &_parent;
				child->uniqueID = ++_uniqueID;
				child->row = _parent.row + 1;
				return *child;
			}

			[[nodiscard]] NodeFactory clone(RE::Workshop::WorkshopMenuNode& a_parent) const noexcept
			{
				return { *this, a_parent };
			}

		private:
			using Allocate_t = void*(RE::MemoryManager&, std::size_t, std::uint32_t, bool);

			NodeFactory(const NodeFactory& a_context, RE::Workshop::WorkshopMenuNode& a_parent) noexcept :
				_parent(a_parent),
				_uniqueID(a_context._uniqueID),
				_mm(a_context._mm),
				_allocate(a_context._allocate)
			{}

			RE::Workshop::WorkshopMenuNode& _parent;
			std::uint32_t& _uniqueID;
			RE::MemoryManager& _mm{ RE::MemoryManager::GetSingleton() };
			Allocate_t* const _allocate{ reinterpret_cast<Allocate_t*>(REL::ID(652767).address()) };
		};

		class LookupTable
		{
		public:
			using key_type = RE::BGSKeyword*;
			using mapped_type = std::vector<std::reference_wrapper<RE::BGSConstructibleObject>>;

			LookupTable() { assert(_currentWorkshop != nullptr); }

			[[nodiscard]] const mapped_type& operator()(RE::BGSKeyword& a_key)
			{
				const auto key = &a_key != _badKeyword ? &a_key : nullptr;
				if (const auto it = _lookupCache.find(key); it != _lookupCache.end()) {
					return it->second;
				} else if (_currentWorkshop) {
					mapped_type results;
					for (const auto& elem : _cobjs) {
						if (WorkshopCanShowRecipe(elem, key)) {
							results.push_back(elem.cobj);
						}
					}
					const auto ins = _lookupCache.emplace(key, std::move(results));
					return ins.first->second;
				} else [[unlikely]] {
					const auto ins = _lookupCache.emplace(key, mapped_type());
					return ins.first->second;
				}
			}

		private:
			template <class Key, class T, template <class> class Allocator>
			using map_t = boost::container::flat_map<Key, T, std::less<Key>, Allocator<std::pair<Key, T>>>;

			struct cobj_t
			{
				std::reference_wrapper<RE::BGSConstructibleObject> cobj;
				std::reference_wrapper<const RE::TESForm> createdItem;
				const RE::BGSKeyword* benchKeyword{ nullptr };
				boost::container::small_vector<const RE::BGSKeyword*, 2> filterKeywords;
				bool conditions{ false };
			};

			template <class C, class T>
			[[nodiscard]] __forceinline static bool BinarySearch(const C& a_haystack, T&& a_needle)
			{
				const auto it = std::lower_bound(a_haystack.begin(), a_haystack.end(), a_needle);
				return it != a_haystack.end() && *it == a_needle;
			}

			template <class C, class T>
			[[nodiscard]] __forceinline static bool LinearSearch(const C& a_haystack, T&& a_needle)
			{
				return std::find(a_haystack.begin(), a_haystack.end(), a_needle) != a_haystack.end();
			}

			[[nodiscard]] bool HasKeyword(const RE::BGSKeyword* a_keyword)
			{
				assert(_currentWorkshop != nullptr);

				if (const auto it = _hasKeywordCache.find(a_keyword); it != _hasKeywordCache.end()) {
					return it->second;
				} else {
					const auto ins = _hasKeywordCache.emplace(
						a_keyword,
						_currentWorkshop->HasKeyword(a_keyword, _currentInstance.get()));
					return ins.first->second;
				}
			}

			[[nodiscard]] bool HasStoredItem(const RE::TESForm& a_item) const
			{
				const auto needle =
					&a_item == _splineEndpointMarker && _workshopSplineObject ?
						_workshopSplineObject->GetFormID() :
						a_item.GetFormID();
				return BinarySearch(_storedItems, needle);
			}

			[[nodiscard]] bool WorkshopCanShowRecipe(const cobj_t& a_recipe, RE::BGSKeyword* a_filter)
			{
				if (((!a_filter && a_recipe.filterKeywords.empty()) ||
						LinearSearch(a_recipe.filterKeywords, a_filter)) &&
					(a_recipe.conditions ||
						HasStoredItem(a_recipe.createdItem) ||
						LinearSearch(a_recipe.filterKeywords, _workshopAlwaysShowIcon)) &&
					HasKeyword(a_recipe.benchKeyword)) {
					return true;
				} else {
					return false;
				}
			}

			map_t<key_type, mapped_type, tbb::scalable_allocator> _lookupCache;
			map_t<const RE::BGSKeyword*, bool, tbb::scalable_allocator> _hasKeywordCache;  // avoid duplicate checks
			const std::vector<cobj_t> _cobjs = []() {                                      // pre-filter cobjs
				const auto filters = []() -> std::span<const RE::BGSKeyword*> {
					if (const auto keywords = RE::BGSKeyword::GetTypedKeywords(); keywords) {
						auto& filters = (*keywords)[std::to_underlying(RE::KeywordType::kRecipeFilter)];
						return { const_cast<const RE::BGSKeyword**>(filters.data()), filters.size() };
					} else {
						return {};
					}
				}();

				std::vector<cobj_t> result;
				if (const auto dhandler = RE::TESDataHandler::GetSingleton(); dhandler) {
					const auto player = RE::PlayerCharacter::GetSingleton();
					const REL::Relocation<decltype(&RE::TESCondition::IsTrueForAllButFunction)> isTrueForAllButFunction{ REL::ID(1182457) };

					for (const auto cobj : dhandler->GetFormArray<RE::BGSConstructibleObject>()) {
						if (cobj &&
							cobj->createdItem &&
							(cobj->createdItem->Is<RE::BGSListForm>() || cobj->createdItem->IsBoundObject())) {
							const auto conditions = [&]() {
								constexpr auto hasPerk = static_cast<RE::SCRIPT_OUTPUT>(0x11C0);

								RE::ConditionCheckParams params;
								params.actionRef = player;
								params.targetRef = player;

								return isTrueForAllButFunction(&cobj->conditions, params, hasPerk);
							}();

							auto& back = result.emplace_back(
								*cobj,
								*cobj->createdItem,
								cobj->benchKeyword,
								std::initializer_list<const RE::BGSKeyword*>{},
								conditions);

							for (std::uint32_t i = 0; i < cobj->filterKeywords.size; ++i) {
								const auto index = cobj->filterKeywords.array[i].keywordIndex;
								if (index < filters.size()) {
									back.filterKeywords.push_back(filters[index]);
								}
							}
						}
					}
				}

				return result;
			}();
			const RE::TESForm* const _workshopSplineObject{ REL::Relocation<RE::BGSDefaultObject*>(REL::ID(678816))->form };
			const RE::BGSKeyword* const _badKeyword = []() {
				const auto dobj = RE::BGSDefaultObjectManager::GetSingleton();
				return dobj ?
				           dobj->GetDefaultObject<RE::BGSKeyword>(RE::DEFAULT_OBJECT::kWorkshopMiscItemKeyword) :
				           nullptr;
			}();
			const RE::BGSKeyword* const _workshopAlwaysShowIcon = []() {
				REL::Relocation<RE::BGSDefaultObject*> dobj{ REL::ID(1581182) };
				const auto form = dobj->form;
				return form ? form->As<RE::BGSKeyword>() : nullptr;
			}();
			const RE::TESObjectSTAT* const _splineEndpointMarker{ *REL::Relocation<RE::TESObjectSTAT**>(REL::ID(1116891)) };
			const RE::NiPointer<RE::TESObjectREFR> _currentWorkshop{ REL::Relocation<RE::ObjectRefHandle*>(REL::ID(737927))->get() };
			const RE::BSAutoWriteLock _extraLock = [&]() {  // avoid constantly locking/unlocking the same extralist
				return RE::BSAutoWriteLock(
					_currentWorkshop && _currentWorkshop->extraList ?
						&_currentWorkshop->extraList->extraRWLock :
						nullptr);
			}();
			const RE::BSTSmartPointer<RE::TBO_InstanceData> _currentInstance = [&]() {  // avoid looking up the same instance data every time
				const auto xInst =
					_currentWorkshop && _currentWorkshop->extraList ?
						_currentWorkshop->extraList->GetByType<RE::ExtraInstanceData>() :
						nullptr;
				return xInst ? xInst->data : nullptr;
			}();
			const std::vector<std::uint32_t> _storedItems = [&]() {
				std::vector<std::uint32_t> result;
				const auto xWorkshop =
					_currentWorkshop && _currentWorkshop->extraList ?
						_currentWorkshop->extraList->GetByType<RE::Workshop::ExtraData>() :
						nullptr;
				if (xWorkshop) {
					for (const auto item : xWorkshop->deletedItems) {
						if (item && item->count > 0) {
							result.push_back(item->formID);
						}
					}
					std::sort(result.begin(), result.end());
				}
				return result;
			}();
		};

#if 0
		[[nodiscard]] inline bool CompareRecursive(
			const RE::Workshop::WorkshopMenuNode& a_lhs,
			const RE::Workshop::WorkshopMenuNode& a_rhs)
		{
			const auto getChildren = [&](const RE::Workshop::WorkshopMenuNode& a_node) {
				std::vector<const RE::Workshop::WorkshopMenuNode*> children;
				children.reserve(a_node.children.size());
				for (const auto& child : a_node.children) {
					children.push_back(child.get());
				}
				std::sort(children.begin(), children.end(), [](auto&& a_lhs, auto&& a_rhs) noexcept {
#	define CASE(a_name)                                                            \
		if (!!a_lhs->a_name != !!a_rhs->a_name || a_lhs->a_name != a_rhs->a_name) { \
			return !!a_lhs->a_name != !!a_rhs->a_name ?                             \
			           !!a_lhs->a_name :                                            \
			           a_lhs->a_name->GetFormID() < a_rhs->a_name->GetFormID();     \
		}
					// clang-format off
					CASE(filterKeyword)
					else CASE(recipe)
					else CASE(form)
					else return false;
					// clang-format on
#	undef CASE
				});
				return children;
			};

			const auto lChildren = getChildren(a_lhs);
			const auto rChildren = getChildren(a_rhs);

			if (a_lhs.filterKeyword == a_rhs.filterKeyword &&
				a_lhs.children.size() == a_rhs.children.size() &&
				a_lhs.recipe == a_rhs.recipe &&
				a_lhs.sourceFormListRecipe == a_rhs.sourceFormListRecipe &&
				a_lhs.form == a_rhs.form &&
				a_lhs.row == a_rhs.row) {
				for (std::size_t i = 0; i < lChildren.size(); ++i) {
					assert(lChildren[i] && rChildren[i]);
					if (!CompareRecursive(*lChildren[i], *rChildren[i])) {
						return false;
					}
				}
				return true;
			} else {
				const auto [lIt, rIt] = std::mismatch(
					lChildren.begin(),
					lChildren.end(),
					rChildren.begin(),
					rChildren.end(),
					[](auto&& a_lhs, auto&& a_rhs) noexcept {
						return a_lhs->filterKeyword == a_rhs->filterKeyword &&
					           a_lhs->recipe == a_rhs->recipe &&
					           a_lhs->form == a_rhs->form;
					});
				[[maybe_unused]] const auto lPos = lIt - lChildren.begin();
				[[maybe_unused]] const auto rPos = rIt - rChildren.begin();
				return false;
			}
		}
#endif

		inline void FixupChildren(
			const CompareFactory& a_comp,
			RE::Workshop::WorkshopMenuNode& a_node)
		{
			const auto beg = std::stable_partition(
				a_node.children.begin(),
				a_node.children.end(),
				[](const auto& a_node) {
					assert(a_node);
					return !a_node->children.empty();
				});
			std::stable_sort(beg, a_node.children.end(), a_comp.less());
			if (const auto last = std::unique(a_node.children.begin(), a_node.children.end(), a_comp.equal_to());
				last != a_node.children.end()) {
				a_node.children.erase(last, a_node.children.end());
			}

			for (std::uint32_t i = 0; i < a_node.children.size(); ++i) {
				const auto& child = a_node.children[i];
				assert(child != nullptr);
				child->column = static_cast<std::uint16_t>(i);
			}
		}

		inline bool MakeSubMenu(
			LookupTable&,
			const CompareFactory&,
			NodeFactory,
			RE::TESForm&);

		__forceinline void EnumerateFLST(
			LookupTable& a_table,
			const CompareFactory& a_comp,
			NodeFactory a_factory,
			RE::Workshop::WorkshopMenuNode& a_node,
			const RE::BGSListForm& a_flst)
		{
			auto& children = a_node.children;
			for (const auto lform : a_flst.arrayOfForms) {
				if (lform &&
					!lform->IsDeleted() &&
					MakeSubMenu(a_table, a_comp, a_factory, *lform) &&
					children.back()->children.empty()) {
					children.pop_back();
				}

				FixupChildren(a_comp, a_node);
			}
		}

		inline void MakeLeaf(
			NodeFactory a_factory,
			RE::BGSConstructibleObject& a_cobj)
		{
			assert(a_cobj.createdItem != nullptr);
			const auto make = [&](RE::TESForm* a_form) -> decltype(auto) {
				auto& child = a_factory();
				child.recipe = &a_cobj;
				child.form = a_form;
				return child;
			};

			switch (a_cobj.createdItem->GetFormType()) {
			case RE::ENUM_FORM_ID::kFLST:
				for (const auto& flst = static_cast<RE::BGSListForm&>(*a_cobj.createdItem);
					 const auto form : flst.arrayOfForms) {
					make(form).sourceFormListRecipe = &a_cobj;
				}
				break;
			default:
				make(a_cobj.createdItem);
				break;
			}
		}

		bool MakeSubMenu(
			LookupTable& a_table,
			const CompareFactory& a_comp,
			NodeFactory a_factory,
			RE::TESForm& a_form)
		{
			assert(!a_form.IsDeleted());

			bool success = false;
			const auto make = [&](RE::BGSKeyword* a_keyword) -> decltype(auto) {
				auto& child = a_factory();
				child.filterKeyword = a_keyword;
				return child;
			};

			switch (a_form.GetFormType()) {
			case RE::ENUM_FORM_ID::kKYWD:
				{
					auto& kywd = static_cast<RE::BGSKeyword&>(a_form);
					const auto& matches = a_table(kywd);
					if (!matches.empty()) {
						success = true;

						auto& child = make(&kywd);
						const auto factory = a_factory.clone(child);
						for (RE::BGSConstructibleObject& cobj : matches) {
							MakeLeaf(factory, cobj);
						}

						FixupChildren(a_comp, child);
					}
				}
				break;
			case RE::ENUM_FORM_ID::kFLST:
				{
					auto& flst = static_cast<RE::BGSListForm&>(a_form);
					const auto front = !flst.arrayOfForms.empty() ? flst.arrayOfForms.front() : nullptr;
					if (front && front->Is<RE::BGSKeyword>()) {
						success = true;

						auto& kywd = static_cast<RE::BGSKeyword&>(*front);
						auto& child = make(&kywd);
						EnumerateFLST(
							a_table,
							a_comp,
							a_factory.clone(child),
							child,
							flst);
					}
				}
				break;
			default:
				assert(false);
				break;
			}

			return success;
		}

		inline void MakeRootMenu(
			RE::Workshop::WorkshopMenuNode& a_rootNode,
			RE::BGSListForm a_rootList)
		{
			a_rootNode.uniqueID = static_cast<std::uint32_t>(-1);
			a_rootNode.row = static_cast<std::uint16_t>(-1);

			LookupTable table;
			EnumerateFLST(
				table,
				{},
				{ a_rootNode, a_rootNode.uniqueID },
				a_rootNode,
				a_rootList);

			a_rootNode.uniqueID = static_cast<std::uint32_t>(-1);
			a_rootNode.row = 0;
		}

		inline void BuildWorkShopMenuNodeTree()
		{
			const REL::Relocation<RE::ObjectRefHandle*> hCurrentWorkshop{ REL::ID(737927) };
			const REL::Relocation<RE::ObjectRefHandle*> hLastWorkshop{ REL::ID(56095) };
			const REL::Relocation<std::uint16_t*> currentRow{ REL::ID(833923) };
			const REL::Relocation<RE::Workshop::WorkshopMenuNode*> wireNode{ REL::ID(450339) };
			const REL::Relocation<RE::Workshop::WorkshopMenuNode*> rootNode{ REL::ID(1421138) };
			const REL::Relocation<RE::BGSDefaultObject*> rootList{ REL::ID(1514918) };

			std::optional<std::uint32_t> crc;
			if (*hCurrentWorkshop == *hLastWorkshop) {
				std::uint32_t column = 0;
				const auto node = RE::Workshop::GetSelectedWorkshopMenuNode(*currentRow, column);
				std::uint32_t data = 0;
				if (node) {
					data += node->form ? node->form->GetFormID() : 0;
					data += node->filterKeyword ? node->filterKeyword->GetFormID() : 0;
					data += node->row;
				}
				crc = RE::BSCRC32<std::uint32_t>()(data);
			}

			*currentRow = 0;
			rootNode->Clear();

			const auto dhandler = RE::TESDataHandler::GetSingleton();
			const auto currentWorkshop = hCurrentWorkshop->get();
			if (dhandler && currentWorkshop) {
				const auto splineEndpointMarker = *REL::Relocation<RE::TESObjectSTAT**>{ REL::ID(1116891) };
				const auto xInst =
					currentWorkshop->extraList ?
						currentWorkshop->extraList->GetByType<RE::ExtraInstanceData>() :
						nullptr;
				const auto inst = xInst ? xInst->data : nullptr;
				for (const auto cobj : dhandler->GetFormArray<RE::BGSConstructibleObject>()) {
					if (cobj &&
						cobj->createdItem &&
						cobj->createdItem == splineEndpointMarker &&
						currentWorkshop->HasKeyword(cobj->benchKeyword, inst.get())) {
						wireNode->recipe = cobj;
						wireNode->form = cobj->createdItem;
						break;
					}
				}
			}

			if (const auto form = rootList->form; form && form->Is<RE::BGSListForm>()) {
				MakeRootMenu(*rootNode, *static_cast<RE::BGSListForm*>(form));
			}

			if (crc) {
				rootNode->FindAndSetSelectedNode(0, *crc, *currentRow);
			}
		}
	}

	inline void Install()
	{
		REL::Relocation<std::uintptr_t> target{ REL::ID(1515994) };
		constexpr std::size_t size = 0x24F;
		REL::safe_fill(target.address(), REL::INT3, size);
		stl::asm_jump(target.address(), size, reinterpret_cast<std::uintptr_t>(detail::BuildWorkShopMenuNodeTree));
		logger::info("installed WorkshopMenu patch"sv);
	}
}
