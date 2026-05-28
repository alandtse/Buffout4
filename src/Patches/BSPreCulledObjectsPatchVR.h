#pragma once

#include <shared_mutex>

namespace Patches::BSPreCulledObjectsPatchVR
{
	// VR port of Buffout4's BSPreCulledObjectsPatch. The PC version
	// assumes a hash table layout and culling-call structure that VR
	// does not match. Specifically:
	//
	//   * VR inlines the BSTObjectArena iterator with raw-offset field
	//     access (no private BSTObjectArenaScrapAllocBase base).
	//   * VR has no separate PreCulledRainIDs / PreCulledShadowIDs arrays;
	//     object records carry their own IDs.
	//   * VR's main BSCullingGroup::Add path still queries the stock
	//     IDTo3D hash table (at 0x14391D880) via a Get3DForID call inside
	//     the AddObjectByID worker. We can't overwrite that path outright,
	//     so we hook the call site and keep the stock hash table
	//     populated via a trampoline on UpdateIDto3DMap.
	//
	// All VR addresses verified via Ghidra against Fallout4VR.exe 1.2.72.0.

	namespace detail
	{
		class IDTo3DHandler;

		[[nodiscard]] inline auto SplitID(std::uint32_t a_id) noexcept
			-> std::pair<
				std::uint32_t,  // form id w/o file index
				std::uint8_t>   // file index
		{
			return {
				a_id & 0x00FFFFFF,
				static_cast<std::uint8_t>((a_id >> (8 * 3)) & 0xFF)
			};
		}

		class IDTo3DMap
		{
		public:
			using value_type =
				robin_hood::pair<
					std::uint32_t,                   // full form id
					RE::NiPointer<RE::NiAVObject>>;  // object 3D
			using container_type =
				std::vector<
					value_type,
					tbb::scalable_allocator<value_type>>;

			// Single-object lookup by full formID. Used by the Get3DForID
			// call-site hook — we need exactly one NiAVObject*, not the
			// full span the AddToCullingGroup path uses.
			[[nodiscard]] RE::NiAVObject* find_one(std::uint32_t a_id) const noexcept
			{
				const auto [base, idx] = SplitID(a_id);
				if (idx == 0x00 || idx == 0xFD) [[likely]] {
					const auto it = _special.find(a_id);
					if (it != _special.end()) {
						return it->second.get();
					}
				} else {
					const auto it = _generic.find(base);
					if (it != _generic.end()) {
						const auto& objs = it->second;
						const auto rm = std::lower_bound(objs.begin(), objs.end(), a_id, less_t{});
						if (rm != objs.end() && rm->first == a_id) {
							return rm->second.get();
						}
					}
				}
				return nullptr;
			}

			[[nodiscard]] auto find(std::uint32_t a_id) const noexcept
				-> std::span<const value_type>
			{
				const auto [base, idx] = SplitID(a_id);
				if (idx == 0x00 || idx == 0xFD) [[likely]] {
					const auto it = _special.find(a_id);
					if (it != _special.end()) {
						return { std::addressof(*it), 1 };
					}
				} else {
					const auto it = _generic.find(base);
					if (it != _generic.end()) {
						return { it->second.begin(), it->second.end() };
					}
				}
				return {};
			}

			void insert_or_assign(std::uint32_t a_id, RE::NiAVObject* a_obj) noexcept
			{
				const auto idx = SplitID(a_id).second;
				if (idx == 0x00 || idx == 0xFD) [[likely]] {
					insert_special(a_id, a_obj);
				} else {
					insert_generic(a_id, a_obj);
				}
			}

			void erase(std::uint32_t a_id) noexcept
			{
				const auto idx = SplitID(a_id).second;
				if (idx == 0x00 || idx == 0xFD) [[likely]] {
					_special.erase(a_id);
				} else {
					erase_generic(a_id);
				}
			}

		protected:
			friend class IDTo3DHandler;

