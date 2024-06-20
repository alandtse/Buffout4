#pragma once

namespace Patches::MaxStdIOPatch
{
	inline void Install()
	{
		const auto handle = WinAPI::GetModuleHandle(L"msvcr110.dll");
		const auto proc =
			handle ?
				reinterpret_cast<decltype(&_setmaxstdio)>(WinAPI::GetProcAddress(handle, "_setmaxstdio")) :
				nullptr;
		if (proc != nullptr) {
			const auto result = proc(static_cast<int>(*Settings::MaxStdIO));
			logger::info("set max stdio to {}"sv, result);
		} else {
			logger::error("failed to install MaxStdIO patch"sv);
		}
	}
}
