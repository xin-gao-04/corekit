#pragma once

#if defined(_WIN32)
#if defined(LOGKIT_BUILD_DLL)
#define LOGKIT_API __declspec(dllexport)
#elif defined(LOGKIT_USE_DLL)
#define LOGKIT_API __declspec(dllimport)
#else
#define LOGKIT_API
#endif
#else
#define LOGKIT_API
#endif