			IDTo3DMap() = default;
			IDTo3DMap(const IDTo3DMap&) = default;
			IDTo3DMap(IDTo3DMap&&) = default;
			~IDTo3DMap() = default;
			IDTo3DMap& operator=(const IDTo3DMap&) = default;
			IDTo3DMap& operator=(IDTo3DMap&&) = default;

		private:
			struct less_t
			{
				[[nodiscard]] bool operator()(const value_type& a_lhs, const value_type& a_rhs) const noexcept
				{
					return a_lhs.first < a_rhs.first;
				}

				[[nodiscard]] bool operator()(const value_type& a_lhs, std::uint32_t a_rhs) const noexcept
				{
					return a_lhs.first < a_rhs;
				}
			};

			void erase_generic(std::uint32_t a_id) noexcept
			{
				const auto [base, idx] = SplitID(a_id);
				const auto it = _generic.find(base);
				if (it != _generic.end()) [[likely]] {
					auto& objs = it->second;
					const auto rm = std::lower_bound(objs.begin(), objs.end(), a_id, less_t{});
					if (rm != objs.end() && rm->first == a_id) {
						objs.erase(rm);
						if (objs.empty()) {
							_generic.erase(it);
						}
					}
				}
			}

			void insert_generic(std::uint32_t a_id, RE::NiAVObject* a_obj)
			{
				const auto base = SplitID(a_id).first;
				auto& objs = [&]() -> container_type& {
					const auto it = _generic.find(base);
					if (it != _generic.end()) {
						return it->second;
					} else {
						return _generic
						    .emplace(
								base,
								std::initializer_list<value_type>{})
						    .first->second;
					}
				}();

				// Insert at the lower_bound position so the bucket stays
				// sorted. O(n) shift instead of the PC patch's O(n log n)
				// re-sort after every insertion.
				const auto it = std::lower_bound(objs.begin(), objs.end(), a_id, less_t{});
				if (it != objs.end() && it->first == a_id) [[unlikely]] {
					it->second.reset(a_obj);
				} else {
					objs.emplace(it, a_id, a_obj);
				}
			}

			void insert_special(std::uint32_t a_id, RE::NiAVObject* a_obj)
			{
				const auto it = _special.find(a_id);
				if (it != _special.end()) {
					it->second.reset(a_obj);
				} else {
					_special.emplace(a_id, a_obj);
				}
			}

			robin_hood::unordered_flat_map<
				std::uint32_t,  // form id w/o file index
				container_type>
				_generic;
			robin_hood::unordered_flat_map<
				std::uint32_t,  // full form id
				value_type::second_type>
				_special;
		};

		class IDTo3DHandler
		{
		public:
			using value_type = IDTo3DMap;
			// shared_mutex so the Get3DForID call-site hook can run on
			// worker threads in parallel. Updates (UpdateIDto3DMap) take
			// the exclusive lock.
			using lock_type = std::shared_mutex;

			// VR's BSTObjectArena layout differs from CommonLibF4's PC
			// layout. PC has BSTObjectArenaScrapAllocBase as a private
			// base (+0x00, 8 bytes), shifting every field by 0x08. VR has
			// no such base, so the same fields sit 0x08 lower. Verified
			// against the iteration pattern in the VR rain hook
			// (FUN_1427E0F60) which reads anchors at [arena+0x10],
			// [arena+0x20], [arena+0x28] and walks pages of 0x2000 bytes
			// with the next-page pointer at page+0x2000.
			//
			// We do NOT use RE::BSTObjectArena<> here — its iterator
			// would read PC offsets and crash on VR.
			struct PreCulledObjects_t
			{
				void*         head;    // 0x00 - Page*
				void*         next;    // 0x08 - Page** (unused by us)
				void*         tail;    // 0x10 - Page*
				void*         free;    // 0x18 - Page* (unused by us)
				std::byte*    end_;    // 0x20 - one past last live record
				std::byte*    begin_;  // 0x28 - first live record
				std::uint32_t size_;   // 0x30
			};

