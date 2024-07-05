#pragma once
// https://www.nexusmods.com/fallout4/mods/81859
// Adds support for the new BA2 archive format introduced in NG update 1.10.980 to versions 1.10.163 and older.
// Fixes crashes, similar to BEES for Skyrim Special Edition.
// Used with permission by Nukem.

namespace Fixes::BackportedBA2Support
{
	inline void Install()
	{
		REL::Relocation<uint8_t> versionPatch1{ REL::ID(1566228), 0x3DF };
		REL::safe_fill(versionPatch1.address(), 8, 1);

		REL::Relocation<uint8_t> versionPatch2{ REL::ID(624471), 0xBA };
		REL::safe_fill(versionPatch2.address(), 8, 1);

		logger::info("Patched BackportedBA2Support");
	}
}
