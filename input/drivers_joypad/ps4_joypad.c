/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2013-2014 - CatalystG
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <boolean.h>

#include <rthreads/rthreads.h>

#include "../input_driver.h"

#include "../../tasks/tasks_internal.h"
#include "../../verbosity.h"

#include <userservice.h>
#include <pad.h>

#define PS4_MAX_ORBISPADS 16
#define SCE_USER_SERVICE_MAX_LOGIN_USERS ORBIS_USER_SERVICE_MAX_LOGIN_USERS
#define SCE_USER_SERVICE_USER_ID_INVALID ORBIS_USER_SERVICE_USER_ID_INVALID

#define	ORBISPAD_L3		      ORBIS_PAD_BUTTON_L3
#define	ORBISPAD_R3		      ORBIS_PAD_BUTTON_R3
#define	ORBISPAD_OPTIONS	   ORBIS_PAD_BUTTON_OPTIONS
#define	ORBISPAD_UP		      ORBIS_PAD_BUTTON_UP
#define	ORBISPAD_RIGHT		   ORBIS_PAD_BUTTON_RIGHT
#define	ORBISPAD_DOWN		   ORBIS_PAD_BUTTON_DOWN
#define	ORBISPAD_LEFT		   ORBIS_PAD_BUTTON_LEFT
#define	ORBISPAD_L2		      ORBIS_PAD_BUTTON_L2
#define	ORBISPAD_R2		      ORBIS_PAD_BUTTON_R2
#define	ORBISPAD_L1		      ORBIS_PAD_BUTTON_L1
#define	ORBISPAD_R1		      ORBIS_PAD_BUTTON_R1
#define	ORBISPAD_TRIANGLE	   ORBIS_PAD_BUTTON_TRIANGLE
#define	ORBISPAD_CIRCLE		ORBIS_PAD_BUTTON_CIRCLE
#define	ORBISPAD_CROSS		   ORBIS_PAD_BUTTON_CROSS
#define	ORBISPAD_SQUARE		ORBIS_PAD_BUTTON_SQUARE
#define	ORBISPAD_TOUCH_PAD	ORBIS_PAD_BUTTON_TOUCH_PAD
#define	ORBISPAD_INTERCEPTED	0x80000000

/*
 * Global var's
 */
typedef struct
{
   OrbisUserServiceUserId userId;
   int                   handle;
   bool                  connected;
   OrbisPadVibeParam     rumble; /* last-submitted vibration state */
} ds_joypad_state;

static ds_joypad_state ds_joypad_states[PS4_MAX_ORBISPADS];
static uint64_t pad_state[PS4_MAX_ORBISPADS];
static int16_t analog_state[PS4_MAX_ORBISPADS][2][2];
static int16_t num_players = 0;
static slock_t *ps4_joypad_lock = NULL;

static bool ps4_joypad_ensure_lock(void)
{
   if (ps4_joypad_lock)
      return true;

   ps4_joypad_lock = slock_new();

   if (!ps4_joypad_lock)
   {
      RARCH_ERR("[ps4_joypad] failed to create state lock.\n");
      return false;
   }

   return true;
}

static const char *ps4_joypad_name(unsigned pad)
{
   return "PS4 Controller";
}

static bool ps4_joypad_init(void *data)
{
   int result;
   OrbisUserServiceLoginUserIdList userIdList;

   (void)data;

   if (!ps4_joypad_ensure_lock())
      return false;

   slock_lock(ps4_joypad_lock);

   num_players = 0;
   memset(ds_joypad_states, 0, sizeof(ds_joypad_states));

   scePadInit();

	result = sceUserServiceGetLoginUserIdList(&userIdList);

   RARCH_LOG("sceUserServiceGetLoginUserIdList %x ", result);

	if (result == 0)
	{
      unsigned i;
      for (i = 0; i < SCE_USER_SERVICE_MAX_LOGIN_USERS; i++)
      {
         OrbisUserServiceUserId userId = userIdList.userId[i];
         bool user_already_registered = false;
         int index = 0;

         RARCH_LOG("USER %d ID %x\n", i, userId);

         if (userId != SCE_USER_SERVICE_USER_ID_INVALID)
         {
            while (index < num_players)
            {
               if (ds_joypad_states[index].userId == userId)
               {
                  user_already_registered = true;
                  break;
               }
               index++;
            }

            if (user_already_registered)
               continue;

            if (num_players >= PS4_MAX_ORBISPADS)
            {
               RARCH_WARN("[ps4_joypad] maximum supported users reached (%u).\n",
                     (unsigned)PS4_MAX_ORBISPADS);
               break;
            }

            ds_joypad_states[num_players].handle = scePadOpen(userId, 0, 0, NULL);
            RARCH_LOG("USER %x HANDLE %x\n", userId, ds_joypad_states[num_players].handle);
            if (ds_joypad_states[num_players].handle > 0)
            {
               ds_joypad_states[num_players].connected = true;
               ds_joypad_states[num_players].userId = userId;
               memset(&ds_joypad_states[num_players].rumble, 0,
                     sizeof(ds_joypad_states[num_players].rumble));
               RARCH_LOG("NEW PAD: num_players %x \n", num_players);

               input_autoconfigure_connect(
                     ps4_joypad_name(num_players),
                     NULL,
                     ps4_joypad.ident,
                     num_players,
                     0,
                     0);
               num_players++;
            }
         }

      }

   }

   slock_unlock(ps4_joypad_lock);

   return true;
}

