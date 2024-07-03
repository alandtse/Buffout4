#pragma once

namespace Patches::MaxStdIOPatch
{
	inline void Install()
	{
		const auto handle = REX::W32::GetModuleHandleW(L"msvcr110.dll");
		const auto proc =
			handle ?
				reinterpret_cast<decltype(&_setmaxstdio)>(REX::W32::GetProcAddress(handle, "_setmaxstdio")) :
				nullptr;
		if (proc != nullptr) {
			const auto result = proc(static_cast<int>(*Settings::MaxStdIO));
			logger::info("set max stdio to {}"sv, result);
		} else {
			logger::error("failed to install MaxStdIO patch"sv);
		}
	}
}
