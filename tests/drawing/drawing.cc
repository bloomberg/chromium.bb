/*
 * Copyright 2009, Google Inc.
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

// NaCl Drawing demo
//   Uses Anti-Grain Geometry open source rendering library to render into a
//   Native Client framebuffer.
//
//   See README.txt for information on how to download and build the
//   Anti-Grain Geometry libraries for Native Client.
//
//   See http://www.antigrain.com for more information on Anti-Grain Geometry

#include <errno.h>
#include <stdio.h>
#if !defined(STANDALONE)
#include <nacl/nacl_av.h>
#include <nacl/nacl_srpc.h>
#else
#include "native_client/common/standalone.h"
#endif

#include "native_client/tests/drawing/agg-2.5/include/agg_basics.h"
#include "native_client/tests/drawing/agg-2.5/include/agg_conv_stroke.h"
#include "native_client/tests/drawing/agg-2.5/include/agg_conv_transform.h"
#include "native_client/tests/drawing/agg-2.5/include/agg_ellipse.h"
#include "native_client/tests/drawing/agg-2.5/include/agg_path_storage.h"
#include "native_client/tests/drawing/agg-2.5/include/agg_pixfmt_rgba.h"
#include "native_client/tests/drawing/agg-2.5/include/agg_rendering_buffer.h"
#include "native_client/tests/drawing/agg-2.5/include/agg_rasterizer_outline_aa.h"
#include "native_client/tests/drawing/agg-2.5/include/agg_rasterizer_scanline_aa.h"
#include "native_client/tests/drawing/agg-2.5/include/agg_renderer_scanline.h"
#include "native_client/tests/drawing/agg-2.5/include/agg_scanline_u.h"


// global properties used to setup demo
static const int kMaxWindow = 4096;
static const int kMaxFrames = 10000000;
static int g_window_width = 512;
static int g_window_height = 512;
static int g_num_frames = 9999;

// Simple Surface structure to hold a raster rectangle
struct Surface {
  int width, height, pitch;
  uint32_t *pixels;
  Surface(int w, int h) { width = w;
                          height = h;
                          pitch = w;
                          pixels = new uint32_t[width * height]; }
  ~Surface() { delete[] pixels; }
};


// Drawing class holds information and functionality needed to render
class DrawingDemo {
 public:
  void Display();
  void Update();
  bool PollEvents();
  explicit DrawingDemo(Surface *s);
  ~DrawingDemo();

 private:
  Surface *surf_;
};


// This update loop is run once per frame.
// All of the Anti-Grain Geometry (AGG) rendering is done in this function.
// AGG renders straight into the DrawingDemo's surf_ pixel array.
void DrawingDemo::Update() {
  unsigned int i;
  agg::rendering_buffer rbuf((unsigned char *)surf_->pixels,
                             surf_->width,
                             surf_->height,
                             surf_->width * 4);
  // Setup agg and clear the framebuffer.
  // Use Native Client's bgra pixel format.
  agg::pixfmt_bgra32 pixf(rbuf);
  typedef agg::renderer_base<agg::pixfmt_bgra32> ren_base;
  ren_base ren(pixf);
  ren.clear(agg::rgba(0, 0, 0));
  agg::rasterizer_scanline_aa<> ras;
  agg::scanline_u8 sl;
  ras.reset();
  ras.gamma(agg::gamma_none());

  // Draw an array of filled circles with a cycling color spectrum.
  const double spectrum_violet = 380.0;
  const double spectrum_red = 780.0;
  static double outer_cycle = spectrum_violet;
  static double delta_outer_cycle = 0.4;
  double inner_cycle = outer_cycle;
  double delta_inner_cycle = 0.75;
  for (double y = 0.0; y <= surf_->height; y += 32.0) {
    for (double x = 0.0; x <= surf_->width; x += 32.0) {
      // Draw a small filled circle at x, y with radius 16
      // using inner_cycle as the fill color.
      agg::rgba color(inner_cycle, 1.0);
      agg::ellipse elp;
      elp.init(x, y, 16, 16, 80);
      ras.add_path(elp);
      agg::render_scanlines_aa_solid(ras, sl, ren, color);
      // Bounce color cycle between red & violet.
      inner_cycle += delta_inner_cycle;
      if ((inner_cycle > spectrum_red) || (inner_cycle < spectrum_violet)) {
        delta_inner_cycle = -delta_inner_cycle;
        inner_cycle += delta_inner_cycle;
      }
    }
  }
  // Bounce outer starting color between red & violet.
  outer_cycle += delta_outer_cycle;
  if ((outer_cycle > spectrum_red) || (outer_cycle < spectrum_violet)) {
    delta_outer_cycle = -delta_outer_cycle;
    outer_cycle += delta_outer_cycle;
  }

  // Draw a semi-translucent triangle over the background.
  // The triangle is drawn as an outline, using a pen width
  // of 24 pixels.  The call to close_polygon() ensures that
  // all three corners are cleanly mitered.
  agg::path_storage ps;
  agg::conv_stroke<agg::path_storage> pg(ps);
  pg.width(24.0);
  ps.remove_all();
  ps.move_to(96, 160);
  ps.line_to(384, 256);
  ps.line_to(192, 352);
  ps.line_to(96, 160);
  ps.close_polygon();
  ras.add_path(pg);
  agg::render_scanlines_aa_solid(ras, sl, ren, agg::rgba8(255, 255, 255, 160));
}


// Displays software rendered image on the screen
void DrawingDemo::Display() {
  int r;
  r = nacl_video_update(surf_->pixels);
  if (-1 == r) {
    printf("nacl_video_update() returned %d\n", errno);
  }
}


// Polls events and services them.
bool DrawingDemo::PollEvents() {
  NaClMultimediaEvent event;
  while (0 == nacl_video_poll_event(&event)) {
    if (event.type == NACL_EVENT_QUIT) {
      return false;
    }
  }
  return true;
}


// Sets up and initializes DrawingDemo.
DrawingDemo::DrawingDemo(Surface *surf) {
  surf_ = surf;
}


// Frees up resources.
DrawingDemo::~DrawingDemo() {
}


// Runs the demo and animate the image for kNumFrames
void RunDemo(Surface *surface) {
  DrawingDemo demo(surface);

  for (int i = 0; i < g_num_frames; ++i) {
    demo.Update();
    demo.Display();
    printf("Frame: %04d\b\b\b\b\b\b\b\b\b\b\b", i);
    fflush(stdout);
    if (!demo.PollEvents())
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
        printf("DrawingDemo\n");
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
  ParseCmdLineArgs(argc, argv);
  Surface *surface = Initialize();
  RunDemo(surface);
  Shutdown(surface);
  return 0;
}
