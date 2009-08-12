/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */



#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <errno.h>

/* from sdk */
#include <sys/nacl_syscalls.h>
#include <pthread.h>
#include <nacl/nacl_srpc.h>

/* TODO: eliminate local includes */
#include "nacl_av.h"
#include "nacl_av_priv.h"

extern int __nacl_multimedia_init(int subsystems);
extern int __nacl_multimedia_shutdown();
extern int __nacl_video_init(int width, int height);
extern int __nacl_video_shutdown();
extern int __nacl_video_update(const void* data);
extern int __nacl_video_poll_event(union NaClMultimediaEvent *event);
extern int __nacl_audio_init(enum NaClAudioFormat format,
                             int desired_samples, int *obtained_samples);
extern int __nacl_audio_shutdown();
extern int __nacl_audio_stream(const void *data, size_t *size);

static const float NACL_BRIDGE_TIMEOUT = 2.0f;

struct NaClMultimedia {
  int initialized;
  int embedded;
  int subsystems;
  int sr_subsystems;
  int have_video;

  int display_shm_desc;
  int connected_socket;
  NaClSrpcChannel* channel;

  volatile void *thread_id;
  struct NaClVideoShare *video_data;
  int video_size;
};

static pthread_mutex_t nacl_multimedia_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile __thread int nacl_multimedia_thread_id;
static struct NaClMultimedia nacl_multimedia = {0, 0, 0, 0, 0,
                                                -1, -1, NULL,
                                                NULL, NULL, 0};

/*
 * The audio-visual system must wait until a video initialization RPC is
 * received from the browser.  The main thread will wait until
 * multimedia_init_done is non-zero.  Access is guarded using the
 * mutex and condition variable.
 */
static pthread_mutex_t init_wait_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t init_wait_cv = PTHREAD_COND_INITIALIZER;
static int multimedia_init_done = 0;

static void Fatal(char *msg) {
  fprintf(stderr, "%s", msg);
  exit(-1);
}

static void Log(char *msg) {
  fprintf(stderr, "%s", msg);
}

static inline int RetValErrno(int r) {
  if (r < 0) {
    errno = -r;
    return -1;
  }
  return r;
}

static inline float gettimeofdayf() {
  struct timeval tp;
  long t;
  gettimeofday(&tp, NULL);
  t = ((long)(tp.tv_sec)) * 1000000L + ((long)tp.tv_usec);
  return ((float)(t)) / 1000000.0f;
}

/**
 * Block until the plugin provides us with the shared memory handles
 * (by invoking nacl_multimedia_bridge).
 */
void __av_wait() {
  const int is_embedded = (srpc_get_fd() != -1);
  if (is_embedded) {
    pthread_mutex_lock(&init_wait_mu);
    while (!multimedia_init_done) {
      pthread_cond_wait(&init_wait_cv, &init_wait_mu);
    }
    pthread_mutex_unlock(&init_wait_mu);
  }
}

/**
 * Allow __av_wait to return, which will let main to start.
 */
static void mark_multimedia_init_done() {
  pthread_mutex_lock(&init_wait_mu);
  multimedia_init_done = 1;
  pthread_cond_broadcast(&init_wait_cv);
  pthread_mutex_unlock(&init_wait_mu);
}

/*
 * nacl_multimedia_bridge() is initialized at discovery.
 * It is a once-only event between plugin and native client.
 */
