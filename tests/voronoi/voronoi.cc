/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// NaCl Voronoi threading demo
//   Uses a brute force algorithm to render the Voronoi diagram.
//   Illustrates use of threads, semaphores, and a mutex.
//   Should scale well from 1 to 16 cores.
//

// defines
// TODO(nfullagar): fix/cleanup when various parts of system come online
#define HAVE_THREADS            // enable if pthreads & semaphores available
// #define HAVE_SYSCONF         // enable if sysconf() is available

#include <errno.h>
#include <math.h>
#if defined(HAVE_THREADS)
#include <pthread.h>
#include <semaphore.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if !defined(STANDALONE)
#include <nacl/nacl_av.h>
#include <nacl/nacl_srpc.h>
#else
#include "native_client/common/standalone.h"
#endif
#include "native_client/common/worker.h"

// global properties used to setup voronoi
const int kMinRectSize = 4;
const int kStartRecurseSize = 64;
/* TODO(robertm): consider using UGE_VALF */
const float kHugeZ = 1.0e38f;
const int kMaxWindow = 4096;
const int kMaxPoints = 8192;
const int kMaxFrames = 1000000;
const float kPI = M_PI;
const float kTwoPI = kPI * 2.0f;
int g_window_width = 512;
int g_window_height = 512;
int g_num_points = 256;
int g_num_frames = 10000;
bool g_ask_sysconf = true;
int g_num_threads = 4;           // possibly overridden
int g_num_regions = 4;           // possibly overridden
bool g_multi_threading = false;  // can be overridden on cmd line
bool g_render_dots = true;       // can be overridden on cmd line
bool g_use_av = true;            // can be overridden on cmd line

// random number helper
inline unsigned char rand255() {
  return lrand48() & 0xff;
}

// random number helper
inline float frand() {
  return drand48();
}

// build a packed color
inline uint32_t MakeRGBA(uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
  return (((a) << 24) | ((r) << 16) | ((g) << 8) | (b));
}


// Vec2, simple 2D vector

struct Vec2 {
  float x, y;
  Vec2() { ; }
  Vec2(float px, float py) {
    x = px;
    y = py;
  }
  void Set(float px, float py) {
    x = px;
    y = py;
  }
};


struct Surface {
  int width, height, pitch;
  uint32_t *pixels;
  Surface(int w, int h) {
    width = w;
    height = h;
    pitch = w;
    pixels = new uint32_t[w * h];
  }
  ~Surface() {
    delete[] pixels;
  }
};

Surface *g_surface;

// Voronoi class holds information and functionality needed to render
// voronoi structures into an SDL surface.

class Voronoi {
 public:
  // methods prefixed with 'w' are only called by worker threads!
  // (unless g_multi_threading is false)
  int wCell(float x, float y);
  inline void wFillSpan(uint32_t *pixels, uint32_t color, int width);
  void wRenderTile(int x, int y, int w, int h);
  void wProcessTile(int x, int y, int w, int h);
  void wSubdivide(int x, int y, int w, int h);
  void wMakeRect(int region, int *x, int *y, int *w, int *h);
  bool wTestRect(int *m, int x, int y, int w, int h);
  void wFillRect(int x, int y, int w, int h, uint32_t color);
  void wRenderRect(int x0, int y0, int x1, int y1);
  void wWorkerThreadEntry();

  // these methods are only called by the main thread
  bool CreateWorkerThreads(const int num);
  void UpdateSim();
  void ParallelRender();
  void SequentialRender();
  void RenderDot(float x, float y, uint32_t color1, uint32_t color2);
  void SuperimposePositions();
  void Render();
  void Draw();
  bool PollEvents();
  Voronoi(Surface *s, int count, int numRegions, bool multithreading);
  ~Voronoi();

 private:
  Surface *surf_;
  Vec2 *positions_;
  Vec2 *velocities_;
  uint32_t *colors_;
  int count_;
  int num_regions_;
  volatile bool exiting_;
  int width_, height_, pitch_;
  WorkerThreadManager *workers_;
};


