#include "Fixes/CreateD3DAndSwapChainFix.h"

#include <dxgi.h>
namespace CreateD3DAndSwapChainFix
{
	namespace detail
	{
		::HRESULT GetDisplayModeList(
			::IDXGIOutput& a_this,
			::DXGI_FORMAT a_enumFormat,
			::UINT a_flags,
			::UINT* a_numModes,
			::DXGI_MODE_DESC* a_desc)
		{
			const auto result = a_this.GetDisplayModeList(a_enumFormat, a_flags, a_numModes, a_desc);

			const auto modes = std::span(a_desc, *a_numModes);
			const auto end = std::stable_partition(
				modes.begin(),
				modes.end(),
				[](const ::DXGI_MODE_DESC& a_desc) {
					return a_desc.RefreshRate.Denominator != 0;
				});
			*a_numModes = static_cast<::UINT>(end - modes.begin());

			return result;
		}
	}

	void Install()
	{
		const auto target = REL::Relocation<std::uintptr_t>(REL::ID(224250), REL::Relocate(0x114, 0x114, 0x11F)).address();
		auto& trampoline = F4SE::GetTrampoline();
		trampoline.write_call<5>(target, detail::GetDisplayModeList);

		logger::info("installed CreateD3DAndSwapChain fix"sv);
	}
}
