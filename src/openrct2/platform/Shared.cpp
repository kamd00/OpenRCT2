/*****************************************************************************
 * Copyright (c) 2014-2018 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "../common.h"

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <unistd.h>
#endif

#include <stdlib.h>
#include <time.h>
#include "../Context.h"
#include "../OpenRCT2.h"
#include "../config/Config.h"
#include "../drawing/Drawing.h"
#include "../drawing/LightFX.h"
#include "../Game.h"
#include "../localisation/Currency.h"
#include "../localisation/Localisation.h"
#include "../util/Util.h"
#include "../world/Climate.h"
#include "platform.h"

#ifdef __APPLE__
    #include <mach/mach_time.h>
    #include <AvailabilityMacros.h>
    #ifndef __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
        #error Missing __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ define
    #endif
#endif

#if defined(__APPLE__) && (__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 101200)
    static mach_timebase_info_data_t _mach_base_info = {};
#endif

#if !((defined (_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200809L) || (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 700) || (defined(__APPLE__) && defined(__MACH__)))
char * strndup(const char * src, size_t size)
{
    size_t len = strnlen(src, size);
    char * dst = (char *)malloc(len + 1);

    if (dst == nullptr)
    {
        return nullptr;
    }

    dst      = (char *)memcpy(dst, src, len);
    dst[len] = '\0';
    return dst;
}
#endif // !((defined (_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200809L) || (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 700) || (defined(__APPLE__) && defined(__MACH__)))

#ifdef _WIN32
static uint32        _frequency = 0;
static LARGE_INTEGER _entryTimestamp;
#endif

using update_palette_func = void (*)(const uint8 *, sint32, sint32);

rct_palette_entry gPalette[256];

void platform_update_palette(const uint8 * colours, sint32 start_index, sint32 num_colours)
{
    colours += start_index * 4;

    for (sint32 i = start_index; i < num_colours + start_index; i++)
    {
        uint8 r = colours[2];
        uint8 g = colours[1];
        uint8 b = colours[0];

#ifdef __ENABLE_LIGHTFX__
        if (lightfx_is_available())
        {
            lightfx_apply_palette_filter(i, &r, &g, &b);
        }
        else
#endif
        {
            float night = gDayNightCycle;
            if (night >= 0 && gClimateLightningFlash == 0)
            {
                r = lerp(r, soft_light(r, 8), night);
                g = lerp(g, soft_light(g, 8), night);
                b = lerp(b, soft_light(b, 128), night);
            }
        }

        gPalette[i].red   = r;
        gPalette[i].green = g;
        gPalette[i].blue  = b;
        gPalette[i].alpha = 0;
        colours += 4;
    }

    // Fix #1749 and #6535: rainbow path, donut shop and pause button contain black spots that should be white.
    gPalette[255].alpha = 0;
    gPalette[255].red   = 255;
    gPalette[255].green = 255;
    gPalette[255].blue  = 255;

    if (!gOpenRCT2Headless)
    {
        drawing_engine_set_palette(gPalette);
    }
}

void platform_toggle_windowed_mode()
{
    sint32 targetMode = gConfigGeneral.fullscreen_mode == 0 ? 2 : 0;
    context_set_fullscreen_mode(targetMode);
    gConfigGeneral.fullscreen_mode = targetMode;
    config_save_default();
}

void platform_refresh_video(bool recreate_window)
{
    if (recreate_window)
    {
        context_recreate_window();
    }
    else
    {
        drawing_engine_dispose();
        drawing_engine_init();
        drawing_engine_resize();
    }

    drawing_engine_set_palette(gPalette);
    gfx_invalidate_screen();
}

static void platform_ticks_init()
{
#ifdef _WIN32
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    _frequency = (uint32)(freq.QuadPart / 1000);
    QueryPerformanceCounter(&_entryTimestamp);
#endif
}

uint32 platform_get_ticks()
{
#ifdef _WIN32
    LARGE_INTEGER pfc;
    QueryPerformanceCounter(&pfc);

    LARGE_INTEGER runningDelta;
    runningDelta.QuadPart = pfc.QuadPart - _entryTimestamp.QuadPart;

    return (uint32)(runningDelta.QuadPart / _frequency);
#elif defined(__APPLE__) && (__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 101200)
    return (uint32)(((mach_absolute_time() * _mach_base_info.numer) / _mach_base_info.denom) / 1000000);
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        log_fatal("clock_gettime failed");
        exit(-1);
    }
    return (uint32)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#endif
}

void platform_sleep(uint32 ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

uint8 platform_get_currency_value(const char * currCode)
{
    if (currCode == nullptr || strlen(currCode) < 3)
    {
        return CURRENCY_POUNDS;
    }

    for (sint32 currency = 0; currency < CURRENCY_END; ++currency)
    {
        if (strncmp(currCode, CurrencyDescriptors[currency].isoCode, 3) == 0)
        {
            return currency;
        }
    }

    return CURRENCY_POUNDS;
}

#ifndef __ANDROID__
float platform_get_default_scale()
{
    return 1;
}
#endif

void core_init()
{
    static bool initialised = false;
    if (!initialised)
    {
        initialised = true;

        platform_ticks_init();
        bitcount_init();
        mask_init();

#if defined(__APPLE__) && (__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 101200)
        kern_return_t ret = mach_timebase_info(&_mach_base_info);
        if (ret != 0)
        {
            log_fatal("Unable to get mach_timebase_info.");
            exit(-1);
        }
#endif
    }
}