// This is the core of the Voronoi calculation.  At a given point on the
// screen, iterate through all voronoi positions and render them as 3D cones.
// We're looking for the voronoi cell that generates the closest z value.
// (not really cones - since it is all realative, we avoid doing the
// expensive sqrt and test against z*z instead)
// If multithreading, this function is only called by the worker threads.
int Voronoi::wCell(float x, float y) {
  int n = 0;
  float zz = kHugeZ;
  Vec2 *pos = positions_;
  for (int i = 0; i < count_; ++i) {
    // measured 5.18 cycles per iteration on a core2
    float dx = x - pos[i].x;
    float dy = y - pos[i].y;
    float dd = (dx * dx + dy * dy);
    // if d*d < z*z
    if (dd < zz) {
      zz = dd;
      n = i;
    }
  }
  return n;
}


// Given a region r, derive a rectangle.  Currently this function slices
// the main voronoi buffer into equal sized rows.  This function is used
// to convert a mutex guarded counter into a rectangular region for a given
// worker thread to process.  This rectangle shouldn't overlap with work
// being done by other workers.
// If multithreading, this function is only called by the worker threads.
void Voronoi::wMakeRect(int r, int *x, int *y, int *w, int *h) {
  const int dy = height_ / num_regions_;
  *x = 0;
  *w = width_;
  *y = r * dy;
  *h = dy;
}


// Test 4 corners of a rectangle to see if they all belong to the same
// voronoi cell.  Each test is expensive so bail asap.  Returns true
// if all 4 corners match.
// If multithreading, this function is only called by the worker threads.
bool Voronoi::wTestRect(int *m, int x, int y, int w, int h) {
  // each test is expensive, so exit ASAP
  const int m0 = this->wCell(x, y);
  const int m1 = this->wCell(x + w - 1, y);
  if (m0 != m1) return false;
  const int m2 = this->wCell(x, y + h - 1);
  if (m0 != m2) return false;
  const int m3 = this->wCell(x + w - 1, y + h - 1);
  if (m0 != m3) return false;
  // all 4 corners are the same color
  *m = m0;
  return true;
}


// Quickly fill a span of pixels with a solid color.  Assumes
// span width is divisible by 4.
// If multithreading, this function is only called by the worker threads.
inline void Voronoi::wFillSpan(uint32_t *pixels, uint32_t color, int width) {
  for (int i = 0; i < width; i += 4) {
    *pixels++ = color;
    *pixels++ = color;
    *pixels++ = color;
    *pixels++ = color;
  }
}


// Quickly fill a rectangle with a solid color.  Assumes
// the width w parameter is evenly divisible by 4.
// If multithreading, this function is only called by the worker threads.
void Voronoi::wFillRect(int x, int y, int w, int h, uint32_t color) {
  uint32_t *pixels = surf_->pixels + y * surf_->pitch + x;
  const uint32_t nextline = surf_->pitch;
  for (int j = 0; j < h; j++) {
    this->wFillSpan(pixels, color, w);
    pixels += nextline;
  }
}


// When recursive subdivision reaches a certain minimum without finding a
// rectangle that has four matching corners belonging to the same voronoi
// cell, this function will break the retangular 'tile' into smaller scanlines
//  and look for opportunities to quick fill at the scanline level.  If the
// scanline can't be quick filled, it will slow down even further and compute
// voronoi membership per pixel.
void Voronoi::wRenderTile(int x, int y, int w, int h) {
  // rip through a tile
  uint32_t *pixels = surf_->pixels + y * surf_->pitch + x;
  const uint32_t nextline = surf_->pitch;
  for (int j = 0; j < h; j++) {
    // get start and end cell values
    int ms = this->wCell(x + 0, y + j);
    int me = this->wCell(x + w - 1, y + j);
    // if the end points are the same, quick fill the span
    if (ms == me) {
      this->wFillSpan(pixels, colors_[ms], w);
    } else {
      // else compute each pixel in the span... this is the slow part!
      uint32_t *p = pixels;
      *p++ = colors_[ms];
      for (int i = 1; i < (w - 1); i++) {
        int m = this->wCell(x + i, y + j);
        *p++ = colors_[m];
      }
      *p++ = colors_[me];
    }
    pixels += nextline;
  }
}


