#pragma once

#include "Allocator.h"

namespace Patches::MemoryManagerPatch
{
	namespace detail
	{
		class MemoryTraces
		{
		public:
			using lock_type = std::mutex;
			using value_type = robin_hood::unordered_flat_map<void*, std::pair<bool, boost::stacktrace::stacktrace>>;

			class Accessor
			{
			public:
				Accessor() = delete;
				Accessor(const Accessor&) = delete;
				Accessor(Accessor&&) = delete;

				~Accessor() = default;

				Accessor& operator=(const Accessor&) = delete;
				Accessor& operator=(Accessor&&) = delete;

				[[nodiscard]] value_type& operator*() noexcept { return get(); }
				[[nodiscard]] const value_type& operator*() const noexcept { return get(); }

				[[nodiscard]] value_type* operator->() noexcept { return std::addressof(get()); }
				[[nodiscard]] const value_type* operator->() const noexcept { return std::addressof(get()); }

				[[nodiscard]] value_type& get() noexcept { return _proxy; }
				[[nodiscard]] const value_type& get() const noexcept { return _proxy; }

			protected:
				friend class MemoryTraces;

				Accessor(lock_type& a_lock, value_type& a_proxy) :
					_locker(a_lock),
					_proxy(a_proxy)
				{}

			private:
				std::scoped_lock<lock_type> _locker;
				value_type& _proxy;
			};

			[[nodiscard]] static MemoryTraces& get() noexcept
			{
				static MemoryTraces singleton;
				return singleton;
			}

			[[nodiscard]] Accessor access() { return { _lock, _traces }; }

		private:
			MemoryTraces() noexcept = default;
			MemoryTraces(const MemoryTraces&) = delete;
			MemoryTraces(MemoryTraces&&) = delete;

			~MemoryTraces() noexcept = default;

			MemoryTraces& operator=(const MemoryTraces&) = delete;
			MemoryTraces& operator=(MemoryTraces&&) = delete;

			lock_type _lock;
			value_type _traces;
		};

		namespace AutoScrapBuffer
		{
			inline void CtorLong()
			{
				REL::Relocation<std::uintptr_t> target{ REL::ID(1305199), 0x1D };
				constexpr std::size_t size = 0x15;
				REL::safe_fill(target.address(), REL::NOP, size);
			}

			void CtorShort();
			void Dtor();

			inline void Install()
			{
				CtorLong();
				CtorShort();
				Dtor();
			}
		}

		namespace MemoryManager
		{
			inline void* Allocate(RE::MemoryManager*, std::size_t a_size, std::uint32_t a_alignment, bool a_alignmentRequired)
			{
				if (a_size > 0) {
					auto& heap = Allocator::ProxyHeap::get();
					return a_alignmentRequired ?
					           heap.aligned_alloc(a_alignment, a_size) :
					           heap.malloc(a_size);
				} else {
					return nullptr;
				}
			}

			inline void* DbgAllocate(RE::MemoryManager* a_this, std::size_t a_size, std::uint32_t a_alignment, bool a_alignmentRequired)
			{
				const auto result = Allocate(a_this, a_size, a_alignment, a_alignmentRequired);
				if (result && a_size == sizeof(RE::NiNode)) {
					auto access = MemoryTraces::get().access();
					access->emplace(
						result,
						std::make_pair(a_alignmentRequired, boost::stacktrace::stacktrace{}));
				}
				return result;
			}

			inline void Deallocate(RE::MemoryManager*, void* a_mem, bool a_alignmentRequired)
			{
				auto& heap = Allocator::ProxyHeap::get();
				a_alignmentRequired ?
					heap.aligned_free(a_mem) :
					heap.free(a_mem);
			}

			void DbgDeallocate(RE::MemoryManager* a_this, void* a_mem, bool a_alignmentRequired);

			inline void* Reallocate(RE::MemoryManager*, void* a_oldMem, std::size_t a_newSize, std::uint32_t a_alignment, bool a_alignmentRequired)
			{
				auto& heap = Allocator::ProxyHeap::get();
				return a_alignmentRequired ?
				           heap.aligned_realloc(a_alignment, a_oldMem, a_newSize) :
				           heap.realloc(a_oldMem, a_newSize);
			}