			// 16-byte record per VR's iteration stride. VR pages hold
			// 0x2000 / 16 = 512 records, matching the PC arena's N=512
			// template parameter. The next-page pointer lives at
			// page+0x2000 (immediately after the record buffer).
			static constexpr std::size_t kArenaPageBytes = 0x2000;
			static constexpr std::size_t kArenaRecordBytes =
				sizeof(RE::BSPreCulledObjects::ObjectRecord);
			static_assert(
				kArenaRecordBytes == 16,
				"VR arena iteration assumes 16-byte ObjectRecord stride");

			// Exclusive access — used by UpdateIDto3DMap writes.
			class Accessor
			{
			public:
				[[nodiscard]] value_type&       operator*() noexcept { return _proxy; }
				[[nodiscard]] const value_type& operator*() const noexcept { return _proxy; }
				[[nodiscard]] value_type*       operator->() noexcept { return std::addressof(_proxy); }
				[[nodiscard]] const value_type* operator->() const noexcept { return std::addressof(_proxy); }

			private:
				friend IDTo3DHandler;

				Accessor(lock_type& a_lock, value_type& a_proxy) noexcept :
					_locker(a_lock),
					_proxy(a_proxy)
				{}

				std::unique_lock<lock_type> _locker;
				value_type&                 _proxy;
			};

			// Shared (read-only) access — multiple threads can hold this
			// concurrently. Used by the Get3DForID call-site hook.
			class SharedAccessor
			{
			public:
				[[nodiscard]] const value_type& operator*() const noexcept { return _proxy; }
				[[nodiscard]] const value_type* operator->() const noexcept { return std::addressof(_proxy); }

				// Early release so callers can drop the shared_lock before
				// performing unrelated work (e.g. calling OriginalGet3DForID
				// on the sentinel path). Idempotent.
				void release() noexcept
				{
					if (_locker.owns_lock()) {
						_locker.unlock();
					}
				}

			private:
				friend IDTo3DHandler;

				SharedAccessor(lock_type& a_lock, const value_type& a_proxy) noexcept :
					_locker(a_lock),
					_proxy(a_proxy)
				{}

				std::shared_lock<lock_type> _locker;
				const value_type&           _proxy;
			};

			IDTo3DHandler(const IDTo3DHandler&) = delete;
			IDTo3DHandler(IDTo3DHandler&&) = delete;
			IDTo3DHandler& operator=(const IDTo3DHandler&) = delete;
			IDTo3DHandler& operator=(IDTo3DHandler&&) = delete;

			[[nodiscard]] static IDTo3DHandler& GetSingleton() noexcept
			{
				static IDTo3DHandler singleton;
				return singleton;
			}

			[[nodiscard]] Accessor       Access() noexcept { return { _lock, _value }; }
			[[nodiscard]] SharedAccessor SharedAccess() const noexcept { return { _lock, _value }; }

			void AddToCullingGroup(void* a_group, RE::NiAVObject* a_obj, const RE::NiBound& a_bound, std::uint32_t a_flags) const
			{
				_addToCullingGroup(a_group, a_obj, a_bound, a_flags);
			}

			void TrackVisibility(std::uint32_t a_id) const { _trackVisibility(a_id); }
			void CallVisibilityCallbacks() const { _callVisibilityCallbacks(); }

			void RemoveVisibilityCallBackForID(std::uint32_t a_id) const
			{
				_removeVisibilityCallBackForID(a_id);
			}

			[[nodiscard]] auto* PreCulledDynamicRainObjects() const noexcept
			{
				return _preCulledDynamicRainObjects;
			}

			[[nodiscard]] auto* PreCulledDynamicShadowObjects() const noexcept
			{
				return _preCulledDynamicShadowObjects;
			}

			// Cached binary value of the bUsePreCulledObjects INI setting
			// (DAT_14391D830). VR's stock UpdateIDto3DMap reads this exact
			// byte; we replicate that gating to keep behaviour identical.
			[[nodiscard]] bool UsePreCulledObjects() const noexcept
			{
				return *_usePreCulledObjectsCachedBool != 0;
			}

