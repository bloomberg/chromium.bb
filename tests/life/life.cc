/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// NaCl Life demo
//   Uses a brute force algorithm to render "Life."

#include <errno.h>
#include <math.h>
#if defined(HAVE_THREADS)
#include <pthread.h>
#include <semaphore.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <cstdlib>
#include <iostream>
#if !defined(STANDALONE)
#include <nacl/nacl_av.h>
#include <nacl/nacl_srpc.h>
#else
#include "native_client/common/standalone.h"
#endif

// global properties used to setup life
const int kMaxWindow = 4096;
const int kMaxFrames = 10000000;
int g_window_width = 512;
int g_window_height = 512;
int g_num_frames = 50000;

// seed for rand_r() - we only call rand_r from main thread.
static unsigned int gSeed = 0xC0DE533D;

// random number helper
// binary rand() returns 0 or 1
inline unsigned char brand() {
  return static_cast<unsigned char>(rand_r(&gSeed) & 1);
}

// build a packed color
inline uint32_t MakeRGBA(uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
  return (((a) << 24) | ((r) << 16) | ((g) << 8) | (b));
}


struct Surface {
  int width, height, pitch;
  uint32_t *pixels;
  Surface(int w, int h) { width = w; height = h; pitch = w;
                          pixels = new uint32_t[width * height]; }
  ~Surface() { delete[] pixels; }
};


// Life class holds information and functionality needed to render
// life into an SDL surface.

class Life {
 public:
  void Plot(int x, int y);
  void SwapBuffers();
  void Stir();
  void Draw();
  void Update();
  bool PollEvents();
  explicit Life(Surface *s);
  ~Life();

 private:
  Surface *surf_;
  int width_, height_, pitch_;
  bool scribble_;
  char *cell_in_;
  char *cell_out_;
};


void Life::Plot(int x, int y) {
  if (x < 0) return;
  if (x >= width_) return;
  if (y < 0) return;
  if (y >= height_) return;
  *(cell_in_ + x + y * width_) = 1;
}


// swap double buffer
void Life::SwapBuffers() {
  char *temp = cell_in_;
  cell_in_ = cell_out_;
  cell_out_ = temp;
}


// Applies an external influence, "Stirs the pot"
// In this case, tweaks the top, bottom, left, right
// borders with random state.
void Life::Stir() {
  for (int i = 0; i < width_; ++i) {
    cell_in_[i] = brand();
    cell_in_[i + (height_ - 1) * width_] = brand();
  }
  for (int i = 0; i < height_; ++i) {
    cell_in_[i * width_] = brand();
    cell_in_[i * width_ + (width_ - 2)] = brand();
  }
}


// Runs a simple sim to update Life.  This update loop
// is run once per frame.
void Life::Update() {
  // map neighbor count to color
  static unsigned int colors[18] = {
      MakeRGBA(0x00, 0x00, 0x00, 0x00),
      MakeRGBA(0x00, 0x40, 0x00, 0x00),
      MakeRGBA(0x00, 0x60, 0x00, 0x00),
      MakeRGBA(0x00, 0x80, 0x00, 0x00),
      MakeRGBA(0x00, 0xA0, 0x00, 0x00),
      MakeRGBA(0x00, 0xC0, 0x00, 0x00),
      MakeRGBA(0x00, 0xE0, 0x00, 0x00),
      MakeRGBA(0x00, 0x00, 0x00, 0x00),
      MakeRGBA(0x00, 0x40, 0x00, 0x00),
      MakeRGBA(0x00, 0x60, 0x00, 0x00),
      MakeRGBA(0x00, 0x80, 0x00, 0x00),
      MakeRGBA(0x00, 0xA0, 0x00, 0x00),
      MakeRGBA(0x00, 0xC0, 0x00, 0x00),
      MakeRGBA(0x00, 0xE0, 0x00, 0x00),
      MakeRGBA(0x00, 0xFF, 0x00, 0x00),
      MakeRGBA(0x00, 0xFF, 0x00, 0x00),
      MakeRGBA(0x00, 0xFF, 0x00, 0x00),
      MakeRGBA(0x00, 0xFF, 0x00, 0x00),
  };
  // map neighbor count to alive/dead
  static char replace[18] = {
      0, 0, 0, 1, 0, 0, 0, 0,  // row for center cell dead
      0, 0, 1, 1, 0, 0, 0, 0,  // row for center cell alive
  };
  // do neighbor sumation; apply rules, output pixel color
  for (int y = 1; y < (surf_->height - 1); ++y) {
    char *src0 = cell_in_ + (y - 1) * surf_->width;
    char *src1 = cell_in_ + (y) * surf_->width;
    char *src2 = cell_in_ + (y + 1) * surf_->width;
    int count;
    unsigned int color;
    char *dst = cell_out_ + (y) * surf_->width;
    uint32_t *pixels = surf_->pixels + y * surf_->pitch;
    for (int x = 1; x < (surf_->width - 1); ++x) {
      // build sum, weight center by 8x
      count = src0[-1] +     src0[0] +        src0[1] +
              src1[-1] +     src1[0] * 8 +    src1[1] +
              src2[-1] +     src2[0] +        src2[1];
      color = colors[count];
      *pixels++ = color;
      *dst++ = replace[count];
      ++src0, ++src1, ++src2;
    }
  }
}


