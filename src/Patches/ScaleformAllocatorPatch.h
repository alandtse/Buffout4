#pragma once

#include "Allocator.h"

namespace Patches::ScaleformAllocatorPatch
{
	namespace detail
	{
		class Allocator final :
			public RE::Scaleform::SysAlloc
		{
		public:
			[[nodiscard]] static Allocator* GetSingleton()
			{
				static Allocator singleton;
				return std::addressof(singleton);
			}

		protected:
			void* Alloc(std::size_t a_size, std::size_t a_align) override
			{
				return a_size > 0 ?
				           _heap.aligned_alloc(a_align, a_size) :
				           nullptr;
			}

			void Free(void* a_ptr, std::size_t, std::size_t) override
			{
				_heap.aligned_free(a_ptr);
			}

			void* Realloc(void* a_oldPtr, std::size_t, std::size_t a_newSize, std::size_t a_align) override
			{
				return _heap.aligned_realloc(a_align, a_oldPtr, a_newSize);
			}

		private:
			using Allocate_t = void*(RE::MemoryManager&, std::size_t, std::uint32_t, bool);
			using Deallocate_t = void(RE::MemoryManager&, void*, bool);
			using Reallocate_t = void*(RE::MemoryManager&, void*, std::size_t, std::uint32_t, bool);

			Allocator() = default;
			Allocator(const Allocator&) = delete;
			Allocator(Allocator&&) = delete;
			~Allocator() = default;
			Allocator& operator=(const Allocator&) = delete;
			Allocator& operator=(Allocator&&) = delete;

			::Allocator::GameHeap _heap;
		};

		struct Init
		{
			static void thunk(const RE::Scaleform::MemoryHeap::HeapDesc& a_rootHeapDesc, RE::Scaleform::SysAllocBase*)
			{
				func(a_rootHeapDesc, Allocator::GetSingleton());
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};
	}

	inline void Install()
	{
		REL::Relocation<std::uintptr_t> target{ REL::ID(903830), 0xEC };
		stl::write_thunk_call<5, detail::Init>(target.address());
		logger::info("installed ScaleformAllocator patch"sv);
	}
}
