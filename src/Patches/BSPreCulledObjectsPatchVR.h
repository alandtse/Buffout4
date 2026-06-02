#pragma once

namespace Patches::BSPreCulledObjectsPatchVR
{
	// VR port of Buffout4's BSPreCulledObjectsPatch. The PC version
	// assumes a hash table layout and culling-call structure that VR
	// does not match. Specifically:
	//
	//   * VR inlines the BSTObjectArena iterator with raw-offset field
	//     access (no private BSTObjectArenaScrapAllocBase base).
	//   * VR has no separate PreCulledRainIDs / PreCulledShadowIDs arrays;
	//     object records carry their own IDs.
	//   * VR's main BSCullingGroup::Add path still queries the stock
	//     IDTo3D hash table (at 0x14391D880) via a Get3DForID call inside
	//     the AddObjectByID worker. We can't overwrite that path outright,
	//     so we hook the call site and keep the stock hash table
	//     populated via a trampoline on UpdateIDto3DMap.
	//
	// Implementation lives in BSPreCulledObjectsPatchVR.cpp. All VR
	// addresses verified via Ghidra against Fallout4VR.exe 1.2.72.0.
	void Install();
}
