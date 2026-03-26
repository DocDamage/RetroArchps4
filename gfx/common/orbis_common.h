#ifndef ORBIS_COMMON_H__
#define ORBIS_COMMON_H__

#ifdef HAVE_EGL
#include <piglet.h>
#include "../common/egl_common.h"
#endif

/* --- Output resolution ---
 * These are the native 1080p dimensions for all output modes.
 * orbis_ctx stores the actual runtime resolution separately so
 * that a future dynamic-resolution path can override these. */
#define ATTR_ORBISGL_WIDTH  1920
#define ATTR_ORBISGL_HEIGHT 1080

/* --- ScePglConfig memory budget ---
 * All values sourced from open PS4 homebrew SDK examples.
 * Adjust systemSharedMemorySize / videoSharedMemorySize upward
 * if running heavier cores that allocate large GL textures. */
#define ORBISGL_PGL_SYSTEM_SHARED_MEM   0x200000    /* 2  MB  system shared  */
#define ORBISGL_PGL_VIDEO_SHARED_MEM    0x2400000   /* 36 MB  video shared   */
#define ORBISGL_PGL_MAX_FLEXIBLE_MEM    0xAA00000   /* 170 MB flex memory    */
#define ORBISGL_PGL_DRAW_CMD_BUF        0xC0000     /* 768 KB draw cmd buf   */
#define ORBISGL_PGL_LCUE_RESOURCE_BUF   0x10000     /* 64  KB LCUE resource  */
#define ORBISGL_PGL_FLAGS \
   (SCE_PGL_FLAGS_USE_COMPOSITE_EXT | SCE_PGL_FLAGS_USE_FLEXIBLE_MEMORY | 0x60)

typedef struct
{
#ifdef HAVE_EGL
   egl_ctx_data_t egl;
   ScePglConfig   pgl_config;
#endif

   SceWindow native_window;
   bool  resize;
   /* Runtime resolution (defaults to ATTR_ORBISGL_{WIDTH,HEIGHT}) */
   unsigned width;
   unsigned height;
   float refresh_rate;

   /* Set true once scePigletSetConfigurationVSH has been called so that
    * a second context init in the same process does not call it again. */
   bool piglet_configured;
} orbis_ctx_data_t;

#endif