static bool ps4_joypad_button(unsigned port_num, uint16_t joykey)
{
   if (port_num >= PS4_MAX_ORBISPADS)
      return false;
   return (pad_state[port_num] & (UINT64_C(1) << joykey));
}

static void ps4_joypad_get_buttons(unsigned port_num, input_bits_t *state)
{
	if (port_num < PS4_MAX_ORBISPADS)
   {
		BITS_COPY16_PTR( state, pad_state[port_num] );
	}
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t ps4_joypad_axis(unsigned port_num, uint32_t joyaxis)
{
   OrbisPadData buttons;
   int16_t    val = 0;

   if (port_num >= PS4_MAX_ORBISPADS)
      return 0;
   if (!ds_joypad_states[port_num].connected)
      return 0;
   if (joyaxis == AXIS_NONE)
      return 0;

   if (scePadReadState(ds_joypad_states[port_num].handle, &buttons) != 0)
      return 0;

   if (AXIS_POS_GET(joyaxis) != AXIS_DIR_NONE)
   {
      unsigned axis_idx = AXIS_POS_GET(joyaxis);
      switch (axis_idx)
      {
         case 0:  /* Left  X */
            val = ((int32_t)buttons.leftStick.x  - 0x80) << 8;
            break;
         case 1:  /* Left  Y (inverted: up = negative) */
            val = ((int32_t)buttons.leftStick.y  - 0x80) << 8;
            break;
         case 2:  /* Right X */
            val = ((int32_t)buttons.rightStick.x - 0x80) << 8;
            break;
         case 3:  /* Right Y */
            val = ((int32_t)buttons.rightStick.y - 0x80) << 8;
            break;
         default: break;
      }
      if (val < 0)
         val = 0;
   }
   else if (AXIS_NEG_GET(joyaxis) != AXIS_DIR_NONE)
   {
      unsigned axis_idx = AXIS_NEG_GET(joyaxis);
      switch (axis_idx)
      {
         case 0:  val = ((int32_t)buttons.leftStick.x  - 0x80) << 8; break;
         case 1:  val = ((int32_t)buttons.leftStick.y  - 0x80) << 8; break;
         case 2:  val = ((int32_t)buttons.rightStick.x - 0x80) << 8; break;
         case 3:  val = ((int32_t)buttons.rightStick.y - 0x80) << 8; break;
         default: break;
      }
      if (val > 0)
         val = 0;
   }

   return val;
}

static void ps4_joypad_poll(void)
{
   unsigned player;
   unsigned players_count;
   OrbisPadData buttons;

   if (!ps4_joypad_lock)
      return;

   slock_lock(ps4_joypad_lock);

   players_count = (unsigned)num_players;
   if (players_count > PS4_MAX_ORBISPADS)
      players_count = PS4_MAX_ORBISPADS;

   for (player = 0; player < players_count; player++)
   {
      unsigned j, k;
      unsigned i  = player;
      unsigned p  = player;
      int ret     = scePadReadState(ds_joypad_states[player].handle,&buttons);

      if (ret == 0)
      {
         int32_t state_tmp = buttons.buttons;
         pad_state[i] = 0;

         pad_state[i] |= (state_tmp & ORBISPAD_LEFT) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_DOWN) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_RIGHT) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_UP) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_OPTIONS) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_START) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_TOUCH_PAD) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_TRIANGLE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_X) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_SQUARE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_Y) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_CROSS) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_B) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_CIRCLE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_A) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_R1) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_L1) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_R2) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R2) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_L2) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L2) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_R3) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R3) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_L3) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L3) : 0;
      }
   }

   slock_unlock(ps4_joypad_lock);
}

static bool ps4_joypad_query_pad(unsigned pad)
{
   return pad < PS4_MAX_ORBISPADS && pad_state[pad];
}

static bool ps4_joypad_rumble(unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   uint8_t motor;

   if (pad >= PS4_MAX_ORBISPADS || !ds_joypad_states[pad].connected)
      return false;

   /* Scale RetroArch 0-0xFFFF strength to DualShock4 0-0xFF motor range. */
   motor = (uint8_t)(strength >> 8);

   if (effect == RETRO_RUMBLE_STRONG)
      ds_joypad_states[pad].rumble.lgMotor = motor;
   else
      ds_joypad_states[pad].rumble.smMotor = motor;

   return scePadSetVibration(ds_joypad_states[pad].handle,
         &ds_joypad_states[pad].rumble) == 0;
}

static void ps4_joypad_destroy(void)
{
   unsigned i;

   if (ps4_joypad_lock)
      slock_lock(ps4_joypad_lock);

   for (i = 0; i < PS4_MAX_ORBISPADS; i++)
   {
      if (ds_joypad_states[i].connected)
      {
         /* Stop any active rumble before closing. */
         OrbisPadVibeParam zero = {0, 0};
         scePadSetVibration(ds_joypad_states[i].handle, &zero);
         scePadClose(ds_joypad_states[i].handle);
         ds_joypad_states[i].connected = false;
         ds_joypad_states[i].handle    = 0;
      }
   }
   num_players = 0;

   if (ps4_joypad_lock)
      slock_unlock(ps4_joypad_lock);
}

input_device_driver_t ps4_joypad = {
   ps4_joypad_init,
   ps4_joypad_query_pad,
   ps4_joypad_destroy,
   ps4_joypad_button,
   ps4_joypad_get_buttons,
   ps4_joypad_axis,
   ps4_joypad_poll,
   ps4_joypad_rumble,
   ps4_joypad_name,
   "ps4",
};
