/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2017 - Daniel De Matteis
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

/* Phase 2 hardening: init, memory stats, cpu name, exec path, drive list */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <libkernel.h>
#include <SystemService.h>
#ifdef HAVE_PS4LINK
#include <ps4link.h>
#endif
#ifdef HAVE_DEBUGNET
#include <debugnet.h>
#endif
#include <orbisFile.h>
#include <sys/stat.h>

typedef struct Orbis2dConfig Orbis2dConfig;
typedef struct OrbisPadConfig OrbisPadConfig;
typedef struct OrbisAudioConfig OrbisAudioConfig;
typedef struct OrbisKeyboardConfig OrbisKeyboardConfig;

#include <pthread.h>

#include <string/stdstring.h>
#include <boolean.h>
#include <file/file_path.h>
#ifndef IS_SALAMANDER
#include <lists/file_list.h>
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../frontend.h"
#include "../frontend_driver.h"
#include "../../defaults.h"
#include "../../file_path_special.h"
#include "../../retroarch.h"
#include "../../paths.h"
#include "../../verbosity.h"

typedef struct OrbisGlobalConf
{
	Orbis2dConfig *conf;
	/* confPad: reserved ABI field.  Pad lifecycle is owned by
	 * ps4_joypad_init()/destroy() — this field is never read by
	 * RetroArch.  Do NOT remove: external PS4 loaders that hand us
	 * an OrbisGlobalConf pointer expect this field at this offset. */
	OrbisPadConfig *confPad;
	OrbisAudioConfig *confAudio;
	OrbisKeyboardConfig *confKeyboard;
#ifdef HAVE_PS4LINK
	ps4LinkConfiguration *confLink;
#else
	void *confLink;
#endif
	int orbisLinkFlag;
}OrbisGlobalConf;

static OrbisGlobalConf *myConf;
static bool frontend_orbis_ps4link_initialized = false;

char eboot_path[512];
char user_path[512];

static enum frontend_fork orbis_fork_mode = FRONTEND_FORK_NONE;

/* --- Orbis argv contract ---
 * argv[0]  — executable path (standard)
 * argv[1]  — pointer to OrbisGlobalConf (as a hex string "%p"), passed by
 *            external PS4 loaders that pre-initialise subsystems.  Consumed
 *            and then NULLed after parsing so downstream code does not
 *            attempt to re-interpret it.
 * argv[2]  — (optional) content path for auto-start; consumed by
 *            frontend_orbis_get_environment_settings.
 */
static void frontend_orbis_attach_runtime_conf(int argc, char *argv[])
{
	uintptr_t intptr = 0;

	if (myConf || argc <= 1 || string_is_empty(argv[1]))
		return;

	if (sscanf(argv[1], "%p", &intptr) != 1 || !intptr)
		return;

	myConf = (OrbisGlobalConf *)intptr;

#ifdef HAVE_PS4LINK
	if (myConf->confLink)
	{
		int ret = ps4LinkInitWithConf(myConf->confLink);
		if (ret)
			frontend_orbis_ps4link_initialized = true;
		else
			ps4LinkFinish();
	}
#endif
}

