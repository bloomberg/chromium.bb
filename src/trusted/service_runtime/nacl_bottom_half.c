/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl service runtime, bottom half routines for video.
 */

#include <string.h>
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"

#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_closure.h"
#include "native_client/src/trusted/service_runtime/nacl_sync_queue.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"

#include "native_client/src/trusted/service_runtime/include/sys/errno.h"

#if defined(HAVE_SDL)
# include <SDL.h>
# include "native_client/src/trusted/service_runtime/include/sys/audio_video.h"
#endif

int NaClClosureResultCtor(struct NaClClosureResult *self) {
  self->result_valid = 0;
  self->rv = NULL;
  if (!NaClMutexCtor(&self->mu)) {
    return 0;
  }
  if (!NaClCondVarCtor(&self->cv)) {
    NaClMutexDtor(&self->mu);
    return 0;
  }
  return 1;
}

void NaClClosureResultDtor(struct NaClClosureResult *self) {
  NaClMutexDtor(&self->mu);
  NaClCondVarDtor(&self->cv);
}

void *NaClClosureResultWait(struct NaClClosureResult *self) {
  void  *rv;

  NaClXMutexLock(&self->mu);
  while (!self->result_valid) {
    NaClXCondVarWait(&self->cv, &self->mu);
  }
  rv = self->rv;
  self->result_valid = 0;
  NaClXMutexUnlock(&self->mu);

  return rv;
}

void NaClClosureResultDone(struct NaClClosureResult *self,
                           void                     *rv) {
  NaClXMutexLock(&self->mu);
  if (self->result_valid) {
    NaClLog(LOG_FATAL, "NaClClosureResultDone: result already present?!?\n");
  }
  self->rv = rv;
  self->result_valid = 1;
  NaClXCondVarSignal(&self->cv);
  NaClXMutexUnlock(&self->mu);
}

void NaClStartAsyncOp(struct NaClAppThread  *natp,
                      struct NaClClosure    *ncp) {
  NaClLog(4, "NaClStartAsyncOp(0x%08"NACL_PRIxPTR", 0x%08"NACL_PRIxPTR")\n",
          (uintptr_t) natp,
          (uintptr_t) ncp);
  NaClSyncQueueInsert(&natp->nap->work_queue, ncp);
  NaClLog(4, "Done\n");
}


void *NaClWaitForAsyncOp( struct NaClAppThread *natp ) {
  NaClLog(4, "NaClWaitForAsyncOp(0x%08"NACL_PRIxPTR")\n",
          (uintptr_t) natp);

  return NaClClosureResultWait(&natp->result);
}

int32_t NaClWaitForAsyncOpSysRet(struct NaClAppThread *natp) {
  uintptr_t result;
  int32_t result32;
  NaClLog(4, "NaClWaitForAsyncOp(0x%08"NACL_PRIxPTR")\n",
          (uintptr_t) natp);

  result = (uintptr_t) NaClClosureResultWait(&natp->result);
  result32 = (int32_t) result;

  if (result != (uintptr_t) result) {
    NaClLog(LOG_ERROR,
            ("Overflow in NaClWaitForAsyncOpSysRet: return value is "
            "%"NACL_PRIxPTR"\n"),
            result);
    result32 = -NACL_ABI_EOVERFLOW;
  }

  return result32;
}

#if defined(HAVE_SDL)


typedef struct InfoVideo {
  int32_t                 width;
  int32_t                 height;
  int32_t                 format;
  int32_t                 bits_per_pixel;
  int32_t                 bytes_per_pixel;
  int32_t                 rmask;
  int32_t                 gmask;
  int32_t                 bmask;
  SDL_Surface             *screen;
} InfoVideo;


typedef struct InfoAudio {
  SDL_AudioSpec           *audio_spec;
  struct NaClMutex        mutex;
  struct NaClCondVar      condvar;
  volatile int32_t        ready;
  volatile size_t         size;
  unsigned char           *stream;
  volatile int32_t        first;
  volatile int32_t        shutdown;
  SDL_AudioSpec           obtained;
  struct NaClAppThread    *thread_ptr;
} InfoAudio;


