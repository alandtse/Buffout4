#pragma once

#include "Allocator.h"

namespace Patches::HavokMemorySystemPatch
{
	namespace detail
	{
		class hkMemoryAllocator final :
			public RE::hkMemoryAllocator
		{
		public:
			void* BlockAlloc(std::int32_t a_numBytesIn) override
			{
				return a_numBytesIn > 0 ?
				           _heap.aligned_alloc(0x10, a_numBytesIn) :
				           nullptr;
			}

			void BlockFree(void* a_ptr, std::int32_t) override
			{
				_heap.aligned_free(a_ptr);
			}

			void* BufAlloc(std::int32_t& a_reqNumBytesInOut) override
			{
				return a_reqNumBytesInOut > 0 ?
				           _heap.aligned_alloc(0x10, a_reqNumBytesInOut) :
				           nullptr;
			}

			void BufFree(void* a_ptr, std::int32_t) override
			{
				_heap.aligned_free(a_ptr);
			}

			void* BufRealloc(void* a_old, std::int32_t, std::int32_t& a_reqNumBytesInOut) override
			{
				return _heap.aligned_realloc(0x10, a_old, a_reqNumBytesInOut);
			}

			void BlockAllocBatch(void** a_ptrsOut, std::int32_t a_numPtrs, std::int32_t a_blockSize) override
			{
				std::span range{ a_ptrsOut, static_cast<std::size_t>(a_numPtrs) };
				std::for_each(
					range.begin(),
					range.end(),
					[&](void*& a_elem) {
						a_elem =
							a_blockSize > 0 ?
								_heap.aligned_alloc(0x10, a_blockSize) :
								nullptr;
					});
			}

			void BlockFreeBatch(void** a_ptrsIn, std::int32_t a_numPtrs, std::int32_t) override
			{
				std::span range{ a_ptrsIn, static_cast<std::size_t>(a_numPtrs) };
				std::for_each(
					range.begin(),
					range.end(),
					[&](void* a_elem) {
						_heap.aligned_free(a_elem);
					});
			}

			void GetMemoryStatistics(MemoryStatistics&) const override { return; }
			std::int32_t GetAllocatedSize(const void*, std::int32_t a_numBytes) const override { return a_numBytes; }

		private:
			Allocator::GameHeap _heap;
		};

		class hkMemorySystem final :
			public RE::hkMemorySystem
		{
		public:
			using FlagBits = RE::hkMemorySystem::FlagBits;

			[[nodiscard]] static hkMemorySystem* GetSingleton()
			{
				static hkMemorySystem singleton;
				return std::addressof(singleton);
			}

			RE::hkMemoryRouter* MainInit(
				const FrameInfo&,
				RE::hkFlags<FlagBits, std::int32_t> a_flags) override
			{
				ThreadInit(_router, "main", a_flags);
				return std::addressof(_router);
			}

			RE::hkResult MainQuit(RE::hkFlags<FlagBits, std::int32_t> a_flags) override
			{
				ThreadQuit(_router, a_flags);
				return { RE::hkResultEnum::kSuccess };
			}

			void ThreadInit(
				RE::hkMemoryRouter& a_router,
				const char*,
				RE::hkFlags<FlagBits, std::int32_t> a_flags) override
			{
				const auto allocator = std::addressof(_allocator);

				if (a_flags & FlagBits::kPersistent) {
					a_router.SetHeap(allocator);
					a_router.SetDebug(allocator);
					a_router.SetTemp(nullptr);
					a_router.SetSolver(nullptr);
				}

				if (a_flags & FlagBits::kTemporary) {
					a_router.GetStack().Init(
						allocator,
						allocator,
						allocator);
					a_router.SetTemp(allocator);
					a_router.SetSolver(allocator);
				}
			}

			void ThreadQuit(
				RE::hkMemoryRouter& a_router,
				RE::hkFlags<FlagBits, std::int32_t> a_flags) override
			{
				if (a_flags & FlagBits::kTemporary) {
					a_router.GetStack().Quit(nullptr);
				}

				if (a_flags & FlagBits::kPersistent) {
					std::memset(
						std::addressof(a_router),
						0,
						sizeof(a_router));
				}

				GarbageCollectThread(a_router);
			}

			virtual void PrintStatistics(RE::hkOstream&) const { return; }

			void GetMemoryStatistics(RE::hkMemorySystem::MemoryStatistics&) { return; }

			RE::hkMemoryAllocator* GetUncachedLockedHeapAllocator() override { return std::addressof(_allocator); }

		private:
			hkMemorySystem() = default;
			hkMemorySystem(const hkMemorySystem&) = delete;
			hkMemorySystem(hkMemorySystem&&) = delete;

			~hkMemorySystem() = default;

			hkMemorySystem& operator=(const hkMemorySystem&) = delete;
			hkMemorySystem& operator=(hkMemorySystem&&) = delete;

			alignas(0x10) hkMemoryAllocator _allocator;
			alignas(0x10) RE::hkMemoryRouter _router;
		};
	}

	inline void Install()
	{
		auto& trampoline = F4SE::GetTrampoline();
		REL::Relocation<std::uintptr_t> target{ REL::RelocationID(204659, 2277714), 0x68 };
		trampoline.write_call<5>(target.address(), detail::hkMemorySystem::GetSingleton);
		logger::info("installed HavokMemorySystem patch"sv);
	}
}