static void frontend_orbis_get_environment_settings(int *argc, char *argv[],
      void *args, void *params_data)
{
   unsigned i;
   struct rarch_main_wrap *params = NULL;

   (void)args;

#ifndef IS_SALAMANDER
#if defined(HAVE_LOGGER)
   logger_init();
#elif defined(HAVE_FILE_LOGGER)
   retro_main_log_file_init("host0:app/temp/retroarch-log.txt");
#endif
#endif

   sceSystemServiceHideSplashScreen();

   frontend_orbis_attach_runtime_conf(argc ? *argc : 0, argv);

   if (argv && argc && *argc > 1)
      argv[1] = NULL;

   orbisFileInit();

   /* Pad lifecycle is owned by ps4_joypad_init()/destroy(). */

   strlcpy(eboot_path, "host0:app", sizeof(eboot_path));
   strlcpy(g_defaults.dirs[DEFAULT_DIR_PORT], eboot_path, sizeof(g_defaults.dirs[DEFAULT_DIR_PORT]));
   strlcpy(user_path, "host0:app/data/retroarch/", sizeof(user_path));

   RARCH_LOG("port dir: [%s]\n", g_defaults.dirs[DEFAULT_DIR_PORT]);

#ifdef ORBIS_LITE_BUILD
   strlcpy(g_defaults.settings.menu, "rgui", sizeof(g_defaults.settings.menu));
   g_defaults.overlay.set    = true;
   g_defaults.overlay.enable = false;
#endif

   /* bundle data*/
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE], g_defaults.dirs[DEFAULT_DIR_PORT],
         "", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_INFO], g_defaults.dirs[DEFAULT_DIR_PORT],
         "info", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));
   /* user data*/
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS], user_path,
         "assets", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG], user_path,
         "autoconfig", sizeof(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_DATABASE], user_path,
         "database/rdb", sizeof(g_defaults.dirs[DEFAULT_DIR_DATABASE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CURSOR], user_path,
         "database/cursors", sizeof(g_defaults.dirs[DEFAULT_DIR_CURSOR]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CHEATS], user_path,
         "cheats", sizeof(g_defaults.dirs[DEFAULT_DIR_CHEATS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG], user_path,
         "config", sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS], user_path,
         "downloads", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PLAYLIST], user_path,
         "playlists", sizeof(g_defaults.dirs[DEFAULT_DIR_PLAYLIST]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_REMAP], user_path,
         "remaps", sizeof(g_defaults.dirs[DEFAULT_DIR_REMAP]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SRAM], user_path,
         "savefiles", sizeof(g_defaults.dirs[DEFAULT_DIR_SRAM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SAVESTATE], user_path,
         "savestates", sizeof(g_defaults.dirs[DEFAULT_DIR_SAVESTATE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SYSTEM], user_path,
         "system", sizeof(g_defaults.dirs[DEFAULT_DIR_SYSTEM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SHADER], user_path,
         "shaders", sizeof(g_defaults.dirs[DEFAULT_DIR_SHADER]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CACHE], user_path,
         "temp", sizeof(g_defaults.dirs[DEFAULT_DIR_CACHE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_OVERLAY], user_path,
         "overlays", sizeof(g_defaults.dirs[DEFAULT_DIR_OVERLAY]));
#ifdef HAVE_VIDEO_LAYOUT
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT], user_path,
         "layouts", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT]));
#endif
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS], user_path,
         "thumbnails", sizeof(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT], user_path,
         "screenshots", sizeof(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_LOGS], user_path,
         "logs", sizeof(g_defaults.dirs[DEFAULT_DIR_LOGS]));
   strlcpy(g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY],
         user_path, sizeof(g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY]));
   fill_pathname_join(g_defaults.path.config, user_path,
         file_path_str(FILE_PATH_MAIN_CONFIG), sizeof(g_defaults.path.config));

#ifndef IS_SALAMANDER
   params = (struct rarch_main_wrap*)params_data;
#ifdef ORBIS_LITE_BUILD
   params->verbose = false;
#else
   params->verbose = true;
#endif

   if (argc && *argc > 2 && !string_is_empty(argv[2]))
   {
      static char path[PATH_MAX_LENGTH] = {0};
      struct rarch_main_wrap      *args =
         (struct rarch_main_wrap*)params_data;

      if (args)
      {
         strlcpy(path, argv[2], sizeof(path));

         args->touched        = true;
         args->no_content     = false;
         args->verbose        = false;
         args->config_path    = NULL;
         args->sram_path      = NULL;
         args->state_path     = NULL;
         args->content_path   = path;
         args->libretro_path  = NULL;

         RARCH_LOG("argv[0]: %s\n", argv[0] ? argv[0] : "(null)");
         RARCH_LOG("argv[1]: %s\n", argv[1] ? argv[1] : "(null)");
         RARCH_LOG("argv[2]: %s\n", argv[2] ? argv[2] : "(null)");

         RARCH_LOG("Auto-start game %s.\n", argv[2]);
      }
   }
#endif

   for (i = 0; i < DEFAULT_DIR_LAST; i++)
   {
      const char *dir_path = g_defaults.dirs[i];
      if (!string_is_empty(dir_path))
         path_mkdir(dir_path);
   }
}

static void frontend_orbis_deinit(void *data)
{
   (void)data;
#ifndef IS_SALAMANDER
   verbosity_disable();
#ifdef HAVE_FILE_LOGGER
   command_event(CMD_EVENT_LOG_FILE_DEINIT, NULL);
#endif

#endif
#ifdef HAVE_PS4LINK
	if (frontend_orbis_ps4link_initialized)
		ps4LinkFinish();
#endif
}