typedef struct InfoMultimedia {
  /* do not move begin (initialization dep) */
  volatile int32_t        sdl_init_flags;
  volatile int32_t        initialized;
  volatile int32_t        have_video;
  volatile int32_t        have_audio;
  /* do not move end */
  InfoVideo               video;
  InfoAudio               audio;
} InfoMultimedia;

static struct InfoMultimedia nacl_multimedia;
static struct NaClMutex nacl_multimedia_mutex;


void NaClMultimediaModuleInit() {
  int r;
  /* clear all values in nacl_multimedia to 0 */
  memset(&nacl_multimedia, 0, sizeof(nacl_multimedia));
  r = NaClMutexCtor(&nacl_multimedia_mutex);
  if (1 != r)
    NaClLog(LOG_FATAL, "NaClAudioVideoModuleInit: mutex ctor failed\n");
}


void NaClMultimediaModuleFini() {
  NaClMutexDtor(&nacl_multimedia_mutex);
  /* clear all values in nacl_multimedia to 0 */
  memset(&nacl_multimedia, 0, sizeof(nacl_multimedia));
}


void NaClBotSysMultimedia_Init(struct NaClAppThread *natp, int subsys) {
  int32_t   r;
  uintptr_t retval;

  r = NaClMutexLock(&nacl_multimedia_mutex);
  if (NACL_SYNC_OK != r)
    NaClLog(LOG_FATAL, "NaClBotSysMultimedia_Init: mutex lock failed\n");

  NaClLog(3, "Entered NaClBotSysMultimedia_Init(0x%08"NACL_PRIxPTR", %d)\n",
          (uintptr_t) natp, subsys);

  retval = -NACL_ABI_EINVAL;

  if (!natp->is_privileged && natp->nap->restrict_to_main_thread) {
    retval = -NACL_ABI_EIO;
    goto done;
  }

  /* don't allow nested init */
  if (0 != nacl_multimedia.initialized)
    NaClLog(LOG_FATAL, "NaClSysMultimedia_Init: Already initialized!\n");

  if (0 != nacl_multimedia.have_video)
    NaClLog(LOG_FATAL, "NaClSysMultimedia_Init: video initialized?\n");

  if (0 != nacl_multimedia.have_audio)
    NaClLog(LOG_FATAL, "NaClSysMultimedia_Init: audio initialized?\n");

  /* map nacl a/v to sdl subsystem init */
  if (NACL_SUBSYSTEM_VIDEO == (subsys & NACL_SUBSYSTEM_VIDEO)) {
    nacl_multimedia.sdl_init_flags |= SDL_INIT_VIDEO;
  }
  if (NACL_SUBSYSTEM_AUDIO == (subsys & NACL_SUBSYSTEM_AUDIO)) {
    nacl_multimedia.sdl_init_flags |= SDL_INIT_AUDIO;
  }
  if (SDL_Init(nacl_multimedia.sdl_init_flags)) {
    NaClLog(LOG_ERROR, "NaClSysMultimeida_Init: SDL_Init failed\n");
    retval = -NACL_ABI_EIO;
    goto done;
  }

  nacl_multimedia.initialized = 1;
  retval = 0;

 done:
  r = NaClMutexUnlock(&nacl_multimedia_mutex);
  if (NACL_SYNC_OK != r)
    NaClLog(LOG_FATAL,
      "NaClBotSysMultimedia_Init: mutex unlock failed\n");

  NaClClosureResultDone(&natp->result, (void *) retval);
}


