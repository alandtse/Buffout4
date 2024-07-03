#pragma once

#include "hash.h"
#undef min
#undef max

namespace Compatibility::F4EE
{
	namespace detail
	{
		inline void CopyMorphs(const float a_src[], std::size_t a_size, RE::BSTArray<float>& a_dst)
		{
			constexpr std::size_t max = 5;
			a_dst.resize(max);
			std::fill(a_dst.begin(), a_dst.end(), 0.0F);
			for (std::size_t i = 0; i < std::min(a_size, max); ++i) {
				a_dst[static_cast<std::uint32_t>(i)] = a_src[i];
			}
		}

		inline RE::BSTArray<float>* CreateMorphs()
		{
			return new RE::BSTArray<float>{ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
		}

		void SimpleInlinePatch(std::uintptr_t a_dst, std::size_t a_size, std::uintptr_t a_func);
		void SetMorphValues(std::uintptr_t a_base);
	}

	inline void Install()
	{
		const auto validate = []() {
			// 1.6.20
			constexpr auto precalc = "D5467FEA7D6A722E0ED87B5FB857B5E0C9FDBE57B060ADB711013ED27565688D3D4514F99E4DF02109273ED69330E5272AD76822EB15717FEBC11AF409BA60F1"sv;

			mmio::mapped_file_source file;
			if (file.open("data/f4se/plugins/f4ee.dll")) {
				const auto hash = Hash::SHA512({ file.data(), file.size() });
				if (hash && precalc == *hash) {
					return true;
				} else {
					logger::error(
						"F4EE compatibility patch: mismatch on sha512\n"
						"\texpected \"{}\"\n"
						"\tfound \"{}\""sv,
						precalc,
						hash ? *hash : "FAILURE"sv);
				}
			}

			return false;
		};

		const auto handle = REX::W32::GetModuleHandleW(L"f4ee.dll");
		if (handle != nullptr && validate()) {
			const auto base = reinterpret_cast<std::uintptr_t>(handle);
			detail::SetMorphValues(base);
			logger::info("installed F4EE compatibility patch"sv);
		} else {
			logger::warn("failed to install F4EE compatibility patch"sv);
		}
	}
}