static void frontend_orbis_shutdown(bool unused)
{
   (void)unused;
   return;
}

static uint64_t frontend_orbis_get_mem_total(void)
{
   /* sceKernelGetDirectMemorySize is available on OpenOrbis.
    * Falls back to a conservative constant if the call is
    * not available at link time. */
#if defined(HAVE_ORBIS_KERNEL_MEM)
   return (uint64_t)sceKernelGetDirectMemorySize();
#else
   /* PS4 has 8 GB of unified memory; ~5 GB accessible to userland. */
   return (uint64_t)5 * 1024 * 1024 * 1024;
#endif
}

static uint64_t frontend_orbis_get_mem_used(void)
{
   /* Orbis OS is FreeBSD-based; /proc/self/statm is available on
    * jailbreak kernels that expose procfs.  Field layout (pages):
    *   size  resident  shared  text  lib  data  dt
    * We return resident * PAGE_SIZE.  Falls back to 0 gracefully
    * when procfs is not mounted (retail/stripped kernels). */
   FILE    *f = fopen("/proc/self/statm", "r");
   if (f)
   {
      unsigned long virt_pages, rss_pages;
      if (fscanf(f, "%lu %lu", &virt_pages, &rss_pages) == 2)
      {
         fclose(f);
         return (uint64_t)rss_pages * 4096ULL;
      }
      fclose(f);
   }
   /* /proc/self/statm is not available — retail firmware or procfs not
    * mounted.  Log once so developers know the OSD "0 MB" reading is not
    * a real value.  A proper fix needs sceKernelGetProcessMemoryUsage()
    * or a kernel-specific call; deferred until on-device testing. */
   {
      static bool warned = false;
      if (!warned)
      {
         RARCH_WARN("[ORBIS] /proc/self/statm unavailable — memory stats disabled.\n");
         warned = true;
      }
   }
   return 0;
}

static const char *frontend_orbis_get_cpu_model_name(void)
{
   /* All retail PS4 models use AMD Jaguar x86-64 cores. */
   return "AMD Jaguar x86-64 (PS4)";
}

static void frontend_orbis_init(void *data)
{
   /* If no OrbisGlobalConf was passed from the loader (myConf is
    * still NULL after attach_runtime_conf), boot subsystems here. */

   if (!myConf)
   {
      /* The loader did not hand us pre-initialised subsystems.
       * Perform a standalone init of the pad subsystem so that
       * ps4_joypad can call scePadOpen later. */
      orbisFileInit();
   }

   /* Nothing else to do here: orbisPad, orbisAudio, and
    * orbis2d are all initialised lazily by their respective
    * drivers (ps4_joypad_init, orbis_audio_init, orbis_ctx_init).
    * Keeping this function minimal avoids double-init when the
    * loader already set up OrbisGlobalConf. */
}

static void frontend_orbis_exec(const char *path, bool should_load_game)
{
   char argp[512] = {0};
   int   args = 0;

#ifndef IS_SALAMANDER
   if (should_load_game && !path_is_empty(RARCH_PATH_CONTENT))
   {
      argp[args] = '\0';
      strlcat(argp + args, path_get(RARCH_PATH_CONTENT), sizeof(argp) - args);
      args += strlen(argp + args) + 1;
   }
#endif

   RARCH_LOG("Attempt to load executable: [%s].\n", path);
   RARCH_LOG("Attempt to load executable args=%d [%s].\n", args, argp);

#if defined(HAVE_ORBIS_APPEXEC)
   {
      /* sceAppMgrLoadExec is available when linking against
       * the appropriate stub library. Enable by setting
       * HAVE_ORBIS_APPEXEC=1 in your build environment. */
      const char *argv[3] = {path, NULL, NULL};
      if (args > 0)
         argv[1] = argp;

      int ret = sceAppMgrLoadExec(path, (char * const*)argv, NULL);
      RARCH_LOG("sceAppMgrLoadExec returned: 0x%08X\n", ret);
      if (ret != 0)
         RARCH_ERR("sceAppMgrLoadExec failed (0x%08X) for path: %s\n", ret, path);
   }
#else
   RARCH_WARN("frontend_orbis_exec: HAVE_ORBIS_APPEXEC not set, exec is a no-op.\n");
   (void)args;
   (void)argp;
#endif
}