void NaClBotSysMultimedia_Shutdown(struct NaClAppThread *natp) {
  int32_t   r;
  uintptr_t retval;

  r = NaClMutexLock(&nacl_multimedia_mutex);
  if (NACL_SYNC_OK != r)
    NaClLog(LOG_FATAL, "NaClBotSysMultimedia_Shutdown: mutex lock failed\n");
  if (0 == nacl_multimedia.initialized)
    NaClLog(LOG_FATAL, "NaClBotSysMultimedia_Shutdown: not initialized!\n");
  if (0 != nacl_multimedia.have_video)
    NaClLog(LOG_FATAL,
      "NaClBotSysMultimedia_Shutdown: video subsystem not shutdown!\n");
  if (0 != nacl_multimedia.have_audio)
    NaClLog(LOG_FATAL,
      "NaClBotSysMultimedia_Shutdown: audio subsystem not shutdown!\n");

  SDL_Quit();
  retval = 0;
  nacl_multimedia.sdl_init_flags = 0;
  nacl_multimedia.initialized = 0;

  r = NaClMutexUnlock(&nacl_multimedia_mutex);
  if (NACL_SYNC_OK != r)
    NaClLog(LOG_FATAL,
      "NaClBotSysMultimedia_Shutdown: mutex unlock failed\n");

  NaClClosureResultDone(&natp->result, (void *) retval);
}


void NaClBotSysVideo_Init(struct NaClAppThread *natp,
                          int                  width,
                          int                  height) {
  int32_t   r;
  uintptr_t retval;
  uint32_t  sdl_video_flags = SDL_DOUBLEBUF | SDL_HWSURFACE;

  r = NaClMutexLock(&nacl_multimedia_mutex);
  if (NACL_SYNC_OK != r)
    NaClLog(LOG_FATAL, "NaClBotSysVideo_Init: mutex lock failed\n");

  NaClLog(3, "Entered NaClBotSysVideo_Init(0x%08"NACL_PRIxPTR", %d, %d)\n",
          (uintptr_t) natp, width, height);

  retval = -NACL_ABI_EINVAL;

  if (0 == nacl_multimedia.initialized)
    NaClLog(LOG_FATAL, "NaClBotSysVideo_Init: multimedia not initialized\n");

  /* for now don't allow app to have more than one SDL window or resize */
  if (0 != nacl_multimedia.have_video)
    NaClLog(LOG_FATAL, "NaClBotSysVideo_Init: already initialized!\n");

  if (SDL_INIT_VIDEO != (nacl_multimedia.sdl_init_flags & SDL_INIT_VIDEO))
    NaClLog(LOG_FATAL,
      "NaClBotSysVideo_Init: video not originally requested\n");

  nacl_multimedia.have_video = 1;
  nacl_multimedia.video.screen = SDL_SetVideoMode(width, height,
                                                  0, sdl_video_flags);
  if (!nacl_multimedia.video.screen) {
    NaClLog(LOG_ERROR, "NaClSysVideo_Init: SDL_SetVideoMode failed\n");
    nacl_multimedia.have_video = 0;
    retval = -NACL_ABI_EIO;
    goto done;
  }

  /* width, height and format validated in top half */
  nacl_multimedia.video.width = width;
  nacl_multimedia.video.height = height;
  /* video format always BGRA */
  nacl_multimedia.video.rmask = 0x00FF0000;
  nacl_multimedia.video.gmask = 0x0000FF00;
  nacl_multimedia.video.bmask = 0x000000FF;
  nacl_multimedia.video.bits_per_pixel = 32;
  nacl_multimedia.video.bytes_per_pixel = 4;

  /* set the window caption */
  /* todo: as parameter to nacl_video_init? */
  SDL_WM_SetCaption("NaCl Application", "NaCl Application");
  retval = 0;

 done:
  r = NaClMutexUnlock(&nacl_multimedia_mutex);
  if (NACL_SYNC_OK != r)
    NaClLog(LOG_FATAL, "NaClBotSysVideo_Init: mutex unlock failed\n");
  NaClClosureResultDone(&natp->result, (void *) retval);
}


