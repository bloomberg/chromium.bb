/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// NaCl Earth demo
//   Ray trace planet Earth

// defines
// TODO: fix/cleanup when various parts of system come online
#define HAVE_THREADS            // enable if pthreads & semaphores are available
// #define HAVE_SYSCONF         // enable if sysconf() is available

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// STANDALONE = running without sel_ldr and nacl runtime
#if !defined(STANDALONE)
#include <nacl/nacl_av.h>
#include <nacl/nacl_srpc.h>
#endif
#if defined(STANDALONE)
#include "native_client/common/standalone.h"
#endif
#include "native_client/common/worker.h"

// print/debug messages
static void InfoPrintf(const char *fmt, ...) {
  va_list argptr;
  va_start (argptr, fmt);
  vfprintf (stderr, fmt, argptr);
  va_end (argptr);
  fflush(stderr);
}

#if 1
static void DebugPrintf(const char *fmt, ...) {
  va_list argptr;
  fprintf (stderr, "@@@ EARTH ");

  va_start (argptr, fmt);
  vfprintf (stderr, fmt, argptr);
  va_end (argptr);
  fflush(stderr);
}
#else
static void DebugPrintf(const char *fmt, ...) {}
#endif

// global properties
const float kPI = M_PI;
const float kOneOverPI = 1.0f / kPI;
const float kOneOver2PI = 1.0f / (2.0f * kPI);
const float kOneOver255 = 1.0f / 255.0f;
const int kArcCosineTableSize = 4096;
const float kMaxWindow = 4096;
const int kEarthTextureWidth = 1024;
const int kEarthTextureHeight = 512;
const int kMaxFrames = 1000000;
const int kRegionRatio = 8;
int g_window_width = 512;
int g_window_height = 512;
int g_num_frames = 300;
bool g_ask_sysconf = true;
int g_num_threads = 4;           // possibly overridden by sysconf()
int g_num_regions = 4;           // possibly overridden by sysconf()
bool g_multi_threading = false;  // can be overridden on cmd line

int g_frame_checksum = 0; // used for nacl module testing

// seed for rand_r() - we only call rand_r from main thread.
static unsigned int g_seed = 0xC0DE533D;

// random number helper
inline unsigned char rand255() {
  return static_cast<unsigned char>(rand_r(&g_seed) & 255);
}

// random number helper
inline float frand() {
  return (static_cast<float>(rand_r(&g_seed)) / static_cast<float>(RAND_MAX));
}

// build a packed color
inline uint32_t MakeRGBA(uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
  return (((a) << 24) | ((r) << 16) | ((g) << 8) | (b));
}

// extraction routines
inline float ExtractR(uint32_t c) {
  return static_cast<float>(c & 0xFF) * kOneOver255;
}

inline float ExtractG(uint32_t c) {
  return static_cast<float>((c & 0xFF00) >> 8) * kOneOver255;
}

inline float ExtractB(uint32_t c) {
  return static_cast<float>((c & 0xFF0000) >> 16) * kOneOver255;
}


// simple container for earth texture
struct Texture {
  int width, height;
  unsigned int pixels[kEarthTextureWidth * kEarthTextureHeight];
};


