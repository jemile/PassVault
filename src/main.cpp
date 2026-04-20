// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  main.cpp  -  Runs PassVault
// ============================================================

#include "frame.h"

#ifdef _WIN32
#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
#endif

int main()
{
	FRAME::SetupFrame();
	FRAME::UnloadFrame();

	return 0;
}
 