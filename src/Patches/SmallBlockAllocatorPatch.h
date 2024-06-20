#pragma once

#include "Allocator.h"

namespace Patches::SmallBlockAllocatorPatch
{
	namespace detail
	{
		void InstallAllocations();
		void InstallDeallocations();

		inline void* Allocate(std::size_t a_size)
		{
			auto& heap = Allocator::ProxyHeap::get();
			return a_size > 0 ?
			           heap.aligned_alloc(0x10, a_size) :
			           nullptr;
		}

		inline void Deallocate(void* a_ptr)
		{
			auto& heap = Allocator::ProxyHeap::get();
			heap.aligned_free(a_ptr);
		}
	}

	inline void Install()
	{
		detail::InstallAllocations();
		detail::InstallDeallocations();

		REL::Relocation<std::uintptr_t> target{ REL::ID(329149), 0x48 };
		REL::safe_fill(target.address(), REL::NOP, 0x5);

		logger::info("installed SmallBlockAllocator patch"sv);
	}
}
