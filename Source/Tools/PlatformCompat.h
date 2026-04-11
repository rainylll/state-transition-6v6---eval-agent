#pragma once

#include <cerrno>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <ctime>

#if defined(_WIN32)
#include <windows.h>
#endif

#ifndef SEA_AIR_ENABLE_TACVIEW_SERVER
#define SEA_AIR_ENABLE_TACVIEW_SERVER 0
#endif

#ifndef SEA_AIR_ENABLE_WGUA
#define SEA_AIR_ENABLE_WGUA 0
#endif

#ifndef SEA_AIR_ENABLE_JOYSTICK
#define SEA_AIR_ENABLE_JOYSTICK 0
#endif

#ifndef SEA_AIR_ENABLE_ACMI_FILE
#define SEA_AIR_ENABLE_ACMI_FILE 1
#endif

#if defined(_WIN32)
#define SEA_AIR_API __declspec(dllexport)
#define SEA_AIR_IMPORT __declspec(dllimport)
#else
#define SEA_AIR_API
#define SEA_AIR_IMPORT

using DWORD = std::uint32_t;
using errno_t = int;

#ifndef __stdcall
#define __stdcall
#endif

inline int strcpy_s(char* dest, std::size_t destsz, const char* src)
{
    if (dest == nullptr || destsz == 0) {
        return EINVAL;
    }
    if (src == nullptr) {
        dest[0] = '\0';
        return EINVAL;
    }
    std::snprintf(dest, destsz, "%s", src);
    return 0;
}

inline int sprintf_s(char* buffer, std::size_t sizeOfBuffer, const char* format, ...)
{
    if (buffer == nullptr || sizeOfBuffer == 0 || format == nullptr) {
        return -1;
    }

    va_list args;
    va_start(args, format);
    const int written = std::vsnprintf(buffer, sizeOfBuffer, format, args);
    va_end(args);
    return written;
}

inline int fopen_s(FILE** file, const char* filename, const char* mode)
{
    if (file == nullptr) {
        return EINVAL;
    }
    *file = std::fopen(filename, mode);
    return *file == nullptr ? errno : 0;
}

inline std::size_t lstrlenA(const char* text)
{
    return text == nullptr ? 0U : std::strlen(text);
}
#endif

inline std::tm* sea_air_gmtime(const std::time_t* time_value, std::tm* out_tm)
{
#if defined(_WIN32)
    return (gmtime_s(out_tm, time_value) == 0) ? out_tm : nullptr;
#else
    return gmtime_r(time_value, out_tm);
#endif
}

inline std::uint64_t sea_air_tick_count64()
{
#if defined(_WIN32)
    return static_cast<std::uint64_t>(::GetTickCount64());
#else
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count()
    );
#endif
}