int nacl_multimedia_bridge(NaClSrpcChannel *channel,
                           NaClSrpcArg **in_args,
                           NaClSrpcArg **out_args) {
  struct stat st;

  nacl_multimedia.channel = (NaClSrpcChannel*) malloc(sizeof(NaClSrpcChannel));
  nacl_multimedia.display_shm_desc = in_args[0]->u.hval;
  nacl_multimedia.connected_socket = in_args[1]->u.hval;

  /* Start the SRPC client to communicate over the connected socket. */
  if (!NaClSrpcClientCtor(nacl_multimedia.channel,
       nacl_multimedia.connected_socket)) {
    Log("nacl_av: SRPC constructor FAILED\n");
    return NACL_SRPC_RESULT_INTERNAL;
  }

  /* Determine the size of the region. */
  if (fstat(nacl_multimedia.display_shm_desc, &st)) {
    Log("nacl_av: fstat failed\n");
    return NACL_SRPC_RESULT_INTERNAL;
  }
  /* Map the shared memory region into the NaCl module's address space. */
  nacl_multimedia.video_size = (size_t) st.st_size;
  nacl_multimedia.video_data = (struct NaClVideoShare*) mmap(NULL,
                                nacl_multimedia.video_size,
                                PROT_READ | PROT_WRITE,
                                MAP_SHARED,
                                nacl_multimedia.display_shm_desc,
                                0);
  if (MAP_FAILED == nacl_multimedia.video_data) {
    Log("nacl_av: mmap failed\n");
    nacl_multimedia.video_size = 0;
    nacl_multimedia.video_data = NULL;
    nacl_multimedia.embedded = 0;
    return NACL_SRPC_RESULT_INTERNAL;
  }

  /* Tell main to proceed allowing further processing */
  mark_multimedia_init_done();
  return NACL_SRPC_RESULT_OK;
}

/*
 * Export the method as taking two handles (the display shared memory and
 * a connected socket for upcalls)
 */
NACL_SRPC_METHOD("nacl_multimedia_bridge:hh:", nacl_multimedia_bridge);

static int nacl_multimedia_bridge_connected() {
  return NULL != nacl_multimedia.video_data;
}

static int nacl_video_bridge_init() {
  nacl_multimedia.video_data->u.h.video_ready = 1;
  return 0;
}

static int nacl_video_bridge_shutdown() {
  return 0;
}

static int nacl_video_bridge_poll_event(union NaClMultimediaEvent *event) {
  int rindex;
  int windex;
  if (NULL == nacl_multimedia.video_data) {
    return -EIO;
  }
  rindex = nacl_multimedia.video_data->u.h.event_read_index;
  windex = nacl_multimedia.video_data->u.h.event_write_index;
  if (rindex == windex) {
    /* when indices are equal, the queue is empty */
    return -ENODATA;
  }
  memcpy(event, &nacl_multimedia.video_data->u.h.event_queue[rindex],
      sizeof(*event));
  rindex = (rindex + 1) & (NACL_EVENT_RING_BUFFER_SIZE - 1);
  nacl_multimedia.video_data->u.h.event_read_index = rindex;

#define NACL_SHOW_EVENTS 0
#if NACL_SHOW_EVENTS
  switch (event->type) {
    case NACL_EVENT_MOUSE_MOTION:
      printf("mouse move: %d %d\n", event->motion.x, event->motion.y);
      break;
    case NACL_EVENT_KEY_DOWN:
      printf("key down: scancode: %d  sym: %d\n", event->key.keysym.scancode,
          event->key.keysym.sym);
      break;
    case NACL_EVENT_KEY_UP:
      printf("key up\n");
      break;
    case NACL_EVENT_MOUSE_BUTTON_UP:
    case NACL_EVENT_MOUSE_BUTTON_DOWN:
      printf("mouse button: %d %d\n", event->button.button, event->button.state);
      break;
    default:
      printf("another event\n");
      break;
  }
#endif

  return 0;
}

static int nacl_video_bridge_update(const void *data) {
  if (NULL != nacl_multimedia.video_data) {
    memcpy(&nacl_multimedia.video_data->video_pixels[0], data,
        nacl_multimedia.video_data->u.h.video_size);

    if (NACL_SRPC_RESULT_OK !=
        NaClSrpcInvokeByName(nacl_multimedia.channel, "upcall")) {
      Fatal("nacl_video_bridge_update() failed upcall\n");
      return -EIO;
    }
    sched_yield();
    return 0;
  }
  return -EIO;
}