		private:
			using AddToCullingGroup_t       = void(void*, RE::NiAVObject*, const RE::NiBound&, std::uint32_t);
			using TrackVisibility_t         = void(std::uint32_t);
			using CallVisibilityCallbacks_t = void();
			using RemoveVisibility_t        = void(std::uint32_t);

			IDTo3DHandler()  = default;
			~IDTo3DHandler() = default;

			// VR Address Library doesn't yet expose IDs for several of
			// these functions/globals, so we use module-relative offsets
			// verified via Ghidra against Fallout4VR.exe 1.2.72.0.
			AddToCullingGroup_t* const _addToCullingGroup{
				reinterpret_cast<AddToCullingGroup_t*>(REL::Offset(0x1D4F840).address())
			};
			TrackVisibility_t* const _trackVisibility{
				reinterpret_cast<TrackVisibility_t*>(REL::Offset(0x27E0C20).address())
			};
			CallVisibilityCallbacks_t* const _callVisibilityCallbacks{
				reinterpret_cast<CallVisibilityCallbacks_t*>(REL::Offset(0x27E1C50).address())
			};
			RemoveVisibility_t* const _removeVisibilityCallBackForID{
				reinterpret_cast<RemoveVisibility_t*>(REL::Offset(0x27DFC70).address())
			};

			const PreCulledObjects_t*& _preCulledDynamicRainObjects{
				*reinterpret_cast<const PreCulledObjects_t**>(REL::Offset(0x6878B30).address())
			};
			const PreCulledObjects_t*& _preCulledDynamicShadowObjects{
				*reinterpret_cast<const PreCulledObjects_t**>(REL::Offset(0x6878B50).address())
			};
			const std::uint8_t* const _usePreCulledObjectsCachedBool{
				reinterpret_cast<const std::uint8_t*>(REL::Offset(0x391D830).address())
			};

			// std::shared_mutex on Windows maps to SRWLock (~20-40 ns per
			// shared acquire). At ~500k Get3DForID calls/sec that is on
			// the order of 10-20 ms/sec of wall time spent in the lock.
			// A lock-free map (atomic snapshot + copy-on-write updates,
			// or RCU) would remove it at the cost of a significant
			// rewrite. HookedGet3DForID below shortens the critical
			// section by releasing the lock before any subsequent work
			// (sentinel verifier call, counter publish).
			mutable lock_type _lock;
			value_type        _value;
		};

		// ----------------------------------------------------------------
		// Hook bodies.
		//
		// VR's hooked rain/shadow paths only need to walk the ObjectRecord
		// arena — there are no separate PreCulled*ID arrays to cross-check
		// against. Match Buffout4 PC's DoAddToCullingGroup shape (filter
		// per record, call the stock AddToCullingGroup worker) but with
		// VR's per-record shadowcaster check.
		// ----------------------------------------------------------------