/*
 * NaClBotSysVideo_Shutdown -- shuts down the Video interface.  This
 * doesn't have to be a bottom half function per se, except that it
 * references nacl_multimedia, and if we were to make that accessible
 * to top-half code, we'd have to ensure memory barriers are in place
 * around accesses to it.
 */
void NaClBotSysVideo_Shutdown(struct NaClAppThread *natp) {
  int32_t   r;
  uintptr_t retval;

  r = NaClMutexLock(&nacl_multimedia_mutex);
  if (NACL_SYNC_OK != r)
    NaClLog(LOG_FATAL, "NaClBotSysVideo_Shutdown: mutex lock failed\n");
  if (0 == nacl_multimedia.initialized)
    NaClLog(LOG_FATAL,
      "NaClBotSysVideo_Shutdown: multimedia not initialized!\n");
  if (0 == nacl_multimedia.have_video) {
    NaClLog(LOG_ERROR, "NaClBotSysVideo_Shutdown: video already shutdown!\n");
    retval = -NACL_ABI_EPERM;
    goto done;
  }
  nacl_multimedia.have_video = 0;
  retval = 0;
 done:
  r = NaClMutexUnlock(&nacl_multimedia_mutex);
  if (NACL_SYNC_OK != r)
    NaClLog(LOG_FATAL, "NaClBotSysVideo_Shutdown: mutex unlock failed\n");
  NaClClosureResultDone(&natp->result, (void *) retval);
}


/*
 * Update the "frame buffer" with user-supplied video data.
 *
 * Note that "data" is a user address that must be validated in the
 * bottom half code because the top half does not record the
 * width/height/video format info, and the global static variable
 * nacl_multimedia is inaccessible to top-half threads.
 */
void NaClBotSysVideo_Update(struct NaClAppThread *natp,
                            const void           *data) {
  uintptr_t     retval;
  SDL_Surface   *image;
  uintptr_t     sysaddr;
  int32_t       size;

  retval = -NACL_ABI_EINVAL;
  image = NULL;

  if (0 == nacl_multimedia.have_video)
    NaClLog(LOG_FATAL, "NaClBotVideo_Update: video not initialized\n");

  /* verify data ptr range */
  size = nacl_multimedia.video.bytes_per_pixel *
         nacl_multimedia.video.width *
         nacl_multimedia.video.height;
  sysaddr = NaClUserToSysAddrRange(natp->nap,
                                   (uintptr_t) data,
                                   size);
  if (kNaClBadAddress == sysaddr) {
    NaClLog(LOG_ERROR, "NaClBotSysVideo_Update: width, height: %d, %d\n",
      nacl_multimedia.video.width, nacl_multimedia.video.height);
    NaClLog(LOG_FATAL,
      "NaClBotSysVideo_Update: input data address range invalid\n");
  }

  image = SDL_CreateRGBSurfaceFrom((unsigned char*) sysaddr,
                                   nacl_multimedia.video.width,
                                   nacl_multimedia.video.height,
                                   nacl_multimedia.video.bits_per_pixel,
                                   nacl_multimedia.video.width *
                                     nacl_multimedia.video.bytes_per_pixel,
                                   nacl_multimedia.video.rmask,
                                   nacl_multimedia.video.gmask,
                                   nacl_multimedia.video.bmask, 0);
  if (NULL == image) {
    NaClLog(LOG_ERROR,
      "NaClBotSysVideo_Update: SDL_CreateRGBSurfaceFrom failed\n");
    retval = -NACL_ABI_EPERM;
    goto done;
  }

  retval = SDL_SetAlpha(image, 0, 255);
  if (0 != retval) {
    NaClLog(LOG_ERROR,
      "NaClBotSysVideo_Update SDL_SetAlpha failed (%"NACL_PRIuS")\n", retval);
    retval = -NACL_ABI_EPERM;
    goto done_free_image;
  }

  retval = SDL_BlitSurface(image, NULL, nacl_multimedia.video.screen, NULL);
  if (0 != retval) {
    NaClLog(LOG_ERROR,
      "NaClBotSysVideo_Update: SDL_BlitSurface failed\n");
    retval = -NACL_ABI_EPERM;
    goto done_free_image;
  }

  retval = SDL_Flip(nacl_multimedia.video.screen);
  if (0 != retval) {
    NaClLog(LOG_ERROR, "NaClBotSysVideo_Update: SDL_Flip failed\n");
    retval = -NACL_ABI_EPERM;
    goto done_free_image;
  }

  retval = 0;
  /* fall through to free image and closure */

 done_free_image:
  SDL_FreeSurface(image);
 done:
  NaClClosureResultDone(&natp->result, (void *) retval);
}


