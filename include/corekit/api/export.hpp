#pragma once

#if defined(_WIN32)
#if defined(COREKIT_BUILD_DLL)
#define COREKIT_API __declspec(dllexport)
#elif defined(COREKIT_USE_DLL)
#define COREKIT_API __declspec(dllimport)
#else
#define COREKIT_API
#endif
#else
#define COREKIT_API
#endif