		// Hand-rolled VR arena walk. Mirrors the inlined iterator logic
		// in FUN_1427E0F60: start at begin_, advance 16 bytes per record,
		// and when the cursor reaches the current page's end
		// (head + 0x2000), chase *(page + 0x2000) for the next page.
		// Stops when the cursor reaches end_.
		template <class Fn>
		inline void ForEachArenaRecord(
			const IDTo3DHandler::PreCulledObjects_t* a_arena, Fn a_fn)
		{
			if (!a_arena || !a_arena->begin_ || a_arena->begin_ == a_arena->end_) {
				return;
			}

			auto* page   = static_cast<std::byte*>(a_arena->head);
			auto* cursor = a_arena->begin_;
			auto* endAll = a_arena->end_;
			if (!page) {
				return;
			}
			auto* pageEnd = page + IDTo3DHandler::kArenaPageBytes;

			// begin_ must lie on the head page; otherwise the
			// `cursor == pageEnd` check below will never flip the page
			// and we'll run off into unrelated memory. Bail safely if
			// the invariant is violated.
			if (cursor < page || cursor >= pageEnd) [[unlikely]] {
				logger::error(
					"ForEachArenaRecord: begin ({:X}) not on head page [{:X}..{:X})"sv,
					reinterpret_cast<std::uintptr_t>(cursor),
					reinterpret_cast<std::uintptr_t>(page),
					reinterpret_cast<std::uintptr_t>(pageEnd));
				return;
			}

			constexpr std::size_t kMaxIter = 1'000'000;
			std::size_t           count    = 0;

			while (cursor != endAll) {
				if (++count > kMaxIter) {
					logger::error(
						"ForEachArenaRecord: safety limit {} hit; arena={:X} head={:X} begin={:X} end={:X} cursor={:X}"sv,
						kMaxIter,
						reinterpret_cast<std::uintptr_t>(a_arena),
						reinterpret_cast<std::uintptr_t>(a_arena->head),
						reinterpret_cast<std::uintptr_t>(a_arena->begin_),
						reinterpret_cast<std::uintptr_t>(a_arena->end_),
						reinterpret_cast<std::uintptr_t>(cursor));
					break;
				}

				const auto& record =
					*reinterpret_cast<const RE::BSPreCulledObjects::ObjectRecord*>(cursor);
				a_fn(record);

				cursor += IDTo3DHandler::kArenaRecordBytes;
				// Defensive: if begin_/end_ aren't stride-aligned, cursor
				// can overshoot the current page without exact equality.
				// Bail rather than chase a corrupt next-page pointer.
				// We don't compare cursor against endAll directly because
				// begin_ and end_ can live on different chained pages at
				// arbitrary virtual addresses; only the per-page bound is
				// safe to compare with < / >.
				if (cursor > pageEnd) [[unlikely]] {
					logger::error(
						"ForEachArenaRecord: cursor overshot page; arena={:X} page={:X} pageEnd={:X} cursor={:X} end={:X}"sv,
						reinterpret_cast<std::uintptr_t>(a_arena),
						reinterpret_cast<std::uintptr_t>(page),
						reinterpret_cast<std::uintptr_t>(pageEnd),
						reinterpret_cast<std::uintptr_t>(cursor),
						reinterpret_cast<std::uintptr_t>(endAll));
					break;
				}
				if (cursor == pageEnd) {
					page = *reinterpret_cast<std::byte**>(pageEnd);
					if (!page) {
						break;
					}
					cursor  = page;
					pageEnd = page + IDTo3DHandler::kArenaPageBytes;
				}
			}
		}

		inline void AddToRainCullingGroup(void* a_cullingGroup)
		{
			auto& handler = IDTo3DHandler::GetSingleton();
			ForEachArenaRecord(
				handler.PreCulledDynamicRainObjects(),
				[&](const RE::BSPreCulledObjects::ObjectRecord& a_record) {
					if (!a_record.obj) {
						return;
					}
					// VR's stock rain path skips shadowcasters in the
					// arena pass. Mirror that here.
					const auto raw = reinterpret_cast<const std::uint8_t*>(a_record.obj);
					const auto flags = *reinterpret_cast<const std::uint64_t*>(raw + 0x108);
					if (((flags >> 0x28) & 1ULL) == 0) {
						handler.AddToCullingGroup(
							a_cullingGroup,
							a_record.obj,
							a_record.obj->worldBound,
							a_record.flags);
					}
				});
		}

		inline void AddToShadowCullingGroup(void* a_cullingGroup)
		{
			auto& handler = IDTo3DHandler::GetSingleton();
			ForEachArenaRecord(
				handler.PreCulledDynamicShadowObjects(),
				[&](const RE::BSPreCulledObjects::ObjectRecord& a_record) {
					if (a_record.obj) {
						handler.AddToCullingGroup(
							a_cullingGroup,
							a_record.obj,
							a_record.obj->worldBound,
							a_record.flags);
					}
				});
		}