void NaClBotSysVideo_Poll_Event(struct NaClAppThread *natp,
                                union  NaClMultimediaEvent *event) {
  int32_t                   sdl_r;
  uintptr_t                 retval;
  int32_t                   repoll;
  SDL_Event                 sdl_event;

  if (0 == nacl_multimedia.initialized)
    NaClLog(LOG_FATAL,
            "NaClBotSysVideo_Poll_Event: multmedia not initialized!\n");
  if (0 == nacl_multimedia.have_video)
    NaClLog(LOG_FATAL,
      "NaClBotSysVideo_Poll_Event: video subsystem not initialized!\n");

  do {
    sdl_r = SDL_PollEvent(&sdl_event);
    repoll = 0;
    if (sdl_r == 0) {
      retval = -NACL_ABI_ENODATA;
      break;
    } else {
      retval = 0;
      switch(sdl_event.type) {
        case SDL_ACTIVEEVENT:
          event->type = NACL_EVENT_ACTIVE;
          event->active.gain = sdl_event.active.gain;
          event->active.state = sdl_event.active.state;
          break;
        case SDL_VIDEOEXPOSE:
          event->type = NACL_EVENT_EXPOSE;
          break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
          event->type = (SDL_KEYUP == sdl_event.type) ?
                         NACL_EVENT_KEY_UP : NACL_EVENT_KEY_DOWN;
          event->key.which = sdl_event.key.which;
          event->key.state = sdl_event.key.state;
          event->key.keysym.scancode = sdl_event.key.keysym.scancode;
          event->key.keysym.sym = sdl_event.key.keysym.sym;
          event->key.keysym.mod = sdl_event.key.keysym.mod;
          event->key.keysym.unicode = sdl_event.key.keysym.unicode;
          break;
        case SDL_MOUSEMOTION:
          event->type = NACL_EVENT_MOUSE_MOTION;
          event->motion.which = sdl_event.motion.which;
          event->motion.state = sdl_event.motion.state;
          event->motion.x = sdl_event.motion.x;
          event->motion.y = sdl_event.motion.y;
          event->motion.xrel = sdl_event.motion.xrel;
          event->motion.yrel = sdl_event.motion.yrel;
          break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
          event->type = (SDL_MOUSEBUTTONUP == sdl_event.type) ?
                         NACL_EVENT_MOUSE_BUTTON_UP :
                         NACL_EVENT_MOUSE_BUTTON_DOWN;
          event->button.which = sdl_event.button.which;
          event->button.button = sdl_event.button.button;
          event->button.state = sdl_event.button.state;
          event->button.x = sdl_event.button.x;
          event->button.y = sdl_event.button.y;
          break;
        case SDL_QUIT:
          event->type = NACL_EVENT_QUIT;
          break;
        default:
          /* an sdl event happened, but we don't support it
             so move along and try polling again */
          repoll = 1;
          break;
      }
    }
  } while (0 != repoll);

  NaClClosureResultDone(&natp->result, (void *) retval);
}