			inline void* DbgReallocate(RE::MemoryManager* a_this, void* a_oldMem, std::size_t a_newSize, std::uint32_t a_alignment, bool a_alignmentRequired)
			{
				auto access = MemoryTraces::get().access();
				const auto result = Reallocate(a_this, a_oldMem, a_newSize, a_alignment, a_alignmentRequired);
				if (const auto it = access->find(a_oldMem); it != access->end()) {
					access->erase(it);
					access->emplace(
						result,
						std::make_pair(a_alignmentRequired, boost::stacktrace::stacktrace{}));
				}
				return result;
			}

			inline void ReplaceAllocRoutines()
			{
				using tuple_t = std::tuple<std::uint64_t, std::size_t, void*>;
				const std::array todo{
					tuple_t{ 652767, 0x24B, *Settings::MemoryManagerDebug ? &DbgAllocate : &Allocate },
					tuple_t{ 1582181, 0x115, *Settings::MemoryManagerDebug ? &DbgDeallocate : &Deallocate },
					tuple_t{ 1502917, 0xA2, *Settings::MemoryManagerDebug ? &DbgReallocate : &Reallocate },
				};

				for (const auto& [id, size, func] : todo) {
					REL::Relocation<std::uintptr_t> target{ REL::ID(id) };
					REL::safe_fill(target.address(), REL::INT3, size);
					stl::asm_jump(target.address(), size, reinterpret_cast<std::uintptr_t>(func));
				}
			}

			inline void StubInit()
			{
				REL::Relocation<std::uintptr_t> target{ REL::ID(597736) };
				REL::safe_fill(target.address(), REL::INT3, 0x9C);
				REL::safe_write(target.address(), REL::RET);

				REL::Relocation<std::uint32_t*> initFence{ REL::ID(1570354) };
				*initFence = 2;
			}

			inline void Install()
			{
				StubInit();
				ReplaceAllocRoutines();

				RE::MemoryManager::GetSingleton().RegisterMemoryManager();
				RE::BSThreadEvent::InitSDM();
			}
		}

		namespace ScrapHeap
		{
			inline void* Allocate(RE::ScrapHeap*, std::size_t a_size, std::size_t a_alignment)
			{
				auto& heap = Allocator::ProxyHeap::get();
				return a_size > 0 ?
				           heap.aligned_alloc(a_alignment, a_size) :
				           nullptr;
			}

			inline RE::ScrapHeap* Ctor(RE::ScrapHeap* a_this)
			{
				std::memset(a_this, 0, sizeof(RE::ScrapHeap));
				stl::emplace_vtable(a_this);
				return a_this;
			}

			inline void Deallocate(RE::ScrapHeap*, void* a_mem)
			{
				auto& heap = Allocator::ProxyHeap::get();
				heap.aligned_free(a_mem);
			}

			inline void WriteHooks()
			{
				using tuple_t = std::tuple<std::uint64_t, std::size_t, void*>;
				const std::array todo{
					tuple_t{ 1085394, 0x5F6, &Allocate },
					tuple_t{ 923307, 0x144, &Deallocate },
					tuple_t{ 48809, 0x12B, &Ctor },
				};

				for (const auto& [id, size, func] : todo) {
					REL::Relocation<std::uintptr_t> target{ REL::ID(id) };
					REL::safe_fill(target.address(), REL::INT3, size);
					stl::asm_jump(target.address(), size, reinterpret_cast<std::uintptr_t>(func));
				}
			}

			inline void WriteStubs()
			{
				using tuple_t = std::tuple<std::uint64_t, std::size_t>;
				const std::array todo{
					tuple_t{ 550677, 0xC3 },  // Clean
					tuple_t{ 111657, 0x8 },   // ClearKeepPages
					tuple_t{ 975239, 0xF6 },  // InsertFreeBlock
					tuple_t{ 84225, 0x183 },  // RemoveFreeBlock
					tuple_t{ 1255203, 0x4 },  // SetKeepPages
					tuple_t{ 912706, 0x32 },  // dtor
				};

				for (const auto& [id, size] : todo) {
					REL::Relocation<std::uintptr_t> target{ REL::ID(id) };
					REL::safe_fill(target.address(), REL::INT3, size);
					REL::safe_write(target.address(), REL::RET);
				}
			}

			inline void Install()
			{
				WriteStubs();
				WriteHooks();
			}
		}
	}

	inline void Install()
	{
		detail::AutoScrapBuffer::Install();
		detail::MemoryManager::Install();
		detail::ScrapHeap::Install();
		logger::info("installed MemoryManager patch"sv);
	}
}
