/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Concrete implemenatation of IMultimedia interface using SDL
 */

#include <SDL.h>
#include <string.h>

#include <functional>
#include <queue>
#include <string>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"

#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/sel_universal/multimedia.h"
#include "native_client/src/trusted/sel_universal/workqueue.h"

// This file implements a IMultimedia interface using SDL

typedef struct InfoVideo {
  int32_t width;
  int32_t height;
  int32_t format;
  int32_t bits_per_pixel;
  int32_t bytes_per_pixel;
  int32_t rmask;
  int32_t gmask;
  int32_t bmask;
  SDL_Surface* screen;
} InfoVideo;


typedef struct SDLInfo {
  int32_t initialized_sdl;
  int32_t initialized_sdl_video;
  InfoVideo video;
} InfoMultimedia;


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
    const int flags =  SDL_INIT_VIDEO;
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

    info_->initialized_sdl_video = 1;
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


class JobSdlUpdate: public Job {
 private:
  SDLInfo* info_;
  const void* data_;

 public:
  JobSdlUpdate(SDLInfo* info, const void* data) : info_(info), data_(data) {}

  virtual void Action() {
    NaClLog(3, "JobSdlUpdate::Action\n");
    SDL_Surface* image = NULL;
    InfoVideo* video_info = &info_->video;
    image = NULL;

    if (!info_->initialized_sdl_video) {
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
};


// Again, the issue that all this wrapping magic is trying to work around
// is that SDL requires certain calls to be made from the same thread
class MultimediaSDL : public IMultimedia {
 private:
  ThreadedWorkQueue sdl_workqueue_;
  SDLInfo sdl_info_;

 public:
  MultimediaSDL(int width, int heigth, const char* title) {
    NaClLog(2, "MultimediaSDL::Constructor\n");
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
};

// Factor, so we can hide class MultimediaSDL from the outside world
IMultimedia* MakeMultimediaSDL(int width, int heigth, const char* title) {
  return new MultimediaSDL(width, heigth, title);
}