void __NaCl_InternalAudioCallback(void *unused, Uint8 *stream, int size) {
  int32_t r;

  UNREFERENCED_PARAMETER(unused);
  r = NaClMutexLock(&nacl_multimedia.audio.mutex);
  if (NACL_SYNC_OK != r)
    NaClLog(LOG_FATAL, "__NaCl_InternalAudioCallback: mutex lock failed\n");
  if (0 == nacl_multimedia.audio.ready) {
    nacl_multimedia.audio.stream = stream;
    nacl_multimedia.audio.size = size;
    nacl_multimedia.audio.ready = 1;
    r = NaClCondVarSignal(&nacl_multimedia.audio.condvar);
    if (NACL_SYNC_OK != r)
      NaClLog(LOG_FATAL,
        "__NaCl_InternalAudioCallback: cond var signal failed\n");
    /* wait for return signal */
    while ((1 == nacl_multimedia.audio.ready) &&
           (0 == nacl_multimedia.audio.shutdown)) {
      r = NaClCondVarWait(&nacl_multimedia.audio.condvar,
                          &nacl_multimedia.audio.mutex);
      if (NACL_SYNC_OK != r) {
        NaClLog(LOG_FATAL,
          "__NaCl_InternalAudioCallback: cond var wait failed\n");
      }
    }
  }
  r = NaClMutexUnlock(&nacl_multimedia.audio.mutex);
  if (NACL_SYNC_OK != r)
    NaClLog(LOG_FATAL, "__NaCl_InternalAudioCallback: mutex unlock failed\n");
}


void NaClBotSysAudio_Init(struct NaClAppThread *natp,
                          enum NaClAudioFormat format,
                          int                  desired_samples,
                          int                  *obtained_samples) {
  SDL_AudioSpec desired;
  int32_t       r;
  uintptr_t     retval;

  r = NaClMutexLock(&nacl_multimedia_mutex);
  if (NACL_SYNC_OK != r)
    NaClLog(LOG_FATAL, "NaClBotSysAudio_Init: mutex lock failed\n");

  NaClLog(3, "Entered NaClBotSysAudio_Init(0x%08"NACL_PRIxPTR", %d)\n",
          (uintptr_t) natp, format);

  retval = -NACL_ABI_EINVAL;

  if (0 == nacl_multimedia.initialized)
    NaClLog(LOG_FATAL, "NaClSysAudio_Init: multimedia not initialized!\n");

  /* for now, don't allow an app to open more than one SDL audio device */
  if (0 != nacl_multimedia.have_audio) {
    NaClLog(LOG_FATAL, "NaClSysAudio_Init: already initialized!\n");
  }

  if ((desired_samples < 128) || (desired_samples > 8192)) {
    NaClLog(LOG_ERROR,
      "NaClSysAudio_Init: desired sample value out of range\n");
    retval = -NACL_ABI_ERANGE;
    goto done;
  }

  memset(&nacl_multimedia.audio, 0, sizeof(nacl_multimedia.audio));
  memset(&desired, 0, sizeof(desired));

  r = NaClMutexCtor(&nacl_multimedia.audio.mutex);
  if (1 != r)
    NaClLog(LOG_FATAL, "NaClBotSysAudio_Init: mutex ctor failed\n");
  r = NaClCondVarCtor(&nacl_multimedia.audio.condvar);
  if (1 != r)
    NaClLog(LOG_FATAL, "NaClBotSysAudio_Init: cond var ctor failed\n");

  nacl_multimedia.audio.thread_ptr = 0;
  nacl_multimedia.audio.size = 0;
  nacl_multimedia.audio.ready = 0;
  nacl_multimedia.audio.first = 1;
  nacl_multimedia.audio.shutdown = 0;

  desired.format = AUDIO_S16LSB;
  desired.channels = 2;
  desired.samples = desired_samples;
  desired.callback = __NaCl_InternalAudioCallback;

  if (NACL_AUDIO_FORMAT_STEREO_44K == format) {
    desired.freq = 44100;
  } else if (NACL_AUDIO_FORMAT_STEREO_48K == format) {
    desired.freq = 48000;
  } else {
    /* we only support two simple high quality stereo formats */
    NaClLog(LOG_ERROR, "NaClSysAudio_Init: unsupported format\n");
    retval = -NACL_ABI_EINVAL;
    goto done;
  }

  if (SDL_OpenAudio(&desired, &nacl_multimedia.audio.obtained) < 0) {
    NaClLog(LOG_ERROR, "NaClSysAudio_Init: Couldn't open SDL audio: %s\n",
      SDL_GetError());
    retval = -NACL_ABI_EIO;
    goto done;
  }

  if (nacl_multimedia.audio.obtained.format != desired.format) {
    NaClLog(LOG_ERROR, "NaClSysAudio_Init: Couldn't get desired format\n");
    retval = -NACL_ABI_EIO;
    goto done_close;
  }

  *obtained_samples = nacl_multimedia.audio.obtained.samples;

  nacl_multimedia.have_audio = 1;
  retval = 0;
  goto done;

 done_close:
  SDL_CloseAudio();

 done:
  r = NaClMutexUnlock(&nacl_multimedia_mutex);
  if (NACL_SYNC_OK != r)
    NaClLog(LOG_FATAL, "NaClBotSysAudio_Init: mutex unlock failed\n");

  NaClClosureResultDone(&natp->result, (void *) retval);
}