/*
 * Currently, only one instance of nacl multimedia can
 * be initialized at any given time.
 *
 * The video subsystem can only have one rectangular output region.
 * The audio subsystem can only have one stream of stereo output.
 * There is one event stream, attached to the video region.
 */
int nacl_multimedia_init(int subsystems) {
  int inited = 0;
  int m;
  int r;

  m = pthread_mutex_lock(&nacl_multimedia_mutex);
  if (0 != m) {
    Fatal("nacl_multimedia_init: mutex lock failed\n");
  }
  /* don't allow nested init */
  if (0 != nacl_multimedia.initialized) {
    Fatal("nacl_multimedia_init: already initialized!\n");
  }

  /* verify event struct size */
  if (sizeof(union NaClMultimediaEvent) >
      sizeof(struct NaClMultimediaPadEvent)) {
    Fatal("nacl_multimedia_init: event union size mismatch\n");
  }

  /* verify header size */
  if ((NACL_VIDEO_SHARE_HEADER_SIZE + NACL_VIDEO_SHARE_PIXEL_PAD) !=
      sizeof(struct NaClVideoShare)) {
    Fatal("nacl_multimedia_init: shared structure size mismatch\n");
  }

  /* record thread id */
  nacl_multimedia.thread_id = &nacl_multimedia_thread_id;
  fprintf(stderr, "nacl_thread_id: 0x%0X 0x%0X\n",
      (unsigned int)nacl_multimedia.thread_id,
      (unsigned int)&nacl_multimedia_thread_id);

  if (nacl_multimedia_bridge_connected()) {
    Log("nacl_av: embedded in browser\n");
    /* embedded in browser */
    nacl_multimedia.embedded = 1;
    nacl_multimedia.subsystems = subsystems;
    nacl_multimedia.sr_subsystems = subsystems;
    /* strip out video from service runtime when embeding */
    /* in a browser -- srpc methods will be used instead */
    nacl_multimedia.sr_subsystems &= (~NACL_SUBSYSTEM_VIDEO);
    nacl_multimedia.sr_subsystems &= (~NACL_SUBSYSTEM_EMBED);
    inited = 1;
  }

  if (0 == inited) {
    /* not in browser */
    Log("nacl_av: NOT embedded in browser\n");
    nacl_multimedia.embedded = 0;
    nacl_multimedia.subsystems = subsystems;
    nacl_multimedia.sr_subsystems = (subsystems & (~NACL_SUBSYSTEM_EMBED));
    nacl_multimedia.video_data = NULL;
  }

  /* initialize service runtime portion of multimedia */
  r = __nacl_multimedia_init(nacl_multimedia.sr_subsystems);
  if (0 == r) {
    Log("nacl_av: multimedia initialized\n");
    nacl_multimedia.initialized = 1;
  } else {
    printf ("nacl_av: multimedia NOT initialized\n");
  }

  m = pthread_mutex_unlock(&nacl_multimedia_mutex);
  if (0 != m) {
    Fatal("nacl_multimedia_init: mutex unlock failed\n");
  }

  return RetValErrno(r);
}

int nacl_multimedia_shutdown() {
  /* if we're embedded...
   * verify thread id via tls.
   */
  int r;
  int m = pthread_mutex_lock(&nacl_multimedia_mutex);
  if (0 != m) {
    Fatal("nacl_multimedia_shutdown: mutex lock failed\n");
  }
  if (0 == nacl_multimedia.initialized) {
    Fatal("nacl_multimedia_shutdown: multimedia not initialized\n");
  }
  if (nacl_multimedia.thread_id != &nacl_multimedia_thread_id) {
    Fatal("nacl_multimedia_shutdown: called from different thread\n");
  }
  if (nacl_multimedia.embedded) {
    if (0 != nacl_multimedia.have_video) {
      Fatal("nacl_multimedia_shutdown: video not shutdown\n");
    }
  }
  r = __nacl_multimedia_shutdown();
  if (0 == r) {
    nacl_multimedia.embedded = 0;
    nacl_multimedia.subsystems = 0;
    nacl_multimedia.sr_subsystems = 0;
    nacl_multimedia.thread_id = NULL;
    nacl_multimedia.initialized = 0;
  }
  m = pthread_mutex_unlock(&nacl_multimedia_mutex);
  if (0 != m) {
    Fatal("nacl_multimedia_shutdown: mutex unlock failed\n");
  }
  return RetValErrno(r);
}

