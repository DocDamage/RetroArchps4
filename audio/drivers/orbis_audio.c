/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

/*
 * orbis_audio.c — RetroArch audio driver for PS4 (Orbis) via orbisAudio.
 *
 * orbisAudio uses a fixed-size ring of 512-sample stereo s16 blocks.
 * RetroArch feeds us arbitrary-sized float or s16 chunks, so we
 * buffer internally and drain one block at a time to orbisAudioOutput.
 *
 * Threading: orbisAudio outputs are synchronous (blocking) so we simply
 * submit one block per write call.  The nonblocking fast-forward path
 * silences output rather than tearing audio.
 *
 * Enabling: add HAVE_ORBIS_AUDIO=1 to your Makefile.orbis flags and
 * ensure -lorbisAudio is present in your link line.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>

#include "../../retroarch.h"
#include "../../verbosity.h"

#if !defined(HAVE_ORBIS_AUDIO)
/* Stub driver — compiles cleanly when orbisAudio is not available */

static void *orbis_audio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned block_frames, unsigned *new_rate)
{
   RARCH_WARN("[orbis_audio] driver stub — no HAVE_ORBIS_AUDIO support.\n");
   (void)device; (void)rate; (void)latency;
   (void)block_frames; (void)new_rate;
   return NULL;
}

static ssize_t  orbis_audio_write(void *d, const void *b, size_t s)   { (void)d;(void)b;(void)s; return -1; }
static bool     orbis_audio_stop(void *d)                              { (void)d; return true; }
static bool     orbis_audio_start(void *d, bool x)                    { (void)d;(void)x; return false; }
static bool     orbis_audio_alive(void *d)                             { (void)d; return false; }
static void     orbis_audio_set_nonblock_state(void *d, bool t)        { (void)d;(void)t; }
static void     orbis_audio_free(void *d)                              { (void)d; }
static bool     orbis_audio_use_float(void *d)                         { (void)d; return false; }
static size_t   orbis_audio_write_avail(void *d)                       { (void)d; return 0; }

#else /* HAVE_ORBIS_AUDIO */

#include <orbisAudio.h>
#include <audio/conversion/float_to_s16.h>

/* orbisAudio submits 512-sample stereo frames (2 channels × 512 s16) */
#define ORBIS_AUDIO_BLOCK_SAMPLES  512
#define ORBIS_AUDIO_CHANNELS       2
#define ORBIS_AUDIO_BLOCK_BYTES \
   (ORBIS_AUDIO_BLOCK_SAMPLES * ORBIS_AUDIO_CHANNELS * sizeof(int16_t))

typedef struct
{
   int   port;        /* orbisAudio port handle */
   bool  alive;
   bool  nonblocking;
   size_t buffered_frames; /* queued frames waiting for a full output block */

   /* Conversion staging buffer (holds one full block in float) */
   float  float_buf[ORBIS_AUDIO_BLOCK_SAMPLES * ORBIS_AUDIO_CHANNELS];
   int16_t pcm_buf[ORBIS_AUDIO_BLOCK_SAMPLES * ORBIS_AUDIO_CHANNELS];
} orbis_audio_t;

static void *orbis_audio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned block_frames, unsigned *new_rate)
{
   orbis_audio_t *oa;
   int port;
   int ret;

   (void)device;
   (void)latency;
   (void)block_frames;

   /* orbisAudio always runs at 48 kHz */
   if (new_rate)
      *new_rate = 48000;

   ret = orbisAudioInit();
   if (ret < 0)
   {
      RARCH_ERR("[orbis_audio] orbisAudioInit() failed: 0x%08X\n", ret);
      return NULL;
   }

   port = orbisAudioOpen(ORBIS_AUDIO_PORT_TYPE_MAIN,
                         ORBIS_AUDIO_BLOCK_SAMPLES,
                         ORBIS_AUDIO_FORMAT_S16_STEREO);
   if (port < 0)
   {
      RARCH_ERR("[orbis_audio] orbisAudioOpen() failed: 0x%08X\n", port);
      orbisAudioFinish();
      return NULL;
   }

   oa = (orbis_audio_t *)calloc(1, sizeof(*oa));
   if (!oa)
   {
      orbisAudioClose(port);
      orbisAudioFinish();
      return NULL;
   }

   oa->port  = port;
   oa->alive = true;

   RARCH_LOG("[orbis_audio] Opened port %d at 48 kHz stereo s16.\n", port);
   return oa;
}