void NaClBotSysAudio_Shutdown(struct NaClAppThread *natp) {
  int32_t   r;
  uintptr_t retval = -NACL_ABI_EINVAL;

  /* set volatile shutdown outside of mutexes to avoid deadlocking */
  nacl_multimedia.audio.shutdown = 1;
  r = NaClMutexLock(&nacl_multimedia_mutex);
  if (NACL_SYNC_OK != r)
    NaClLog(LOG_FATAL, "NaClBotSysAudio_Shutdown: mutex lock failed\n");
  if (0 == nacl_multimedia.initialized)
    NaClLog(LOG_FATAL, "NaClSysAudio_Shutdown: multimedia not initialized\n");
  if (0 == nacl_multimedia.have_audio)
    NaClLog(LOG_FATAL, "NaClSysAudio_Shutdown: audio not initialized!\n");
  /* tell audio thread we're shutting down */
  r = NaClMutexLock(&nacl_multimedia.audio.mutex);
  if (NACL_SYNC_OK != r)
    NaClLog(LOG_FATAL, "NaClSysAudio_Shutdown: mutex lock failed\n");
  r = NaClCondVarBroadcast(&nacl_multimedia.audio.condvar);
  if (NACL_SYNC_OK != r)
    NaClLog(LOG_FATAL, "NaClSysAudio_Shutdown: cond var broadcast failed\n");
  r = NaClMutexUnlock(&nacl_multimedia.audio.mutex);
  if (NACL_SYNC_OK != r)
    NaClLog(LOG_FATAL, "NaClSysAudio_Shutdown: mutex unlock failed\n");
  /* close out audio */
  SDL_CloseAudio();
  /* no more callbacks at this point */
  nacl_multimedia.have_audio = 0;
  retval = 0;
  r = NaClMutexUnlock(&nacl_multimedia_mutex);
  if (NACL_SYNC_OK != r)
    NaClLog(LOG_FATAL, "NaClBotSysAudio_Shutdown: mutex unlock failed\n");
  NaClClosureResultDone(&natp->result, (void *) retval);
}


/*
 * returns 0 during normal operation.
 * returns -1 indicating that it is time to exit the audio thread
 */
