#pragma once

namespace Patches::MaxStdIOPatch
{
	inline void Install()
	{
		const auto handle = REX::W32::GetModuleHandleW(REL::Module::IsNG() ? L"api-ms-win-crt-runtime-l1-1-0.dll" : L"msvcr110.dll");
		const auto proc =
			handle ?
				reinterpret_cast<decltype(&_setmaxstdio)>(REX::W32::GetProcAddress(handle, "_setmaxstdio")) :
				nullptr;
		if (proc != nullptr) {
			const auto get = reinterpret_cast<decltype(&_getmaxstdio)>(REX::W32::GetProcAddress(handle, "_getmaxstdio"));
			const auto old = get();
			const auto result = proc(static_cast<int>(*Settings::MaxStdIO));
			if (get)
				logger::info("set max stdio to {} from {}"sv, result, old);
			else
				logger::info("set max stdio to {}"sv, result);
		} else {
			logger::error("failed to install MaxStdIO patch"sv);
		}
	}
}