// Take a rectangular region and do one of -
//   If all four corners below to the same voronoi cell, stop recursion and
// quick fill the rectangle.
//   If the minimum rectangle size has been reached, break out of recursion
// and process the rectangle.  This small rectangle is called a tile.
//   Otherwise, keep recursively subdividing the rectangle into 4 equally
// sized smaller rectangles.
// Note: at the moment, these will always be squares, not rectangles.
// If multithreading, this function is only called by the worker threads.
void Voronoi::wSubdivide(int x, int y, int w, int h) {
  int m;
  // if all 4 corners are equal, quick fill interior
  if (this->wTestRect(&m, x, y, w, h)) {
    this->wFillRect(x, y, w, h, colors_[m]);
  } else {
    // did we reach the minimum rectangle size?
    if ((w <= kMinRectSize) || (h <= kMinRectSize)) {
      this->wRenderTile(x, y, w, h);
    } else {
      // else recurse into smaller rectangles
      const int half_w = w / 2;
      const int half_h = h / 2;
      this->wSubdivide(x, y, half_w, half_h);
      this->wSubdivide(x + half_w, y, half_w, half_h);
      this->wSubdivide(x, y + half_h, half_w, half_h);
      this->wSubdivide(x + half_w, y + half_h, half_w, half_h);
    }
  }
}


// This function cuts up the rectangle into power of 2 sized squares.  It
// assumes the input rectangle w & h are evenly divisible by
// kStartRecurseSize.
// If multithreading, this function is only called by the worker threads.
void Voronoi::wRenderRect(int x, int y, int w, int h) {
  for (int iy = y; iy < (y + h); iy += kStartRecurseSize) {
    for (int ix = x; ix < (x + w); ix += kStartRecurseSize) {
      this->wSubdivide(ix, iy, kStartRecurseSize, kStartRecurseSize);
    }
  }
}


// Thread entry point Voronoi::wWorkerThread()
// This is the main loop for the worker thread(s).  It waits for work
// by testing a semaphore, which will sleep this thread until work arrives.
// It then grabs a mutex protected counter (which is also decremented)
// and uses this value to determine which subregion this thread should be
// processing.  When rendering is finished the worker then pings the main
// thread via PostDone() semaphore.
// If multithreading, this function is only called by the worker threads.
void Voronoi::wWorkerThreadEntry() {
  // we're a 'detached' thread...
  // (so we should automagically die when the main thread exits)
  while (!exiting_) {
    // wait for some work
    workers_->WaitWork();
    // if main thread is exiting, so are we
    if (exiting_) break;
    // okay, grab region to work on from worker counter
    int region = workers_->DecCounter();
    if (region < 0) {
      // This indicates we're not sync'ing properly
      fprintf(stderr, "Region value went negative!\n");
      exit(-1);
    }
    // convert region # into x0, y0, x1, y1 rectangle
    int x, y, w, h;
    this->wMakeRect(region, &x, &y, &w, &h);

    // render this rectangle
    this->wRenderRect(x, y, w, h);

    // let main thread know we've finished a region
    workers_->PostDone();
  }
}


// Entry point for worker thread.  (Can't really pass a member function to
// pthread_create(), so we have to do this little round-about)
// If multithreading, this function is only called by the worker threads.
void* wWorkerThreadEntry(void *args) {
  // unpack this pointer
  Voronoi *pVoronoi = reinterpret_cast<Voronoi*>(args);
  pVoronoi->wWorkerThreadEntry();
  return NULL;
}


// Function Voronoi::CreateWorkerThreads()
// Create worker threads and pass along our this pointer.
// If workers_ is NULL, we're running in non-threaded mode.
bool Voronoi::CreateWorkerThreads(const int num) {
  if (NULL != workers_) {
    return workers_->CreateThreadPool(num, ::wWorkerThreadEntry, this);
  } else {
    return true;
  }
}


// Function Voronoi::UpdateSim()
// Run a simple sim to move the voronoi positions.  This update loop
// is run once per frame.  Called from the main thread only, and only
// when the worker threads are idle.
void Voronoi::UpdateSim() {
  static float ang = 0.0f;
  ang += 0.002f;
  if (ang > kTwoPI) {
    ang = ang - kTwoPI;
  }
  float z = cosf(ang) * 3.0f;
  // push the points around on the screen for animation
  for (int j = 0; j < count_; j++) {
    positions_[j].x += (velocities_[j].x) * z;
    positions_[j].y += (velocities_[j].y) * z;
  }
}