		// Original UpdateIDto3DMap. Saved at install time by a manual
		// trampoline buffer that runs the original's 5-byte prologue
		// (mov [rsp+8], rbx) then jumps to target+5. We call through
		// this so the stock IDTo3D hash table at 0x14391D880 stays
		// populated for un-hooked Get3DForID call sites.
		using UpdateIDto3DMap_t = void(std::uint32_t, RE::NiAVObject*);
		inline UpdateIDto3DMap_t* OriginalUpdateIDto3DMap = nullptr;

		inline void UpdateIDto3DMap(std::uint32_t a_id, RE::NiAVObject* a_obj)
		{
			// 1. Call the original to keep the stock hash table
			//    populated for the un-hooked main AddToCullingGroup path.
			if (OriginalUpdateIDto3DMap) {
				OriginalUpdateIDto3DMap(a_id, a_obj);
			}

			// 2. Also write to our fast robin_hood map, used by the
			//    hooked rain/shadow arenas and the Get3DForID call-site
			//    hook.
			auto& handler = IDTo3DHandler::GetSingleton();
			if (!handler.UsePreCulledObjects()) {
				return;
			}
			auto token = handler.Access();
			if (a_obj) {
				token->insert_or_assign(a_id, a_obj);
			} else {
				token->erase(a_id);
				// RemoveVisibilityCallBackForID already called by the
				// original above — double-call is idempotent.
			}
		}

		// ----------------------------------------------------------------
		// Get3DForID call-site hook.
		//
		// Intercepts the call at 0x27E0675 inside the AddObjectByID worker
		// (function at 0x27E05F0). VR's stock code resolves every formID
		// through the slow stock hash table at 0x14391D880 — a branch
		// flag at 0x391D848 is permanently 1 and never cleared, so the
		// fast path is effectively dead. We resolve from our robin_hood
		// map (O(1) amortised), return nullptr on miss, and keep a
		// 1-in-4096 sentinel verifier that calls the original to catch
		// any future map-vs-stock divergence.
		//
		// Signature: NiAVObject* __fastcall Get3DForID(uint32_t formID)
		// ----------------------------------------------------------------
		using Get3DForID_t = RE::NiAVObject*(std::uint32_t);
		inline Get3DForID_t* OriginalGet3DForID = nullptr;

		// Performance counters — atomic, near-zero overhead on the hot
		// path. A thread-local batch counter aggregates up to kBatchSize
		// hits before publishing to the global atomic, cutting cross-core
		// cacheline ping-pong by that factor. The heartbeat log is still
		// emitted from the publishing thread when a gate mask trips, so
		// QueryPerformanceCounter is rare.
		struct PerfCounters
		{
			std::atomic<std::uint64_t> mapHits{ 0 };
			std::atomic<std::uint64_t> mapMisses{ 0 };
			std::atomic<std::uint64_t> ghostMisses{ 0 };
			std::atomic<std::uint64_t> realMisses{ 0 };
			std::atomic<std::int64_t>  lastLogTime{ 0 };

			static constexpr double        kLogIntervalSeconds = 60.0;
			static constexpr std::uint64_t kSentinelMask       = 0xFFFu;   // 1 in 4096
			static constexpr std::uint64_t kLogGateMask        = 0x3FFFu;  // 1 in 16384

			// Thread-local batch size — up to this many hits aggregate in
			// a thread's local counter before publishing to the global
			// atomic. 64 keeps the lag per-thread bounded (<0.2ms at 500k
			// calls/sec across 4 threads) while reducing atomic traffic.
			static constexpr std::uint32_t kBatchSize = 64;

			static_assert(
				(kSentinelMask & (kSentinelMask + 1)) == 0,
				"kSentinelMask must be of the form 2^n - 1");
			static_assert(
				(kLogGateMask & (kLogGateMask + 1)) == 0,
				"kLogGateMask must be of the form 2^n - 1");