static ssize_t orbis_audio_write(void *data, const void *buf, size_t size)
{
   orbis_audio_t *oa = (orbis_audio_t *)data;
   const float   *src;
   size_t         frames_in;
   size_t         frames_written = 0;
   int            ret;

   if (!oa || !oa->alive)
      return -1;

   if (oa->nonblocking)
      return (ssize_t)size;   /* fast-forward: accept but discard */

   /* RetroArch passes interleaved float samples (L R L R …). */
   src       = (const float *)buf;
   frames_in = size / (ORBIS_AUDIO_CHANNELS * sizeof(float));
   if (frames_in == 0)
      return 0;

   while (frames_in > 0)
   {
      size_t frames_free = ORBIS_AUDIO_BLOCK_SAMPLES - oa->buffered_frames;
      size_t frames_copy = frames_in < frames_free ? frames_in : frames_free;

      memcpy(&oa->float_buf[oa->buffered_frames * ORBIS_AUDIO_CHANNELS],
             &src[frames_written * ORBIS_AUDIO_CHANNELS],
             frames_copy * ORBIS_AUDIO_CHANNELS * sizeof(float));

      oa->buffered_frames += frames_copy;
      frames_written      += frames_copy;
      frames_in           -= frames_copy;

      if (oa->buffered_frames == ORBIS_AUDIO_BLOCK_SAMPLES)
      {
         convert_float_to_s16(oa->pcm_buf, oa->float_buf,
               ORBIS_AUDIO_BLOCK_SAMPLES * ORBIS_AUDIO_CHANNELS);

         ret = orbisAudioOutput(oa->port, ORBIS_AUDIO_VOLUME_0DB,
               ORBIS_AUDIO_VOLUME_0DB, oa->pcm_buf);
         if (ret < 0)
         {
            RARCH_ERR("[orbis_audio] orbisAudioOutput failed: 0x%08X\n", ret);
            return -1;
         }

         oa->buffered_frames = 0;
      }
   }

   return (ssize_t)(frames_written * ORBIS_AUDIO_CHANNELS * sizeof(float));
}

static bool orbis_audio_stop(void *data)
{
   orbis_audio_t *oa = (orbis_audio_t *)data;
   if (oa)
   {
      oa->alive = false;
      oa->buffered_frames = 0;
   }
   return true;
}

static bool orbis_audio_start(void *data, bool is_shutdown)
{
   orbis_audio_t *oa = (orbis_audio_t *)data;
   if (!oa)
      return false;
   if (!is_shutdown)
      oa->alive = true;
   return true;
}

static bool orbis_audio_alive(void *data)
{
   orbis_audio_t *oa = (orbis_audio_t *)data;
   return oa && oa->alive;
}

static void orbis_audio_set_nonblock_state(void *data, bool toggle)
{
   orbis_audio_t *oa = (orbis_audio_t *)data;
   if (oa)
   {
      oa->nonblocking = toggle;
      if (toggle)
         oa->buffered_frames = 0;
   }
}

static void orbis_audio_free(void *data)
{
   orbis_audio_t *oa = (orbis_audio_t *)data;
   if (!oa)
      return;

   oa->alive = false;
   orbisAudioClose(oa->port);
   orbisAudioFinish();
   free(oa);
}

static bool orbis_audio_use_float(void *data)
{
   (void)data;
   return true;  /* we accept float and convert internally */
}

static size_t orbis_audio_write_avail(void *data)
{
   orbis_audio_t *oa = (orbis_audio_t *)data;
   size_t frames_free;

   if (!oa)
      return 0;

   if (oa->buffered_frames < ORBIS_AUDIO_BLOCK_SAMPLES)
      frames_free = ORBIS_AUDIO_BLOCK_SAMPLES - oa->buffered_frames;
   else
      frames_free = 0;

   return frames_free * ORBIS_AUDIO_CHANNELS * sizeof(float);
}

#endif /* HAVE_ORBIS_AUDIO */

audio_driver_t audio_orbis = {
   orbis_audio_init,
   orbis_audio_write,
   orbis_audio_stop,
   orbis_audio_start,
   orbis_audio_alive,
   orbis_audio_set_nonblock_state,
   orbis_audio_free,
   orbis_audio_use_float,
   "orbis",
   NULL,              /* device_list_new  */
   NULL,              /* device_list_free */
   orbis_audio_write_avail,
   NULL               /* buffer_size      */
};
