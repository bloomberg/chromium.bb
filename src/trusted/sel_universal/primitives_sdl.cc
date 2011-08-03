/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Concrete implemenatation of IMultimedia interface using SDL
 */

#include <SDL/SDL.h>
#include <SDL/SDL_timer.h>

#include <string.h>

#include <functional>
#include <queue>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"

// TODO(robertm): the next include file should be moved to src/untrusted
#include "native_client/src/trusted/sel_universal/primitives.h"
#include "native_client/src/trusted/sel_universal/workqueue.h"

// from sdl_ppapi_event_translator.cc
// TODO(robertm): add header when this becomes more complex
/* @IGNORE_LINES_FOR_CODE_HYGIENE[1] */
extern UserEvent* MakeInputEventFromSDLEvent(const SDL_Event& sdl_event);

// This file implements a IMultimedia interface using SDL

struct InfoVideo {
  int32_t width;
  int32_t height;
  int32_t format;
  int32_t bits_per_pixel;
  int32_t bytes_per_pixel;
  int32_t rmask;
  int32_t gmask;
  int32_t bmask;
  SDL_Surface* screen;
};


struct InfoAudio {
  int32_t frequency;
  int32_t channels;
  int32_t frame_size;
};


struct SDLInfo {
  int32_t initialized_sdl;
  InfoVideo video;
  InfoAudio audio;
};


class MultimediaSDL;


struct TimerEventState {
  MultimediaSDL* mm;
  UserEvent* event;

  TimerEventState(MultimediaSDL* the_mm, UserEvent* the_event)
    : mm(the_mm), event(the_event) {}
};


static Uint32 TimerCallBack(Uint32 interval, void* data);


// Wrap each call to SDL into Job so we can submit them to a workqueue
// which guarantees that they are all executed by the same thread.
class JobSdlInit: public Job {
 public:
  int result;
  JobSdlInit(SDLInfo* info, int width, int height, const char* title)
    : info_(info),
      width_(width),
      height_(height),
      title_(title)
  {}

 private:
  SDLInfo* info_;
  int width_;
  int height_;
  const char* title_;

  virtual void Action() {
    NaClLog(3, "JobSdlInit::Action\n");
    const int flags =  SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER;
    const uint32_t sdl_video_flags = SDL_DOUBLEBUF | SDL_HWSURFACE;

    memset(info_, 0, sizeof(*info_));
    if (SDL_Init(flags)) {
      NaClLog(LOG_FATAL, "MultimediaModuleInit: SDL_Init failed\n");
    }
    info_->initialized_sdl = 1;

    info_->video.screen = SDL_SetVideoMode(width_,
                                           height_,
                                           0,
                                           sdl_video_flags);

    if (!info_->video.screen) {
      NaClLog(LOG_FATAL, "NaClSysVideo_Init: SDL_SetVideoMode failed\n");
    }

    // width, height and format validated in top half
    info_->video.width = width_;
    info_->video.height = height_;
    // video format always BGRA
    info_->video.rmask = 0x00FF0000;
    info_->video.gmask = 0x0000FF00;
    info_->video.bmask = 0x000000FF;
    info_->video.bits_per_pixel = 32;
    info_->video.bytes_per_pixel = 4;

    // TODO(robertm): verify non-ownership of title_
    SDL_WM_SetCaption(title_, title_);
  }
};


class JobSdlQuit: public Job {
 private:
  SDLInfo* info_;

 public:
  explicit JobSdlQuit(SDLInfo* info) : info_(info) {}

  virtual void Action() {
    NaClLog(3, "JobSdlQuit::Action\n");
    if (!info_->initialized_sdl) {
      NaClLog(LOG_FATAL, "sdl not initialized\n");
    }

    SDL_Quit();
    memset(info_, 0, sizeof(*info_));
  }
};


class JobSdlAudioStart: public Job {
 private:
  SDLInfo* info_;

 public:
  explicit JobSdlAudioStart(SDLInfo* info) : info_(info) {}

  virtual void Action() {
    NaClLog(3, "JobSdlAudioStart::Action\n");
    if (!info_->initialized_sdl) {
      NaClLog(LOG_FATAL, "sdl not initialized\n");
    }
    SDL_PauseAudio(0);
  }
};



class JobSdlAudioStop: public Job {
 private:
  SDLInfo* info_;

 public:
  explicit JobSdlAudioStop(SDLInfo* info) : info_(info) {}

  virtual void Action() {
    NaClLog(3, "JobSdlAudioStop::Action\n");
    if (!info_->initialized_sdl) {
      NaClLog(LOG_FATAL, "sdl not initialized\n");
    }
    SDL_PauseAudio(1);
  }
};