int nacl_multimedia_is_embedded(int *val) {
  int m;
  int r;
  m = pthread_mutex_lock(&nacl_multimedia_mutex);
  if (0 != m) {
    Fatal("nacl_multimedia_is_embedded: mutex lock failed\n");
  }
  /* multimedia must be inited */
  if (0 == nacl_multimedia.initialized) {
    Fatal("nacl_multimedia_is_embedded: multimedia not initialized\n");
  }
  *val = (NULL == nacl_multimedia.video_data) ? 0 : 1;
  r = 0;
  m = pthread_mutex_unlock(&nacl_multimedia_mutex);
  if (0 != m) {
    Fatal("nacl_multimedia_is_embedded: mutex unlock failed\n");
  }
  return RetValErrno(r);
}

int nacl_multimedia_get_embed_size(int *width, int *height) {
  int m;
  int r;
  m = pthread_mutex_lock(&nacl_multimedia_mutex);
  if (0 != m) {
    Fatal("nacl_multimedia_get_embed_size: mutex lock failed\n");
  }
  /* multimedia must be inited */
  if (0 == nacl_multimedia.initialized) {
    Fatal("nacl_multimedia_get_embed_size: multimedia not initialized\n");
  }
  if (nacl_multimedia.video_data) {
    *width = nacl_multimedia.video_data->u.h.video_width;
    *height = nacl_multimedia.video_data->u.h.video_height;
    r = 0;
  } else {
    r = -EIO;
  }
  m = pthread_mutex_unlock(&nacl_multimedia_mutex);
  if (0 != m) {
    Fatal("nacl_multimedia_get_embed_size: mutex unlock failed\n");
  }
  return RetValErrno(r);
}

int nacl_video_init(int width, int height) {
  int m;
  int r;
  m = pthread_mutex_lock(&nacl_multimedia_mutex);
  if (0 != m) {
    Fatal("nacl_video_init: mutex lock failed\n");
  }
  if (0 == nacl_multimedia.initialized) {
    Fatal("nacl_video_init: multimedia not initialized\n");
  }
  if (0 != nacl_multimedia.have_video) {
    Fatal("nacl_video_init: video already initialized\n");
  }
  if (nacl_multimedia.thread_id != &nacl_multimedia_thread_id) {
    Fatal("nacl_video_init: called from different thread\n");
  }
  if (0 != nacl_multimedia.embedded) {
    if (NULL != nacl_multimedia.video_data) {
      /* don't allow browser image to differ(?) */
      if ((width != nacl_multimedia.video_data->u.h.video_width) |
          (height != nacl_multimedia.video_data->u.h.video_height)) {
        printf("nacl_av: video size mismatch\n");
        printf("nacl_av: application width, height: %d %d\n", width, height);
        printf("nacl_av: browser width, height: %d %d\n",
            nacl_multimedia.video_data->u.h.video_width,
            nacl_multimedia.video_data->u.h.video_height);
        r = -EPERM;
        goto done;
      }
    } else {
      r = -EIO;
      goto done;
    }
    /* initialize bridge */
    r = nacl_video_bridge_init();
  } else {
    /* running in a seperate SDL window */
    r = __nacl_video_init(width, height);
  }
  /* on success, set that we have video */
  if (0 == r) {
    nacl_multimedia.have_video = 1;
  }

 done:
  m = pthread_mutex_unlock(&nacl_multimedia_mutex);
  if (0 != m) {
    Fatal("nacl_video_init: mutex unlock failed\n");
  }
  return RetValErrno(r);
}