			static PerfCounters& get() noexcept
			{
				static PerfCounters instance;
				return instance;
			}

			// Batched publish of thread-local hit count. Returns the
			// global value before the publish so callers can gate on the
			// log mask without a second atomic load.
			std::uint64_t publishHits(std::uint32_t a_local) noexcept
			{
				return mapHits.fetch_add(a_local, std::memory_order_relaxed);
			}

			void maybeLog() noexcept
			{
				LARGE_INTEGER now;
				::QueryPerformanceCounter(&now);

				auto prev = lastLogTime.load(std::memory_order_relaxed);
				if (prev == 0) {
					lastLogTime.store(now.QuadPart, std::memory_order_relaxed);
					return;
				}

				LARGE_INTEGER freq;
				::QueryPerformanceFrequency(&freq);
				const double elapsed =
					static_cast<double>(now.QuadPart - prev) /
					static_cast<double>(freq.QuadPart);
				if (elapsed < kLogIntervalSeconds) {
					return;
				}

				if (!lastLogTime.compare_exchange_strong(
						prev, now.QuadPart, std::memory_order_relaxed)) {
					return;
				}

				const auto hits   = mapHits.exchange(0, std::memory_order_relaxed);
				const auto misses = mapMisses.exchange(0, std::memory_order_relaxed);
				const auto ghost  = ghostMisses.exchange(0, std::memory_order_relaxed);
				const auto real   = realMisses.exchange(0, std::memory_order_relaxed);
				const auto total  = hits + misses;

				if (total == 0) {
					return;
				}

				const double hitPct =
					100.0 * static_cast<double>(hits) / static_cast<double>(total);

				logger::info(
					"[BSPreCulledObjectsVR] Get3DForID last {:.0f}s: {} lookups, {:.1f}% hit | {} misses | sentinel: {} ghost, {} real"sv,
					elapsed, total, hitPct, misses, ghost, real);
			}
		};

		inline RE::NiAVObject* HookedGet3DForID(std::uint32_t a_formID)
		{
			auto& handler = IDTo3DHandler::GetSingleton();

			if (!handler.UsePreCulledObjects()) {
				return OriginalGet3DForID(a_formID);
			}

			// Scope the shared_lock to just the map lookup. Everything
			// after this block (miss-path bookkeeping, sentinel orig
			// call, logging) runs without holding the reader lock so
			// concurrent writers aren't blocked unnecessarily.
			RE::NiAVObject* obj = nullptr;
			{
				auto token = handler.SharedAccess();
				obj = token->find_one(a_formID);
				// SharedAccessor destructor releases the lock at scope end.
			}

			auto& counters = PerfCounters::get();

			if (obj) {
				// Thread-local hit aggregation. Only one-in-kBatchSize
				// calls reaches into the global atomic, cutting cross-
				// core cacheline traffic on the hot path.
				thread_local std::uint32_t tlsHits = 0;
				if (++tlsHits >= PerfCounters::kBatchSize) {
					const auto prevHits = counters.publishHits(tlsHits);
					tlsHits = 0;
					if ((prevHits & PerfCounters::kLogGateMask) == 0) [[unlikely]] {
						counters.maybeLog();
					}
				}
				return obj;
			}

			// Map miss — fast path returns nullptr directly. Empirically
			// safe: ~35M miss samples across multiple testers produced
			// zero real holes (map is a strict superset of the stock
			// table). Every 1-in-4096 miss still calls orig as a
			// sentinel so regressions (new mods, load-order changes)
			// surface in the log as a non-zero real count.
			const auto prevMisses =
				counters.mapMisses.fetch_add(1, std::memory_order_relaxed);
			if ((prevMisses & PerfCounters::kSentinelMask) == 0) [[unlikely]] {
				auto* origResult = OriginalGet3DForID(a_formID);
				if (origResult) {
					counters.realMisses.fetch_add(1, std::memory_order_relaxed);
				} else {
					counters.ghostMisses.fetch_add(1, std::memory_order_relaxed);
				}
				if ((prevMisses & PerfCounters::kLogGateMask) == 0) [[unlikely]] {
					counters.maybeLog();
				}
				return origResult;
			}

			if ((prevMisses & PerfCounters::kLogGateMask) == 0) [[unlikely]] {
				counters.maybeLog();
			}
			return nullptr;
		}