// compile our texture straight in to avoid runtime filesystem
Texture g_earth = {
  kEarthTextureWidth, kEarthTextureHeight, {
#include "native_client/tests/earth/earth_image.inc"
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


struct ArcCosine {
  // slightly larger table so we can interpolate beyond table size
  float table[kArcCosineTableSize + 2];
  float TableLerp(float x);
  ArcCosine();
  ~ArcCosine() { ; }
};


ArcCosine::ArcCosine() {
  // build a slightly larger table to allow for numeric imprecision
  for (int i = 0; i < (kArcCosineTableSize + 2); ++i) {
    float f = static_cast<float>(i) / kArcCosineTableSize;
    f = f * 2.0f - 1.0f;
    table[i] = acos(f);
  }
}


// looks up acos(f) using a table and lerping between entries
// (it is expected that input f is between -1 and 1)
float ArcCosine::TableLerp(float f) {
  float x = (f + 1.0f) * 0.5f;
  x = x * kArcCosineTableSize;
  int ix = static_cast<int>(x);
  float fx = static_cast<float>(ix);
  float dx = x - fx;
  float af = table[ix];
  float af2 = table[ix + 1];
  float a = af + (af2 - af) * dx;
  return a;
}


// takes a -0..1+ color, clamps it to 0..1 and maps it to 0..255 integer
inline unsigned int Clamp255(float x) {
  if (x < 0.0f) {
    x = 0.0f;
  } else if (x > 1.0f) {
    x = 1.0f;
  }
  return (unsigned int)(x * 255.0f);
}


// Planet class holds information and functionality needed to render
// a ray-traced planet into an raw pixel surface
class Planet {
  float planet_radius_;
  float planet_spin_;
  float planet_x_, planet_y_, planet_z_;
  float planet_pole_x_, planet_pole_y_, planet_pole_z_;
  float planet_equator_x_, planet_equator_y_, planet_equator_z_;
  float eye_x_, eye_y_, eye_z_;
  float light_x_, light_y_, light_z_;
  float diffuse_r_, diffuse_g_, diffuse_b_;
  float ambient_r_, ambient_g_, ambient_b_;

  // cached calculations
  float planet_xyz_;
  float planet_pole_x_equator_x_;
  float planet_pole_x_equator_y_;
  float planet_pole_x_equator_z_;
  float planet_radius2_;
  float planet_one_over_radius_;
  float eye_xyz_;

  // misc
  Texture *tex_;
  Surface *surf_;
  ArcCosine acos_;
  int num_regions_;
  int width_, height_;
  WorkerThreadManager *workers_;
  volatile bool exiting_;

public:

  // methods prefixed with 'w' are only called by worker threads!
  // (unless MULTI_THREADING is false)
  uint32_t* wGetAddr(int x, int y);
  void wPutPixel(int x, int y, unsigned int c);
  void wRenderPixelSpan(int x0, int x1, int y);
  void wMakeRect(int r, int *x, int *y, int *w, int *h);
  void wRenderRect(int x0, int y0, int x1, int y1);
  void wWorkerThreadEntry();

  // these methods are only called by the main thread
  void CacheCalcs();
  void SetPlanetXYZR(float x, float y, float z, float r);
  void SetPlanetPole(float x, float y, float z);
  void SetPlanetEquator(float x, float y, float z);
  void SetPlanetSpin(float a);
  void SetEyeXYZ(float x, float y, float z);
  void SetLightXYZ(float x, float y, float z);
  void SetAmbientRGB(float r, float g, float b);
  void SetDiffuseRGB(float r, float g, float b);
  bool CreateWorkerThreads(int num);
  void UpdateSim();
  void ParallelRender();
  void SequentialRender();
  void Render();
  void Draw();
  bool PollEvents();
  Planet(Surface *s, int numRegions, bool multithreading, Texture *tex);
  ~Planet();
};

// Given a region r, derive a rectangle.  Currently this function
// slices the main output buffer into equal sized rows.
// This function is used to convert a mutex guarded counter into
// a rectangular region for a given worker thread to process.
// This rectangle shouldn't overlap with work being done by other workers.
// If multithreading, this function is only called by the worker threads.
void Planet::wMakeRect(int r, int *x, int *y, int *w, int *h) {
  int dy = height_ / num_regions_;
  *x = 0;
  *w = width_;
  *y = r * dy;
  *h = dy;
}


inline uint32_t* Planet::wGetAddr(int x, int y) {
  return (surf_->pixels + y * surf_->pitch) + x;
}


inline void Planet::wPutPixel(int x, int y, unsigned int c) {
  uint32_t *pixel = (surf_->pixels + y * surf_->pitch) + x;
  *pixel = c;
}


union Convert {
  float f;
  int i;
  Convert(int x) { i = x; }
  Convert(float x) { f = x; }
  const int asInt() { return i; }
  const float asFloat() { return f; }
};


inline const int AsInteger(const float f) {
  Convert u(f);
  return u.asInt();
}


inline const float AsFloat(const int i) {
  Convert u(i);
  return u.asFloat();
}


const long int kOneAsInteger = AsInteger(1.0f);
const float kScaleUp = float(0x00800000);
const float kScaleDown = 1.0f / kScaleUp;


inline float inline_quick_sqrt(float x) {
  int i;
  i = (AsInteger(x) >> 1) + (kOneAsInteger >> 1);
  return AsFloat(i);
}


inline float inline_sqrt(float x) {
  float y;
  y = inline_quick_sqrt(x);
  y = (y * y + x) / (2.0f * y);
  y = (y * y + x) / (2.0f * y);
  return y;
}


// This is the meat of the ray tracer.  Given a pixel span (x0, x1) on
// scanline y, shoot rays into the scene and render what they hit.  Use
// scanline coherence to do a few optimizations
void Planet::wRenderPixelSpan(int x0, int x1, int y) {
  const int kColorBlack = MakeRGBA(0, 0, 0, 0xFF);
  float y0 = eye_y_;
  float z0 = eye_z_;
  float y1 = (static_cast<float>(y) / height_) * 2.0f - 1.0f;
  float z1 = 0.0f;
  float dy = (y1 - y0);
  float dz = (z1 - z0);
  float dy_dy_dz_dz = dy * dy + dz * dz;
  float two_dy_y0_y_two_dz_z0_z = 2.0f * dy * (y0 - planet_y_) +
                                  2.0f * dz * (z0 - planet_z_);
  float planet_xyz_eye_xyz = planet_xyz_ + eye_xyz_;
  float y_y0_z_z0 = planet_y_ * y0 + planet_z_ * z0;
  uint32_t *pixels = this->wGetAddr(x0, y);
  for (int x = x0; x <= x1; ++x) {
    // scan normalized screen -1..1
    float x1 = (static_cast<float>(x) / width_) * 2.0f - 1.0f;
    // eye
    float x0 = eye_x_;
    // delta from screen to eye
    float dx = (x1 - x0);
    // build a, b, c
    float a = dx * dx + dy_dy_dz_dz;
    float b = 2.0f * dx * (x0 - planet_x_) + two_dy_y0_y_two_dz_z0_z;
    float c = planet_xyz_eye_xyz +
              -2.0f * (planet_x_ * x0 + y_y0_z_z0) - (planet_radius2_);
    // calculate discriminant
    float disc = b * b - 4.0f * a * c;

    // did ray hit the sphere?
    if (disc < 0.0f) {
      *pixels = kColorBlack;
      ++pixels;
      continue;
    }

    // calc parametric t value
    float t = (-b - inline_sqrt(disc)) / (2.0f * a);
    float px = x0 + t * dx;
    float py = y0 + t * dy;
    float pz = z0 + t * dz;
    float nx = (px - planet_x_) * planet_one_over_radius_;
    float ny = (py - planet_y_) * planet_one_over_radius_;
    float nz = (pz - planet_z_) * planet_one_over_radius_;

    float Lx = (light_x_ - px);
    float Ly = (light_y_ - py);
    float Lz = (light_z_ - pz);
    float Lq = 1.0f / inline_quick_sqrt(Lx * Lx + Ly * Ly + Lz * Lz);
    Lx *= Lq;
    Ly *= Lq;
    Lz *= Lq;
    float d = (Lx * nx + Ly * ny + Lz * nz);
    float pr = (diffuse_r_ * d) + ambient_r_;
    float pg = (diffuse_g_ * d) + ambient_g_;
    float pb = (diffuse_b_ * d) + ambient_b_;
    float ds = -(nx * planet_pole_x_ +
                 ny * planet_pole_y_ +
                 nz * planet_pole_z_);
    float ang = acos_.TableLerp(ds);
    float v = ang * kOneOverPI;
    float dp = planet_equator_x_ * nx +
               planet_equator_y_ * ny +
               planet_equator_z_ * nz;
    float w = dp / sin(ang);
    if (w > 1.0f) w = 1.0f;
    if (w < -1.0f) w = -1.0f;
    float th = acos_.TableLerp(w) * kOneOver2PI;
    float dps = planet_pole_x_equator_x_ * nx +
                planet_pole_x_equator_y_ * ny +
                planet_pole_x_equator_z_ * nz;
    float u;
    if (dps < 0.0f)
      u = th;
    else
      u = 1.0f - th;

    int tx = static_cast<int>(u * tex_->width);
    int ty = static_cast<int>(v * tex_->height);
    int offset = tx + ty * tex_->width;
    uint32_t texel = tex_->pixels[offset];
    float tr = ExtractR(texel);
    float tg = ExtractG(texel);
    float tb = ExtractB(texel);

    unsigned int ir = Clamp255(pr * tr);
    unsigned int ig = Clamp255(pg * tg);
    unsigned int ib = Clamp255(pb * tb);
    unsigned int color = MakeRGBA(ir, ig, ib, 0xFF);

    *pixels = color;
    ++pixels;
  }
}


// Renders a rectangular area of the screen, scan line at a time
void Planet::wRenderRect(int x, int y, int w, int h) {
  for (int j = y; j < (y + h); ++j) {
    this->wRenderPixelSpan(x, x + w - 1, j);
  }
}


// Thread entry point Planet::wWorkerThread()
// This is the main loop for the worker thread(s).  It waits for work
// by testing a semaphore, which will sleep this thread until work arrives.
// It then grabs a mutex protected counter (which is also decremented)
// and uses this value to determine which subregion this thread should be
// processing.  When rendering is finished the worker then pings the main
// thread via PostDone() semaphore.
// If multithreading, this function is only called by the worker threads.
void Planet::wWorkerThreadEntry() {

  // we're a 'detached' thread...
  // (so we should automagically die when the main thread exits)
  while (!exiting_) {
    // wait for some work
    workers_->WaitWork();
    // if main thread is exiting, have worker exit too
    if (exiting_) break;
    // okay, grab region to work on from worker counter
    int region = workers_->DecCounter();
    if (region < 0) {
      // This indicates we're not sync'ing properly
      InfoPrintf("Region value went negative!\n");
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
  Planet *pPlanet = reinterpret_cast<Planet*>(args);
  pPlanet->wWorkerThreadEntry();
  return NULL;
}


// Create worker threads and pass along our this pointer.
// If workers_ is NULL, we're running in non-threaded mode.
bool Planet::CreateWorkerThreads(int num) {
  if (NULL != workers_) {
    return workers_->CreateThreadPool(num, ::wWorkerThreadEntry, this);
  } else {
    return true;
  }
}


// Run a simple sim to spin the planet.  Update loop is run once per frame.
// Called from the main thread only and only when the worker threads are idle.
void Planet::UpdateSim() {
  float m = planet_spin_ + 0.01f;
  // keep in nice range
  if (m > (kPI * 2.0f))
    m = m - kPI * 2.0f;
  SetPlanetSpin(m);
}


// This is the main thread's entry point to dispatch rendering of the planet.
// First, it sets the region counter to its max value.  This mutex guarded
// counter is how worker threads determine which region of the diagram they
// should be working on.  Then it pings the PostWork semaphore multiple times
// to wake up the sleeping worker threads.  Finally, this thread needs to
// sleep a little by waiting for the workers to finish.  It does that by
// waiting on the WaitDone semaphore.
void Planet::ParallelRender() {

  // At this point, all worker threads are idle and sleeping

  // setup barrier counter before we wake workers
  workers_->SetCounter(num_regions_);

  // wake up the workers
  for (int i = 0; i < num_regions_; ++i) {
    workers_->PostWork();
  }

  // At this point, all worker threads are awake and busy grabbing
  // work assignments by reading and decrementing the counter.
  // This main thread will sleep on the semaphore below while
  // waiting for the workers to finish.

  // wait for all work to be done
  for (int i = 0; i < num_regions_; ++i) {
    workers_->WaitDone();
  }

  // verify that our counter is where we expect it to be
  int c = workers_->DecCounter();
  if (-1 != c) {
    InfoPrintf("We're not syncing correctly! (%d)\n", c);
    exit(-1);
  }

  // At this point, all worker threads are idle and sleeping.
  // The main thread is free to muck with shared data, such
  // as updating the earth spin in the sim routine.
}


// Performs all rendering from the main thread.
void Planet::SequentialRender() {
  this->wRenderRect(0, 0, width_, height_);
}


// Renders the Planet diagram
// Picks either parallel or sequential rendering implementation.
void Planet::Render() {
  if (NULL == workers_) {
    this->SequentialRender();
  } else {
    this->ParallelRender();
  }
}


// Copies sw rendered earth image to screen
void Planet::Draw() {
  int r;
  r = nacl_video_update(surf_->pixels);
  if (-1 == r) {
    DebugPrintf("nacl_video_update() returned %d\n", errno);
  }
}


// pre-calculations to make inner loops faster
// these need to be recalculated when values change
void Planet::CacheCalcs() {
  planet_xyz_ = planet_x_ * planet_x_ +
                planet_y_ * planet_y_ +
                planet_z_ * planet_z_;
  planet_radius2_ = planet_radius_ * planet_radius_;
  planet_one_over_radius_ = 1.0f / planet_radius_;
  eye_xyz_ = eye_x_ * eye_x_ + eye_y_ * eye_y_ + eye_z_ * eye_z_;
  // spin vector from center->equator
  planet_equator_x_ = cos(planet_spin_);
  planet_equator_y_ = 0.0f;
  planet_equator_z_ = sin(planet_spin_);
  // cache cross product of pole & equator
  planet_pole_x_equator_x_ = planet_pole_y_ * planet_equator_z_ -
                             planet_pole_z_ * planet_equator_y_;
  planet_pole_x_equator_y_ = planet_pole_z_ * planet_equator_x_ -
                             planet_pole_x_ * planet_equator_z_;
  planet_pole_x_equator_z_ = planet_pole_x_ * planet_equator_y_ -
                             planet_pole_y_ * planet_equator_x_;
}


void Planet::SetPlanetXYZR(float x, float y, float z, float r) {
  planet_x_ = x;
  planet_y_ = y;
  planet_z_ = z;
  planet_radius_ = r;
  CacheCalcs();
}


void Planet::SetEyeXYZ(float x, float y, float z) {
  eye_x_ = x;
  eye_y_ = y;
  eye_z_ = z;
  CacheCalcs();
}


void Planet::SetLightXYZ(float x, float y, float z) {
  light_x_ = x;
  light_y_ = y;
  light_z_ = z;
  CacheCalcs();
}


void Planet::SetAmbientRGB(float r, float g, float b) {
  ambient_r_ = r;
  ambient_g_ = g;
  ambient_b_ = b;
  CacheCalcs();
}


void Planet::SetDiffuseRGB(float r, float g, float b) {
  diffuse_r_ = r;
  diffuse_g_ = g;
  diffuse_b_ = b;
  CacheCalcs();
}


void Planet::SetPlanetPole(float x, float y, float z) {
  planet_pole_x_ = x;
  planet_pole_y_ = y;
  planet_pole_z_ = z;
  CacheCalcs();
}


void Planet::SetPlanetEquator(float x, float y, float z) {
  // this is really over-ridden by spin at the momenent
  planet_equator_x_ = x;
  planet_equator_y_ = y;
  planet_equator_z_ = z;
  CacheCalcs();
}


void Planet::SetPlanetSpin(float a) {
  planet_spin_ = a;
  CacheCalcs();
}


// Polls event queue
// returns false if the user tries to quit application early
bool Planet::PollEvents() {
  NaClMultimediaEvent event;
  // bare minimum event servicing
  while (0 == nacl_video_poll_event(&event)) {
    if (NACL_EVENT_QUIT == event.type)
      return false;
  }
  return true;
}


// Setups and initializes planet data structures.
// Seed planet, eye, and light
Planet::Planet(Surface *surf, int numRegions, bool multi,
               Texture *tex) {
  surf_ = surf;
  num_regions_ = numRegions;
  width_ = surf->width;
  height_ = surf->height;
  workers_ = multi ? new WorkerThreadManager() : NULL;
  tex_ = tex;
  exiting_ = false;

  this->SetPlanetXYZR(0.0f, 0.0f, 48.0f, 4.0f);
  this->SetEyeXYZ(0.0f, 0.0f, -14.0f);
  this->SetLightXYZ(-8.0f, -4.0f, 2.0f);
  this->SetAmbientRGB(0.4f, 0.4f, 0.4f);
  this->SetDiffuseRGB(0.8f, 0.8f, 0.8f);
  this->SetPlanetPole(0.0f, 1.0f, 0.0f);
  this->SetPlanetEquator(1.0f, 0.0f, 0.0f);
  this->SetPlanetSpin(kPI / 2.0f);
}


// Frees up planet resources.
Planet::~Planet() {
  if (workers_) {
    exiting_ = true;
    // wake up the worker threads from their slumber
    workers_->PostWorkAll();
    workers_->JoinAll();
    delete workers_;
  }
}


// Runs the earth demo and animates the image for g_num_frames
void RunDemo(Surface *surface) {

  static const char *credit[] = {
    "\n",
    "Image Credit:\n",
    "\n",
    "NASA Goddard Space Flight Center Image by Reto St�ckli (land surface,\n",
    "shallow water, clouds). Enhancements by Robert Simmon (ocean color,\n",
    "compositing, 3D globes, animation).\n",
    "Data and technical support: MODIS Land Group; MODIS Science Data,\n",
    "Support Team; MODIS Atmosphere Group; MODIS Ocean Group Additional data:\n",
    "USGS EROS Data Center (topography); USGS Terrestrial Remote Sensing\n",
    "Flagstaff Field Center (Antarctica); Defense Meteorological\n",
    "Satellite Program (city lights).\n",
    "\n",
    NULL
  };

  Planet planet(surface, g_num_regions, g_multi_threading, &g_earth);

  for (int i = 0; NULL != credit[i]; ++i)
   InfoPrintf("%s", credit[i]);

  if (planet.CreateWorkerThreads(g_num_threads)) {
    for (int i = 0; i < g_num_frames; ++i) {
      planet.UpdateSim();
      planet.Render();
      planet.Draw();
      // TODO: do a proper checksum here which is numerically stable
      g_frame_checksum = i;
      DebugPrintf("Frame: %04d\r", i);
      if (!planet.PollEvents())
        break;
    }
  }
  DebugPrintf("\nDemo complete\n");
}


// Initializes a window buffer.
Surface* Initialize() {
  int r;
  int width;
  int height;
  r = nacl_multimedia_init(NACL_SUBSYSTEM_VIDEO | NACL_SUBSYSTEM_EMBED);
  if (-1 == r) {
    InfoPrintf("Multimedia system failed to initialize!  errno: %d\n", errno);
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
    InfoPrintf("Video subsystem failed to initialize!  errno; %d\n", errno);
    exit(-1);
  }
  Surface *surface = new Surface(g_window_width, g_window_height);
  return surface;
}


// Frees a window buffer.
void Shutdown(Surface *surface) {
  delete surface;
}


// Clamps input to the max we can realistically support.
int ClampThreads(int num) {
  const int max = 128;
  if (num > max) {
    return max;
  }
  return num;
}


// If user specifies options on cmd line, parse them
// here and update global settings as needed.
void ParseCmdLineArgs(int argc, char **argv) {
  // look for cmd line args
  if (argc > 1) {
    for (int i = 1; i < argc; ++i) {
      if (argv[i] == strstr(argv[i], "-m")) {
#if defined(HAVE_THREADS)
        g_multi_threading = true;
        if (strlen(argv[i]) > 2) {
          int numcpu = atoi(&argv[i][2]);
          g_ask_sysconf = false;
          g_num_threads = numcpu;
          g_num_regions = numcpu * kRegionRatio;
          InfoPrintf("User requested %d threads.\n", numcpu);
        }
#else
        InfoPrintf("This demo wasn't built with thread support!\n");
        exit(-1);
#endif // HAVE_THREADS
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
      } else if (argv[i] == strstr(argv[i], "-f")) {
        int f = atoi(&argv[i][2]);
        if ((f > 0) && (f < kMaxFrames)) {
          g_num_frames = f;
        }
      } else {
        InfoPrintf("Earth SDL Demo\n"
                   "usage: -m<n>   enable multi-threading across n threads.\n"
                   "       -w<n>   width of window.\n"
                   "       -h<n>   height of window.\n"
                   "       -f<n>   number of frames.\n"
                   "       --help  show this screen.\n");
        exit(0);
      }
    }
  }

  InfoPrintf("Multi-threading %s.\n", g_multi_threading ? "enabled" : "disabled");

#if defined(HAVE_SYSCONF)
  // see if the system can tell us # cpus
  if ((g_ask_sysconf) && (g_multi_threading)) {
    int ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    if (ncpu > 1) {
      InfoPrintf("System reports having %d processors.\n", ncpu);
      g_num_threads = ncpu;
      g_num_regions = ncpu * kRegionRatio;
    }
  }
#endif // HAVE_SYSCONF

  // clamp threads and regions
  g_num_threads = ClampThreads(g_num_threads);
  g_num_regions = ClampThreads(g_num_regions);
}

// We do not start the demo right away when run as a browswer module
sem_t GlobalDemoSemaphore;

#if !defined(STANDALONE)
NaClSrpcError NaclModuleStartDemo(NaClSrpcChannel *channel,
                                  NaClSrpcArg** in_args,
                                  NaClSrpcArg** out_args) {
  DebugPrintf("Start called with %d\n", in_args[0]->u.ival);
  g_num_frames = in_args[0]->u.ival;
  sem_post(&GlobalDemoSemaphore);
  return NACL_SRPC_RESULT_OK;
}

NACL_SRPC_METHOD("start_demo:i:", NaclModuleStartDemo);

NaClSrpcError NaclModuleFrameChecksum(NaClSrpcChannel *channel,
                                      NaClSrpcArg** in_args,
                                      NaClSrpcArg** out_args) {
  DebugPrintf("checksum called: %d\n", g_frame_checksum);
  out_args[0]->u.ival = g_frame_checksum;
  return NACL_SRPC_RESULT_OK;
}

NACL_SRPC_METHOD("frame_checksum::i", NaclModuleFrameChecksum);
#endif

// Parses cmd line options, initializes surface, runs the demo & shuts down.
int main(int argc, char *argv[]) {
#if defined(STANDALONE)
  ParseCmdLineArgs(argc, argv);
  Surface *surface = Initialize();
  RunDemo(surface);
  Shutdown(surface);
#else
  if (!NaClSrpcModuleInit()) {
    return 1;
  }
  if (!NaClSrpcAcceptClientOnThread(__kNaClSrpcHandlers)) {
    return 1;
  }
  // NOTE: We current cannot distinguish whether we run in the browser or not
  bool run_in_browser = false;
  ParseCmdLineArgs(argc, argv);
  sem_init(&GlobalDemoSemaphore, 0, 0);
  sem_post(&GlobalDemoSemaphore);
  Surface *surface = Initialize();

  do {
    DebugPrintf("Waiting on semaphore\n");
    sem_wait(&GlobalDemoSemaphore);
    RunDemo(surface);
  } while (run_in_browser);  // for now run only once
  NaClSrpcModuleFini();
#endif

  return 0;
}