int nacl_video_shutdown() {
  int m;
  int r;
  m = pthread_mutex_lock(&nacl_multimedia_mutex);
  if (0 != m) {
    Fatal("nacl_video_shutdown: mutex lock failed\n");
  }
  if (0 == nacl_multimedia.initialized) {
    Fatal("nacl_video_shutdown: multimedia not initialized\n");
  }
  if (0 == nacl_multimedia.have_video) {
    Fatal("nacl_video_shutdown: video not initialized\n");
  }
  if (nacl_multimedia.thread_id != &nacl_multimedia_thread_id) {
    Fatal("nacl_video_shutdown: called from different thread\n");
  }
  if (0 != nacl_multimedia.embedded) {
    /* shutdown bridge */
    r = nacl_video_bridge_shutdown();
  } else {
    /* running in a seperate SDL window */
    r = __nacl_video_shutdown();
  }
  /* on success, release video */
  if (0 == r) {
    nacl_multimedia.have_video = 0;
  }
  m = pthread_mutex_unlock(&nacl_multimedia_mutex);
  if (0 != m) {
    Fatal("nacl_video_shutdown: mutex unlock failed\n");
  }
  return RetValErrno(r);
}

int nacl_video_update(const void* data) {
  int m;
  int r;
  m = pthread_mutex_lock(&nacl_multimedia_mutex);
  if (0 != m) {
    Fatal("nacl_video_update: mutex lock failed\n");
  }
  if (0 == nacl_multimedia.initialized) {
    Fatal("nacl_video_update: multimedia not initialized\n");
  }
  if (0 == nacl_multimedia.have_video) {
    Fatal("nacl_video_update: video not initialized\n");
  }
  if (nacl_multimedia.thread_id != &nacl_multimedia_thread_id) {
    Fatal("nacl_video_update: called from different thread\n");
  }
  if (nacl_multimedia.embedded) {
    /* update via bridge to browser */
    r = nacl_video_bridge_update(data);
  } else {
    /* update via service runtime */
    r = __nacl_video_update(data);
  }
  m = pthread_mutex_unlock(&nacl_multimedia_mutex);
  if (0 != m) {
    Fatal("nacl_video_shutdown: mutex unlock failed\n");
  }
  return RetValErrno(r);
}

int nacl_video_poll_event(union NaClMultimediaEvent *event) {
  int m;
  int r;
  m = pthread_mutex_lock(&nacl_multimedia_mutex);
  if (0 != m) {
    Fatal("nacl_video_poll_event: mutex lock failed\n");
  }
  if (0 == nacl_multimedia.initialized) {
    Fatal("nacl_video_poll_event: multimedia not initialized\n");
  }
  if (0 == nacl_multimedia.have_video) {
    Fatal("nacl_video_poll_event: video not initialized\n");
  }
  if (nacl_multimedia.thread_id != &nacl_multimedia_thread_id) {
    Fatal("nacl_video_poll_event: called from different thread\n");
  }
  if (nacl_multimedia.embedded) {
    /* poll event via bridge to browser */
    r = nacl_video_bridge_poll_event(event);
  } else {
    /* poll event via service runtime */
    r = __nacl_video_poll_event(event);
  }
  m = pthread_mutex_unlock(&nacl_multimedia_mutex);
  if (0 != m) {
    Fatal("nacl_video_poll_event: mutex unlock failed\n");
  }
  return RetValErrno(r);
}

int nacl_audio_init(enum NaClAudioFormat format,
                    int desired_samples, int *obtained_samples) {
  int r = __nacl_audio_init(format, desired_samples, obtained_samples);
  return RetValErrno(r);
}

int nacl_audio_shutdown() {
  int r = __nacl_audio_shutdown();
  return RetValErrno(r);
}

int nacl_audio_stream(const void *data, size_t *size) {
  int r = __nacl_audio_stream(data, size);
  return RetValErrno(r);
}
