#pragma once

namespace Fixes::UtilityShaderFix
{
	namespace detail
	{
		inline void CreateShaders()  // inlined in NG to 141cd3017
		{
			REL::Relocation<void()> func{ REL::ID(527640) };
			func();
		}

		void PatchPixelShader(std::uintptr_t a_base);
		void PatchVertexShader(std::uintptr_t a_base);
	}

	inline void Install()
	{
		REL::Relocation<std::uintptr_t> base{ REL::RelocationID(768994, 2319078) };
		detail::CreateShaders();
		detail::PatchPixelShader(base.address());
		detail::PatchVertexShader(base.address());
		logger::info("installed UtilityShader fix"sv);
	}
}
