// The one place the version is set: the exe's version resource and the About card read it.
#pragma once
#define CPS_VER_MAJOR 1
#define CPS_VER_MINOR 0
#define CPS_VER_PATCH 3

#define CPS_STR2(x) #x
#define CPS_STR(x) CPS_STR2(x)
#define CPS_VERSION_A CPS_STR(CPS_VER_MAJOR) "." CPS_STR(CPS_VER_MINOR) "." CPS_STR(CPS_VER_PATCH)
#define CPS_WIDE2(x) L##x
#define CPS_WIDE(x) CPS_WIDE2(x)
#define CPS_VERSION_W CPS_WIDE(CPS_VERSION_A)