		// ----------------------------------------------------------------
		// Install helpers.
		// ----------------------------------------------------------------

		inline void WriteAddToRainCullingGroup()
		{
			const auto     target = REL::Offset(0x27E0F60).address();
			constexpr auto size   = 0x24C;
			REL::safe_fill(target, REL::INT3, size);
			stl::asm_jump(target, size, reinterpret_cast<std::uintptr_t>(AddToRainCullingGroup));
		}

		inline void WriteAddToShadowCullingGroup()
		{
			const auto     target = REL::Offset(0x27E12C0).address();
			constexpr auto size   = 0x21B;
			REL::safe_fill(target, REL::INT3, size);
			stl::asm_jump(target, size, reinterpret_cast<std::uintptr_t>(AddToShadowCullingGroup));
		}

		// UpdateIDto3DMap: function-entry hook that must preserve the
		// rest of the function body (tail-call sub-entries exist at
		// target+N for N > 5). We can only clobber the first 5 bytes
		// (the original prologue `48 89 5C 24 08` = mov [rsp+8], rbx).
		//
		// CommonLibF4's write_branch<5> is a CALL-SITE hook (it decodes
		// a 5-byte E8 rel32 to compute the original target). Using it
		// on a function entry produces a garbage OriginalFn pointer, so
		// we build our own trampoline buffer: [saved prologue bytes] +
		// [FF 25 00 00 00 00 / addr] then install the forward jmp.
		inline void WriteUpdateIDto3DMap()
		{
			constexpr std::size_t kPrologueSize = 5;
			constexpr std::size_t kBufferSize   = kPrologueSize + 6 + 8;

			const auto target = REL::Offset(0x27DF7B0).address();

			auto& trampoline = F4SE::GetTrampoline();
			auto* buffer     = static_cast<std::uint8_t*>(trampoline.allocate(kBufferSize));

			std::memcpy(buffer, reinterpret_cast<const void*>(target), kPrologueSize);

			// FF 25 00 00 00 00        jmp qword ptr [rip + 0]
			// <8-byte absolute addr>
			buffer[kPrologueSize + 0] = 0xFF;
			buffer[kPrologueSize + 1] = 0x25;
			buffer[kPrologueSize + 2] = 0x00;
			buffer[kPrologueSize + 3] = 0x00;
			buffer[kPrologueSize + 4] = 0x00;
			buffer[kPrologueSize + 5] = 0x00;
			const std::uint64_t retAddr = target + kPrologueSize;
			std::memcpy(buffer + kPrologueSize + 6, &retAddr, sizeof(retAddr));

			(void)trampoline.write_branch<5>(
				target,
				reinterpret_cast<std::uintptr_t>(UpdateIDto3DMap));

			OriginalUpdateIDto3DMap = reinterpret_cast<UpdateIDto3DMap_t*>(buffer);
		}

		inline void WriteGet3DForIDCallHook()
		{
			const auto callSite = REL::Offset(0x27E0675).address();
			auto&      trampoline = F4SE::GetTrampoline();
			OriginalGet3DForID =
				reinterpret_cast<Get3DForID_t*>(
					trampoline.write_call<5>(
						callSite,
						reinterpret_cast<std::uintptr_t>(HookedGet3DForID)));
		}
	}

	inline void Install()
	{
		detail::WriteAddToRainCullingGroup();
		detail::WriteAddToShadowCullingGroup();
		detail::WriteUpdateIDto3DMap();
		detail::WriteGet3DForIDCallHook();

		logger::info("installed BSPreCulledObjects VR patch"sv);
	}
}