// This is the main thread's entry point to dispatch rendering of the Voronoi.
// First, it sets the region counter to its max value.  This mutex guarded
// counter is how worker threads determine which region of the diagram they
// should be working on.  Then it pings the PostWork semaphore multiple times
// to wake up the sleeping worker threads.  Finally, this thread needs to
// sleep a little by waiting for the workers to finish.  It does that by
// waiting on the WaitDone semaphore.
void Voronoi::ParallelRender() {
  // At this point, all worker threads are idle and sleeping
  // setup barrier counter before we wake workers
  workers_->SetCounter(num_regions_);
  // wake up the workers
  for (int i = 0; i < num_regions_; i++) {
    workers_->PostWork();
  }
  // At this point, all worker threads are awake and busy grabbing
  // work assignments by reading and decrementing the counter.
  // This main thread will sleep on the semaphore below while
  // waiting for the workers to finish.
  for (int i = 0; i < num_regions_; i++) {
    workers_->WaitDone();
  }
  // verify that our counter is where we expect it to be
  int c = workers_->DecCounter();
  if (-1 != c) {
    fprintf(stderr, "We're not syncing correctly! (%d)\n", c);
    exit(-1);
  }
  // At this point, all worker threads are idle and sleeping.
  // The main thread is free to muck with shared data, such
  // as updating all the positions of the voronoi cells in the sim routine.
}


// Performs all rendering from the main thread.
void Voronoi::SequentialRender() {
  this->wRenderRect(0, 0, width_, height_);
}


// Renders a small diamond shaped dot at x, y clipped against the window
void Voronoi::RenderDot(float x, float y, uint32_t color1, uint32_t color2) {
  const int ix = static_cast<int>(x);
  const int iy = static_cast<int>(y);
  // clip it against window
  if (ix < 1) return;
  if (ix >= (width_ - 1)) return;
  if (iy < 1) return;
  if (iy >= (height_ - 1)) return;
  uint32_t *pixel = static_cast<uint32_t *>(surf_->pixels) + iy * pitch_ + ix;
  // render dot as a small diamond
  *pixel = color1;
  *(pixel - 1) = color2;
  *(pixel + 1) = color2;
  *(pixel - pitch_) = color2;
  *(pixel + pitch_) = color2;
}


// Superimposes dots on the positions.
void Voronoi::SuperimposePositions() {
  const uint32_t white = MakeRGBA(255, 255, 255, 255);
  const uint32_t gray = MakeRGBA(192, 192, 192, 255);
  for (int i = 0; i < count_; i++) {
    this->RenderDot(positions_[i].x, positions_[i].y, white, gray);
  }
}


// Renders the Voronoi diagram
// Pick either parallel or sequential rendering implementation.
void Voronoi::Render() {
  if (NULL == workers_) {
    this->SequentialRender();
  } else {
    this->ParallelRender();
  }
  if (g_render_dots) {
    this->SuperimposePositions();
  }
}


// Copies sw rendered voronoi image to screen
void Voronoi::Draw() {
  if (g_use_av) {
    int r;
    r = nacl_video_update(surf_->pixels);
    if (-1 == r) {
      printf("nacl_video_update() returned %d\n", errno);
    }
  }
}


// Polls event queue
// returns false if the user tries to quit application early
bool Voronoi::PollEvents() {
  if (g_use_av) {
    NaClMultimediaEvent event;
    // bare minimum event servicing
    while (0 == nacl_video_poll_event(&event)) {
      if (NACL_EVENT_QUIT == event.type)
        return false;
    }
  }
  return true;
}