class JobSdlAudioInit: public Job {
 private:
  SDLInfo* info_;
  AUDIO_CALLBACK cb_;
  void* cb_data_;

 public:
  explicit JobSdlAudioInit(SDLInfo* info,
                           int frequency,
                           int channels,
                           int frame_size,
                           AUDIO_CALLBACK cb,
                           void* cb_data)
    : info_(info), cb_(cb), cb_data_(cb_data) {
    info->audio.frequency = frequency;
    info->audio.channels = channels;
    info->audio.frame_size = frame_size;
  }

  virtual void Action() {
    NaClLog(3, "JobSdlAudioInit::Action\n");
    if (!info_->initialized_sdl) {
      NaClLog(LOG_FATAL, "sdl not initialized\n");
    }

    SDL_AudioSpec fmt;
    fmt.freq = info_->audio.frequency;
    fmt.format = AUDIO_S16;
    fmt.channels = info_->audio.channels;
    // NOTE: SDL seems to halve that the sample count for the callback
    //       so we compensate here by doubling
    fmt.samples = info_->audio.frame_size * 2;
    fmt.callback = cb_;
    fmt.userdata = cb_data_;
    NaClLog(LOG_INFO, "JobSdlAudioInit %d %d %d %d\n",
            fmt.freq, fmt.format, fmt.channels, fmt.samples);
    if (SDL_OpenAudio(&fmt, NULL) < 0) {
      NaClLog(LOG_FATAL, "could not initialize SDL audio\n");
    }
  }
};


class JobSdlUpdate: public Job {
 public:
  JobSdlUpdate(SDLInfo* info, const void* data) : info_(info), data_(data) {}

  virtual void Action() {
    NaClLog(3, "JobSdlUpdate::Action\n");
    SDL_Surface* image = NULL;
    InfoVideo* video_info = &info_->video;
    image = NULL;

    if (!info_->initialized_sdl) {
      NaClLog(LOG_FATAL, "MultimediaVideoUpdate: video not initialized\n");
    }

    image = SDL_CreateRGBSurfaceFrom((unsigned char*) data_,
                                     video_info->width,
                                     video_info->height,
                                     video_info->bits_per_pixel,
                                     video_info->width *
                                     video_info->bytes_per_pixel,
                                     video_info->rmask,
                                     video_info->gmask,
                                     video_info->bmask,
                                     0);
    if (NULL == image) {
      NaClLog(LOG_FATAL, "SDL_CreateRGBSurfaceFrom failed\n");
    }

    if (0 != SDL_SetAlpha(image, 0, 255)) {
      NaClLog(LOG_FATAL, "SDL_SetAlpha failed\n");
    }

    if (0 != SDL_BlitSurface(image, NULL, video_info->screen, NULL)) {
      NaClLog(LOG_FATAL, "SDL_BlitSurface failed\n");
    }

    if (0 != SDL_Flip(video_info->screen)) {
      NaClLog(LOG_FATAL, "SDL_Flip failed\n");
    }

    SDL_FreeSurface(image);
  }

 private:
  SDLInfo* info_;
  const void* data_;
};


class JobSdlEventPoll: public Job {
 public:
  JobSdlEventPoll(SDLInfo* info, bool poll)
    : info_(info), event_(NULL), poll_(poll) {}

  virtual void Action() {
    NaClLog(3, "JobSdlEventPoll::Action\n");

    if (!info_->initialized_sdl) {
      NaClLog(LOG_FATAL, "MultimediaEventPoll: sdl not initialized\n");
    }

    for (;;) {
      SDL_Event sdl_event;
      const int32_t result = poll_ ?
                             SDL_PollEvent(&sdl_event) :
                             SDL_WaitEvent(&sdl_event);
      if (result == 0) {
        if (poll_) {
          return;
        } else {
          NaClLog(LOG_WARNING, "SDL_WaitEvent failed\n");
          continue;
        }
      }

      // special handling for user events
      if (sdl_event.type == SDL_USEREVENT) {
        event_ = static_cast<UserEvent*>(sdl_event.user.data1);
        break;
      }

      event_ = MakeInputEventFromSDLEvent(sdl_event);
      if (event_ != NULL) {
        break;
      }
    }
  }

  UserEvent* GetEvent() {
    return event_;
  }

 private:
  SDLInfo* info_;
  UserEvent* event_;
  bool poll_;
};