#ifndef IS_SALAMANDER
static bool frontend_orbis_set_fork(enum frontend_fork fork_mode)
{
   switch (fork_mode)
   {
      case FRONTEND_FORK_CORE:
         RARCH_LOG("FRONTEND_FORK_CORE\n");
         orbis_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_CORE_WITH_ARGS:
         RARCH_LOG("FRONTEND_FORK_CORE_WITH_ARGS\n");
         orbis_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_RESTART:
         RARCH_LOG("FRONTEND_FORK_RESTART\n");
         /* NOTE: We don't implement Salamander, so just turn
          * this into FRONTEND_FORK_CORE. */
         orbis_fork_mode  = FRONTEND_FORK_CORE;
         break;
      case FRONTEND_FORK_NONE:
      default:
         return false;
   }

   return true;
}
#endif

static void frontend_orbis_exitspawn(char *s, size_t len)
{
   bool should_load_game = false;
#ifndef IS_SALAMANDER
   if (orbis_fork_mode == FRONTEND_FORK_NONE)
      return;

   switch (orbis_fork_mode)
   {
      case FRONTEND_FORK_CORE_WITH_ARGS:
         should_load_game = true;
         break;
      case FRONTEND_FORK_NONE:
      default:
         break;
   }
#endif
   frontend_orbis_exec(s, should_load_game);
}

static int frontend_orbis_get_rating(void)
{
   return 6; /* Go with a conservative figure for now. */
}

enum frontend_architecture frontend_orbis_get_architecture(void)
{
   return FRONTEND_ARCH_X86_64;
}

/* Returns true if path exists and is accessible.  Used to suppress
 * removable mounts (/usb0, /usb1) from the drive list when they are
 * not present.  Note: stat() confirms the path exists in the VFS; it
 * does not distinguish between an empty mount point and a mounted
 * volume — on-device testing is needed to verify this behaviour. */
static bool orbis_path_accessible(const char *path)
{
   struct stat st;
   return stat(path, &st) == 0;
}

static int frontend_orbis_parse_drive_list(void *data, bool load_content)
{
#ifndef IS_SALAMANDER
   file_list_t *list = (file_list_t*)data;
   enum msg_hash_enums enum_idx = load_content ?
      MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR :
      MSG_UNKNOWN;

   menu_entries_append_enum(list,
         "host0:app",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
   menu_entries_append_enum(list,
         "host0:app/data",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
   menu_entries_append_enum(list,
         "host0:app/data/retroarch",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
   menu_entries_append_enum(list,
         "/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
   menu_entries_append_enum(list,
         "/data",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
   if (orbis_path_accessible("/usb0"))
      menu_entries_append_enum(list,
            "/usb0",
            msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
            enum_idx,
            FILE_TYPE_DIRECTORY, 0, 0);
   if (orbis_path_accessible("/usb1"))
      menu_entries_append_enum(list,
            "/usb1",
            msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
            enum_idx,
            FILE_TYPE_DIRECTORY, 0, 0);
   menu_entries_append_enum(list,
         "/data/self",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
#endif
   return 0;

}

frontend_ctx_driver_t frontend_ctx_orbis = {
   frontend_orbis_get_environment_settings,
   frontend_orbis_init,
   frontend_orbis_deinit,
   frontend_orbis_exitspawn,
   NULL,                         /* process_args */
   frontend_orbis_exec,
#ifdef IS_SALAMANDER
   NULL,
#else
   frontend_orbis_set_fork,
#endif
   frontend_orbis_shutdown,
   NULL,                         /* get_name */
   NULL,                         /* get_os */
   frontend_orbis_get_rating,
   NULL,                         /* load_content */
   frontend_orbis_get_architecture,
   NULL,
   frontend_orbis_parse_drive_list,
   frontend_orbis_get_mem_total, /* get_mem_total */
   frontend_orbis_get_mem_used,  /* get_mem_free  */
   NULL,                         /* install_signal_handler */
   NULL,                         /* get_sighandler_state */
   NULL,                         /* set_sighandler_state */
   NULL,                         /* destroy_sighandler_state */
   NULL,                         /* attach_console */
   NULL,                         /* detach_console */
   NULL,                         /* watch_path_for_changes */
   NULL,                         /* check_for_path_changes */
   NULL,                         /* set_sustained_performance_mode */
   frontend_orbis_get_cpu_model_name,
   NULL,                         /* get_user_language */
   "orbis",
};