// Sets up and initializes the voronoi data structures.
// Seeds positions and velocities with random values.
Voronoi::Voronoi(Surface *surf, int count, int numRegions, bool multi) {
  count_ = count;
  surf_ = surf;
  num_regions_ = numRegions;
  exiting_ = false;
  width_ = surf->width;
  height_ = surf->height;
  pitch_ = surf->pitch;
  positions_ = new Vec2[count_];
  velocities_ = new Vec2[count_];
  colors_ = new uint32_t[count_];
  workers_ = multi ? new WorkerThreadManager() : NULL;
  for (int i = 0; i < count_; i++) {
    // random initial start position
    const float x = frand() * width_;
    const float y = frand() * height_;
    positions_[i].Set(x, y);
    // random directional velocity ( -1..1, -1..1 )
    const float speed = 0.25f;
    const float u = (frand() * 2.0f - 1.0f) * speed;
    const float v = (frand() * 2.0f - 1.0f) * speed;
    velocities_[i].Set(u, v);
    // 'unique' color (well... unique enough for our purposes)
    uint32_t c3 = rand255();
    uint32_t c2 = rand255();
    uint32_t c1 = rand255();
    colors_[i] = MakeRGBA(c1, c2, c3, 255);
  }
}


// Frees up resources.
Voronoi::~Voronoi() {
  exiting_ = true;
  // wake up the worker threads from their slumber
  if (workers_) {
    workers_->PostWorkAll();
    workers_->JoinAll();
    delete workers_;
  }
  delete[] positions_;
  delete[] velocities_;
  delete[] colors_;
}


// Runs the voronoi demo and animates the image for g_num_frames
void RunDemo(Surface *surface) {
  Voronoi voronoi(surface, g_num_points, g_num_regions, g_multi_threading);

  if (voronoi.CreateWorkerThreads(g_num_threads)) {
    for (int i = 0; i < g_num_frames; ++i) {
      voronoi.UpdateSim();
      voronoi.Render();
      voronoi.Draw();
      if (g_use_av) {
        printf("Frame: %04d\b\b\b\b\b\b\b\b\b\b\b", i);
        fflush(stdout);
      }
      if (!voronoi.PollEvents())
        break;
    }
  }
}


// Initializes a window buffer.
Surface* Initialize() {
  if (g_use_av) {
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
  }
  Surface *surface = new Surface(g_window_width, g_window_height);
  return surface;
}


int Checksum() {
  int size = g_surface->width * g_surface->height;
  uint32_t checksum = 0;
  for (int i = 0; i < size; ++i) {
    checksum = checksum ^ (g_surface->pixels[i] ^ lrand48());
    srand48(checksum);
  }

  return static_cast<unsigned int>(checksum);
}


// Frees a window buffer.
void Shutdown() {
  if (g_use_av) {
    nacl_video_shutdown();
    nacl_multimedia_shutdown();
  }
  delete g_surface;
}




// Clamps input to the max we can realistically support.
// (ie can only subdivide so much if window size is small to begin with)
int ClampThreads(int num) {
  // maximum threads spawnable limited by window size
  const int max = g_window_height / kStartRecurseSize;
  if (num > max) {
    return max;
  }
  return num;
}