// Again, the issue that all this wrapping magic is trying to work around
// is that SDL requires certain calls to be made from the same thread
class MultimediaSDL : public IMultimedia {
 public:
  MultimediaSDL(int width, int heigth, const char* title)
    : video_width_(width), video_height_(heigth) {
    NaClLog(1, "MultimediaSDL::Constructor\n");
    sdl_workqueue_.StartInAnotherThread();
    JobSdlInit job(&sdl_info_, width, heigth, title);
    sdl_workqueue_.JobPut(&job);
    job.Wait();
  }

  virtual ~MultimediaSDL() {
    JobSdlQuit job(&sdl_info_);
    sdl_workqueue_.JobPut(&job);
    job.Wait();
  }

  virtual int VideoWidth() {
    return video_width_;
  }

  virtual int VideoHeight() {
    return video_height_;
  }

  virtual int VideoBufferSize() {
    InfoVideo* video_info = &sdl_info_.video;
    return video_info->bytes_per_pixel *
      video_info->width *
      video_info->height;
  }

  virtual void VideoUpdate(const void* data) {
    JobSdlUpdate job(&sdl_info_, data);
    sdl_workqueue_.JobPut(&job);
    job.Wait();
  }

  virtual void PushUserEvent(UserEvent* event) {
    CHECK(event != NULL);
    // NOTE: this is intentionally not using the work queue
    // so we can unblock a queue that is waiting for an event
    NaClLog(2, "JobSdlPushUserEvent::Action [%s]\n",
            StringifyEvent(event).c_str());
    if (!sdl_info_.initialized_sdl) {
      NaClLog(LOG_FATAL, "sdl not initialized\n");
    }

    SDL_Event sdl_event;
    sdl_event.type = SDL_USEREVENT;
    sdl_event.user.code = 0;
    sdl_event.user.data1 = event;
    sdl_event.user.data2 = 0;
    // NOTE:  SDL_PushEvent copies the sdl_event
    SDL_PushEvent(&sdl_event);
  }

  virtual void PushDelayedUserEvent(int delay, UserEvent* event) {
    CHECK(event != NULL);
    NaClLog(2, "JobSdlPushDelayedUserEvent::Action %d [%s]\n",
            delay, StringifyEvent(event).c_str());
    // NOTE: this is intentionally not using the work queue
    // so we can unblock a queue that is waiting for an event
    if (!sdl_info_.initialized_sdl) {
      NaClLog(LOG_FATAL, "sdl not initialized\n");
    }

    if (delay == 0) {
      PushUserEvent(event);
      return;
    }
    // schedule a timer to inject the event into the event stream
    SDL_TimerID id =
      SDL_AddTimer(delay, TimerCallBack, new TimerEventState(this, event));
    CHECK(id != NULL);
  }

  virtual UserEvent* EventPoll() {
    JobSdlEventPoll job(&sdl_info_, true);
    sdl_workqueue_.JobPut(&job);
    job.Wait();
    return job.GetEvent();
  }

  virtual UserEvent* EventGet() {
    JobSdlEventPoll job(&sdl_info_, false);
    sdl_workqueue_.JobPut(&job);
    job.Wait();
    return job.GetEvent();
  }

  virtual void AudioInit16Bit(int frequency,
                              int channels,
                              int frame_size,
                              AUDIO_CALLBACK cb,
                              void* cb_data ) {
    JobSdlAudioInit job(&sdl_info_,
                        frequency,
                        channels,
                        frame_size,
                        cb,
                        cb_data);
    sdl_workqueue_.JobPut(&job);
    job.Wait();
  }

  virtual void AudioStart() {
    JobSdlAudioStart job(&sdl_info_);
    sdl_workqueue_.JobPut(&job);
    job.Wait();
  }

  virtual void AudioStop() {
    JobSdlAudioStop job(&sdl_info_);
    sdl_workqueue_.JobPut(&job);
    job.Wait();
  }

 private:
  int video_width_;
  int video_height_;
  ThreadedWorkQueue sdl_workqueue_;
  SDLInfo sdl_info_;
};

// This is called when a timed event encapsulatd by "data"
// is finally due. We simply inject into the event stream,
static Uint32 TimerCallBack(Uint32 interval, void* data) {
  CHECK(interval != 0);
  CHECK(data != NULL);
  UNREFERENCED_PARAMETER(interval);
  TimerEventState* state = reinterpret_cast<TimerEventState*>(data);
  NaClLog(2, "Timer: %d [%s]\n",
          interval, StringifyEvent(state->event).c_str());
  state->mm->PushUserEvent(state->event);
  // state was allocated in PushDelayedUserEvent()
  delete state;
  // stop timer: this MUST be a different value from interval
  return 0;
}

// Factor, so we can hide class MultimediaSDL from the outside world
IMultimedia* MakeEmuPrimitives(int width, int heigth, const char* title) {
  return new MultimediaSDL(width, heigth, title);
}