int32_t NaClSliceSysAudio_Stream(struct NaClAppThread *natp,
                                 const void           *data,
                                 size_t               *size) {
  int32_t                   r;
  int32_t                   retval = 0;
  uintptr_t                 sysaddr;
  size_t                    *syssize;

  r = NaClMutexLock(&nacl_multimedia_mutex);
  if (NACL_SYNC_OK != r)
    NaClLog(LOG_FATAL, "NaClSliceSysAudio_Stream: global mutex lock failed\n");

  if (0 == nacl_multimedia.have_audio) {
    /*
     * hmm... if we don't have audio, one reason could be that shutdown was
     * just called in another thread.  So we silently fail, telling the app
     * it is time to leave the audio thread.  The alternative is to treat
     * this situation as fatal, but that makes shutdown that much harder
     */
    retval = -1;
    goto done;
  }

  sysaddr = NaClUserToSysAddrRange(natp->nap,
                                   (uintptr_t) size,
                                   sizeof(size));
  if (kNaClBadAddress == sysaddr) {
    NaClLog(LOG_FATAL, "NaClSliceSysAudio_Stream: size address invalid\n");
  }
  syssize = (size_t *) sysaddr;

  if (nacl_multimedia.audio.first) {
    /* don't copy data on first call... */
    nacl_multimedia.audio.thread_ptr = natp;
  } else {
    /* verify we're being called from same thread as before */
    /* TODO(nfullagar): need to use a stronger thread uid mechanism */
    if (natp != nacl_multimedia.audio.thread_ptr) {
      NaClLog(LOG_FATAL,
        "NaClSliceSysAudio_Stream: called from different thread\n");
    }

    /* validate data buffer based on last size */
    sysaddr = NaClUserToSysAddrRange(natp->nap,
                                    (uintptr_t) data,
                                    nacl_multimedia.audio.size);
    if (kNaClBadAddress == sysaddr) {
      NaClLog(LOG_ERROR,
        "NaClSliceSysAudio_Stream: size: %"NACL_PRIdS"\n",
              nacl_multimedia.audio.size);
      NaClLog(LOG_FATAL, "NaClSliceSysAudio_Stream: data address invalid\n");
    }

    /* copy the audio data into the sdl audio buffer */
    memcpy(nacl_multimedia.audio.stream,
           (void *) sysaddr, nacl_multimedia.audio.size);
  }

  /* callback synchronization */
  r = NaClMutexLock(&nacl_multimedia.audio.mutex);
  if (NACL_SYNC_OK != r)
    NaClLog(LOG_FATAL, "NaClSliceSysAudio_Stream: audio mutex lock failed\n");
  /* unpause audio on first, start callbacks */
  if (nacl_multimedia.audio.first) {
    SDL_PauseAudio(0);
    nacl_multimedia.audio.first = 0;
  }

  /* signal callback (if it is waiting) */
  if (nacl_multimedia.audio.ready != 0) {
    nacl_multimedia.audio.ready = 0;
    r = NaClCondVarSignal(&nacl_multimedia.audio.condvar);
    if (NACL_SYNC_OK != r)
      NaClLog(LOG_FATAL,
        "NaClSliceSysAudio_Stream: cond var signal failed\n");
  }

  nacl_multimedia.audio.size = 0;

  /* wait for next callback */
  while ((0 == nacl_multimedia.audio.ready) &&
         (0 == nacl_multimedia.audio.shutdown)) {
    r = NaClCondVarWait(&nacl_multimedia.audio.condvar,
                        &nacl_multimedia.audio.mutex);
    if (NACL_SYNC_OK != r) {
      NaClLog(LOG_FATAL,
        "__NaCl_InternalAudioCallback: cond var wait failed\n");
    }
  }

  if (0 != nacl_multimedia.audio.shutdown) {
    nacl_multimedia.audio.size = 0;
    retval = -1;
  }
  /* return size of next audio block */
  *syssize = (size_t)nacl_multimedia.audio.size;
  r = NaClMutexUnlock(&nacl_multimedia.audio.mutex);
  if (NACL_SYNC_OK != r)
    NaClLog(LOG_FATAL,
      "NaClSliceSysAudio_Stream: audio mutex unlock failed\n");

 done:
  r = NaClMutexUnlock(&nacl_multimedia_mutex);
  if (NACL_SYNC_OK != r)
    NaClLog(LOG_FATAL,
      "NaClSliceSysAudio_Stream: global mutex unlock failed\n");
  return retval;
}


#endif  /* HAVE_SDL */