// If user specifies options on cmd line, parse them
// here and update global settings as needed.
void ParseCmdLineArgs(int argc, char **argv) {
  bool test_mode = false;
  // look for cmd line args
  if (argc > 1) {
    for (int i = 1; i < argc; i++) {
      if (argv[i] == strstr(argv[i], "-m")) {
#if defined(HAVE_THREADS)
        g_multi_threading = true;
        if (strlen(argv[i]) > 2) {
          int numcpu = atoi(&argv[i][2]);
          g_ask_sysconf = false;
          g_num_threads = g_num_regions = numcpu;
          fprintf(stderr, "User requested %d threads.\n", numcpu);
        }
#else
        fprintf(stderr, "This demo wasn't built with thread support!\n");
        exit(-1);
#endif  // HAVE_THREADS
      } else if (0 == strcmp(argv[i], "-d")) {
        g_render_dots = true;
      } else if (argv[i] == strstr(argv[i], "-w")) {
        int w = atoi(&argv[i][2]);
        if ((w > 0) && (w < kMaxWindow)) {
          g_window_width = w;
        }
      } else if (argv[i] == strstr(argv[i], "-h")) {
        int h = atoi(&argv[i][2]);
        if ((h > 0) && (h < kMaxWindow)) {
          g_window_height = h;
        }
      } else if (argv[i] == strstr(argv[i], "-n")) {
        int n = atoi(&argv[i][2]);
        if ((n > 0) && (n < kMaxPoints)) {
          g_num_points = n;
        }
      } else if (argv[i] == strstr(argv[i], "-f")) {
        int f = atoi(&argv[i][2]);
        if ((f > 0) && (f < kMaxFrames)) {
          g_num_frames = f;
        }
      } else if (argv[i] == strstr(argv[i], "-t")) {
        test_mode = true;
      } else {
        printf("Voronoi SDL Demo\n");
        printf("usage: -m<n>   enable multi-threading across n threads.\n");
        printf("       -d      overlay dots.\n");
        printf("       -w<n>   width of window.\n");
        printf("       -h<n>   height of window.\n");
        printf("       -n<n>   number of cells.\n");
        printf("       -f<n>   number of frames.\n");
        printf("       -t      test mode, print checksum.\n");
        printf("       --help  show this screen.\n");
        exit(0);
      }
    }
  }

#if defined(HAVE_SYSCONF)
  // see if the system can tell us # cpus
  if ((g_ask_sysconf) && (g_multi_threading)) {
    int ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    if (ncpu > 1) {
      fprintf(stderr, "System reports having %d processors.\n", ncpu);
      g_num_threads = g_num_regions = ncpu;
    }
  }
#endif  // HAVE_SYSCONF

  // if user specified test mode, other options are overridden
  if (test_mode) {
    g_window_width = 64;
    g_window_height = 1024;
    g_num_points = 256;
    g_num_frames = 16;
    g_render_dots = false;
    g_use_av = false;
#if defined(HAVE_THREADS)
    g_multi_threading = true;
    g_num_threads = g_num_regions = 8;
#endif
  }

  // make sure window size is compatible
  if ((0 != (g_window_width % kStartRecurseSize)) ||
      (0 != (g_window_height % kStartRecurseSize))) {
        fprintf(stderr, "%s %d\n",
                "Window width & height must be evenly divisible by",
                kStartRecurseSize);
        exit(-1);
  }

  fprintf(stderr,
          "Multi-threading %s.\n",
          g_multi_threading ? "enabled" : "disabled");

  // clamp threads and regions
  g_num_threads = ClampThreads(g_num_threads);
  g_num_regions = ClampThreads(g_num_regions);
}


static void StartDemo() {
  // Initialise with an arbitrary seed in order to get consistent
  // results between newlib and glibc.
  srand48(0xC0DE533D);
  g_surface = Initialize();
  RunDemo(g_surface);
}

void StartDemoSrpc(NaClSrpcRpc *rpc,
                   NaClSrpcArg **in_args,
                   NaClSrpcArg **out_args,
                   NaClSrpcClosure *done) {
  StartDemo();
  done->Run(done);
}

void StopDemoSrpc(NaClSrpcRpc *rpc,
                   NaClSrpcArg **in_args,
                   NaClSrpcArg **out_args,
                   NaClSrpcClosure *done) {
  Shutdown();
  done->Run(done);
}

void ChecksumSrpc(NaClSrpcRpc *rpc,
                  NaClSrpcArg **in_args,
                  NaClSrpcArg **out_args,
                  NaClSrpcClosure *done) {
  out_args[0]->u.ival = Checksum();
  done->Run(done);
}

const struct NaClSrpcHandlerDesc srpc_methods[] = {
  NACL_AV_DECLARE_METHODS
  { "start_demo::", StartDemoSrpc },
  { "checksum::i", ChecksumSrpc },
  { "stop_demo::", StopDemoSrpc },
  { NULL, NULL },
};

// Parses cmd line options, initializes surface, runs the demo & shuts down.
int main(int argc, char **argv) {
  ParseCmdLineArgs(argc, argv);
#if !defined(STANDALONE)
  if (!NaClSrpcModuleInit()) {
    return 1;
  }
  if (!NaClSrpcAcceptClientOnThread(srpc_methods)) {
    return 1;
  }
  NaClSrpcModuleFini();
#else
  StartDemo();
#endif  // STANDALONE

  return 0;
}