// Copies sw rendered life image to screen
void Life::Draw() {
  int r;
  r = nacl_video_update(surf_->pixels);
  if (-1 == r) {
    printf("nacl_video_update() returned %d\n", errno);
  }
}


// polls events and services them
bool Life::PollEvents() {
  NaClMultimediaEvent event;
  while (0 == nacl_video_poll_event(&event)) {
    bool plot = false;
    if (event.type == NACL_EVENT_MOUSE_BUTTON_DOWN) {
      scribble_ = true;
      plot = true;
    }
    if (event.type == NACL_EVENT_MOUSE_BUTTON_UP) {
      scribble_ = false;
    }
    if (event.type == NACL_EVENT_MOUSE_MOTION) {
      plot = scribble_;
    }
    if (event.type == NACL_EVENT_QUIT) {
      return false;
    }
    if (plot) {
      // place a blob of life
      Plot(event.motion.x - 1, event.motion.y - 1);
      Plot(event.motion.x + 0, event.motion.y - 1);
      Plot(event.motion.x + 1, event.motion.y - 1);
      Plot(event.motion.x - 1, event.motion.y + 0);
      Plot(event.motion.x + 0, event.motion.y + 0);
      Plot(event.motion.x + 1, event.motion.y + 0);
      Plot(event.motion.x - 1, event.motion.y + 1);
      Plot(event.motion.x + 0, event.motion.y + 1);
      Plot(event.motion.x + 1, event.motion.y + 1);
    }
  }
  return true;
}


// Sets up and initializes Life data structures.  Seeds with random values.
Life::Life(Surface *surf) {
  surf_ = surf;
  width_ = surf->width;
  height_ = surf->height;
  pitch_ = surf->pitch;
  scribble_ = false;
  cell_in_ = new char[surf->width * surf->height];
  cell_out_ = new char[surf->width * surf->height];
  for (int i = 0; i < surf->width * surf->height; ++i) {
    cell_in_[i] = cell_out_[i] = 0;
  }
}


// Frees up resources.
Life::~Life() {
  delete[] cell_in_;
  delete[] cell_out_;
}


// Runs the life demo and animate the image for kNumFrames
void RunDemo(Surface *surface) {
  Life life(surface);

  for (int i = 0; i < g_num_frames; ++i) {
    life.Stir();
    life.Update();
    life.Draw();
    life.SwapBuffers();
    printf("Frame: %04d\b\b\b\b\b\b\b\b\b\b\b", i);
    fflush(stdout);
    if (!life.PollEvents())
      break;
  }
}


// Initializes a window buffer.
Surface* Initialize() {
  int r;
  int width;
  int height;
  r = nacl_multimedia_init(NACL_SUBSYSTEM_VIDEO | NACL_SUBSYSTEM_EMBED);
  if (-1 == r) {
    printf("Multimedia system failed to initialize!  errno: %d\n", errno);
    exit(-1);
  }
  // if this call succeeds, use width & height from embedded html
  r = nacl_multimedia_get_embed_size(&width, &height);
  if (0 == r) {
    g_window_width = width;
    g_window_height = height;
  }
  r = nacl_video_init(g_window_width, g_window_height);
  if (-1 == r) {
    printf("Video subsystem failed to initialize!  errno; %d\n", errno);
    exit(-1);
  }
  Surface *surface = new Surface(g_window_width, g_window_height);
  return surface;
}


// Frees window buffer.
void Shutdown(Surface *surface) {
  delete surface;
  nacl_video_shutdown();
  nacl_multimedia_shutdown();
}


// If user specified options on cmd line, parse them
// here and update global settings as needed.
void ParseCmdLineArgs(int argc, char **argv) {
  // look for cmd line args
  if (argc > 1) {
    for (int i = 1; i < argc; i++) {
      if (argv[i] == strstr(argv[i], "-w")) {
        int w = atoi(&argv[i][2]);
        if ((w > 0) && (w < kMaxWindow)) {
          g_window_width = w;
        }
      } else if (argv[i] == strstr(argv[i], "-h")) {
        int h = atoi(&argv[i][2]);
        if ((h > 0) && (h < kMaxWindow)) {
          g_window_height = h;
        }
      } else if (argv[i] == strstr(argv[i], "-f")) {
        int f = atoi(&argv[i][2]);
        if ((f > 0) && (f < kMaxFrames)) {
          g_num_frames = f;
        }
      } else {
        printf("Life SDL Demo\n");
        printf("usage: -w<n>   width of window.\n");
        printf("       -h<n>   height of window.\n");
        printf("       -f<n>   number of frames.\n");
        printf("       --help  show this screen.\n");
        exit(0);
      }
    }
  }
}


// Parses cmd line options, initializes surface, runs the demo & shuts down.
int main(int argc, char **argv) {
#if !defined(STANDALONE)
  if (!NaClSrpcModuleInit()) {
    return 1;
  }
  if (!NaClSrpcAcceptClientOnThread(__kNaClSrpcHandlers)) {
    return 1;
  }
#endif  // !defined(STANDALONE)
  ParseCmdLineArgs(argc, argv);
  Surface *surface = Initialize();
  RunDemo(surface);
  Shutdown(surface);
#if !defined(STANDALONE)
  NaClSrpcModuleFini();
#endif  // !defined(STANDALONE)
  return 0;
}
