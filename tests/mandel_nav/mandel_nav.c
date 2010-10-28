/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
  simple demo for nacl client, computed mandelbrot set
*/


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>
#include <nacl/nacl_srpc.h>
#include <nacl/nacl_av.h>

#define FPEPSILON  1.0e-10
#define FPEQUAL(x, y) (abs(x - y) < FPEPSILON)

#define IND(arr, width, i, j) *(arr + (i) * (width) + (j))

#define BASEWIDTH  500
#define BASEHEIGHT 500

pthread_mutex_t work_mu = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t work_cv = PTHREAD_COND_INITIALIZER;

enum work_id { WORK_SETUP,
               WORK_SHUTDOWN,
               WORK_SET_REGION,
               WORK_DISPLAY,
               WORK_SHIFT_COLOR };

struct work_item {
  enum work_id      kind;
  pthread_mutex_t   *mu;
  pthread_cond_t    *cv;
  int               *done;
  union {
    double          setup_canvas_width;
    struct {
      double new_x_left, new_y_top, new_x_right, new_y_bottom;
    }               set_region;
    int             shift_color;
  } u;
};

struct work_item  *work = NULL;

__thread struct work_item tls_item;
__thread pthread_mutex_t  tls_mu = PTHREAD_MUTEX_INITIALIZER;
__thread pthread_cond_t   tls_cv = PTHREAD_COND_INITIALIZER;
__thread int              tls_done;

void work_put(struct work_item *item) {
  item->mu = &tls_mu;
  item->cv = &tls_cv;
  item->done = &tls_done;
  tls_done = 0;

  pthread_mutex_lock(&work_mu);
  while (NULL != work) {
    pthread_cond_wait(&work_cv, &work_mu);
  }
  work = item;
  pthread_cond_broadcast(&work_cv);
  pthread_mutex_unlock(&work_mu);

  pthread_mutex_lock(item->mu);
  while (0 == *item->done) {
    pthread_cond_wait(item->cv, item->mu);
  }
  pthread_mutex_unlock(item->mu);
}

struct work_item *work_get(void) {
  struct work_item *cur;

  pthread_mutex_lock(&work_mu);
  while (NULL == (cur = work)) {
    pthread_cond_wait(&work_cv, &work_mu);
  }
  pthread_mutex_unlock(&work_mu);

  return cur;
}

void work_mark_done(struct work_item *item) {
  pthread_mutex_lock(item->mu);
  *item->done = 1;
  pthread_cond_broadcast(item->cv);
  pthread_mutex_unlock(item->mu);

  pthread_mutex_lock(&work_mu);
  work = NULL;
  pthread_cond_broadcast(&work_cv);
  pthread_mutex_unlock(&work_mu);
}

/*
 * Local types.
 */
typedef struct Rgb_ {
  unsigned char r;
  unsigned char g;
  unsigned char b;
} Rgb;

typedef struct Pixel_ {
  unsigned char b;
  unsigned char g;
  unsigned char r;
  unsigned char a;
} Pixel;

/*
 * Module static variables.
 */

/*
 * Display memory related variables.
 */
static char   display_mem[BASEWIDTH * BASEHEIGHT * sizeof(Pixel)];
static char   pixmap[BASEWIDTH * BASEHEIGHT];
static char   mask[BASEWIDTH * BASEHEIGHT];

/*
 * Mandelbrot set range values.
 * The first four variables are set to "funny" values to force a complete
 * computation of the client-requested region on the first invocation.
 */
static double x_left   = -MAXFLOAT;
static double y_top    = MAXFLOAT;
static double x_right  = MAXFLOAT;
static double y_bottom = -MAXFLOAT;
static double xstep;
static double ystep;
static double new_x_left;
static double new_y_top;
static double new_x_right;
static double new_y_bottom;


/*
 * The pixel map is a canvas_width * canvas_width array of palette indices.
 */
static double canvas_width;
static unsigned int palette_select = 0;

/*
 * Palette indices select one of 256 different colors from a palette that
 * may be changing (psychedelic mode).
 */
#define kPsychInterpSteps 4
static Pixel palette[256][256];

/*
 * Interpolate ARGB values from the starting colors.
 */
static void InterpolateColors() {
  static const Rgb base_colors[] = {
    /*
     * Starting color values are inspired by those used by the Xaos
     * visualizer (sadly not the same as their palette).  We picked
     * some nearby colors using a color wheel site.
     */
    {  0,   0,   0},  /* black */
    { 30, 144, 255},  /* blue */
    { 32,  13,  33},  /* dim violet */
    {222,  85,  34},  /* red-orange */
    { 17,  10,   6},  /* dim violet */
    {135,  46,  71},  /* dim orange */
    { 53,  60,  28},  /* dim greenish-yellow */
    {216, 204,  94},  /* yellow */
    { 34,  58,  46},  /* dim light green */
    {220, 146, 139},  /* reddish / salmon */
    { 24,  11,  53},  /* dark blue */
    {133, 115, 209},  /* blue-purple */
    { 64,  50,  37},  /* dim orange */
    { 26, 161,  33},  /* green */
    { 28,  21,  61},  /* dim blue */
    { 29, 114,  53},  /* green */
    { 39,   5,  42},  /* dim violet */
    {163, 198, 247},  /* sky blue */
    { 10,  51,  14},  /* dim green */
    {211, 132,  10},  /* orange */
    { 17,  25,  42},  /* dim blue */
    {166, 138,  68},  /* yellow-orange */
    { 25,  46,  10},  /* dim mint green */
    {190, 252, 177},  /* bright mint green */
    { 12,   6,  62},  /* dim blue */
    { 46,  67,  32},  /* dim yellow-gren */
    { 23,  51,  53},  /* dim sky blue */
    {146,  78, 186},  /* violet */
    { 11,   6,  15},  /* dim violet */
    {227,  87, 216},  /* pink-purple */
    { 47,  49,  28},  /* dim yellow */
    { 19,  28,  55}   /* dim blue */
  };

  const int colors = sizeof(base_colors) / sizeof(Rgb);
  const int palette_entries = 256;
  const int entries_per_color = palette_entries / colors;
  const double scale = (double) colors / (double) palette_entries;
  int i;
  int j;

  /*
   * First, build the colors for normal display.
   */
  for (i = 0; i< colors; ++i) {
    int r = base_colors[i].r;
    int g = base_colors[i].g;
    int b = base_colors[i].b;
    double rs = (double) (base_colors[(i + 1) % colors].r - r) * scale;
    double gs = (double) (base_colors[(i + 1) % colors].g - g) * scale;
    double bs = (double) (base_colors[(i + 1) % colors].b - b) * scale;
    for (j = 0; j < entries_per_color; ++j) {
      int index = i * entries_per_color + j;
      palette[0][index].r = base_colors[i].r + (double) j * rs;
      palette[0][index].g = base_colors[i].g + (double) j * gs;
      palette[0][index].b = base_colors[i].b + (double) j * bs;
      palette[0][index].a = 0;
    }
  }
  /*
   * Then build the rotated palettes.
   */
  for (i = 1; i < palette_entries; ++i) {
    for (j = 0; j < palette_entries; ++j) {
      palette[i][j] = palette[0][(j + i) % palette_entries];
    }
  }
}

/*
 * The routine to set up the display arrays to be used by all the calls.
 */

NaClSrpcError SetupGlobals(NaClSrpcChannel *channel,
                           NaClSrpcArg **in_args,
                           NaClSrpcArg **out_args) {
  struct work_item p;
  p.kind = WORK_SETUP;
  p.u.setup_canvas_width = in_args[0]->u.dval;
  work_put(&p);
  return NACL_SRPC_RESULT_OK;
}

void do_SetupGlobals(struct work_item *p) {
  canvas_width = p->u.setup_canvas_width;
  /* Initialize the mask so that all pixels recompute every time */
  memset(mask, 0, sizeof(mask));
  InterpolateColors();
}

NACL_SRPC_METHOD("setup:d:", SetupGlobals);

NaClSrpcError ShutdownSharedMemory(NaClSrpcChannel *channel,
                                   NaClSrpcArg **in_args,
                                   NaClSrpcArg **out_args) {
  struct work_item p;
  p.kind = WORK_SHUTDOWN;
  work_put(&p);
  return NACL_SRPC_RESULT_OK;
}

void do_ShutdownSharedMemory(struct work_item *p) {
  /* Free the pixmap. */
  /* free(pixmap); TODO(sehr): is this vestigial code? */
  /* Return success. */
}

NACL_SRPC_METHOD("shutdown::", ShutdownSharedMemory);

/*
 * Compute ARGB value for one pixel.
 */
static inline unsigned int mandel(double cx, double cy) {
  double re = cx;
  double im = cy;
  unsigned int count = 0;
  const double threshold = 1.0e8;

  while (count < 255 && re * re + im * im < threshold) {
    double new_re = re * re - im * im + cx;
    double new_im = 2 * re * im + cy;
    re = new_re;
    im = new_im;
    ++count;
  }

  return count;
}

/*
 * Compute an entire pixmap of palette values.
 */
static void Compute(int xlo, int xhi, double xstart,
                    int ylo, int yhi, double ystart,
                    int width, char* map) {
  double x;
  double y;
  int i;
  int j;

  /* Compute the mandelbrot set, leaving color values in pixmap. */
  y = ystart;
  for (i = ylo; i < yhi; ++i) {
    x = xstart;
    for (j = xlo; j < xhi; ++j) {
      IND(map, width, i, j) = mandel(x, y);
      x += xstep;
    }
    y += ystep;
  }
}

#if 0
static void ComputeMasked(int xlo, int xhi, double xstart,
                          int ylo, int yhi, double ystart,
                          int width, char* map) {
  double x;
  double y;
  int i;
  int j;

  /* Compute the mandelbrot set, leaving color values in pixmap. */
  y = ystart;
  for (i = ylo; i < yhi; ++i) {
    x = xstart;
    for (j = xlo; j < xhi; ++j) {
      if (!IND(mask, width, i, j)) {
        IND(map, width, i, j) = mandel(x, y);
      }
      x += xstep;
    }
    y += ystep;
  }
}
#endif

static void BuildMask(int width, char* map, char* mask) {
  int i;
  int j;
  unsigned int total;

  if (x_left == -MAXFLOAT &&
      y_top == MAXFLOAT &&
      x_right == MAXFLOAT &&
      y_bottom == -MAXFLOAT) {
    return;
  }
  /* Compute a nine-point stencil around the current point */
  for (i = 1; i < width - 1; ++i) {
    for (j = 1; j < width - 1; ++j) {
      total = (IND(map, width, i - 1, j - 1) + IND(map, width, i - 1, j) +
               IND(map, width, i - 1, j + 1) +
               IND(map, width, i, j - 1) + IND(map, width, i, j + 1) +
               IND(map, width, i + 1, j - 1) + IND(map, width, i + 1, j) +
               IND(map, width, i + 1, j + 1)) >> 3;
      if (total == IND(map, width, i, j)) {
        IND(mask, width, i, j) = 1;
      } else {
        IND(mask, width, i, j) = 0;
      }
    }
  }
}

/*
 * Move a selected region up or down, reusing already computed values.
 */
static void MoveUD(int offset) {
  int width = (int) canvas_width;

  if (offset > 0) {
    char* p = pixmap;
    char* q = pixmap + offset * width;
    size_t size = (width - offset) * width;

    /* Move the invariant picture down by offset pixels. */
    memmove(q, p, size);
    /* Recompute the newly revealed top portion */
    Compute(0, width, x_left,
            0, offset, new_y_top,
            (int) canvas_width, pixmap);
  } else {
    char* p = pixmap - offset * width;
    char* q = pixmap;
    size_t size = (width + offset) * width;

    /* Move the invariant picture up by offset pixels. */
    memmove(q, p, size);
    /* Recompute the newly revealed bottom portion */
    Compute(0, width, x_left,
            width + offset, width, y_bottom,
            (int) canvas_width, pixmap);
  }
}

/*
 * Move a selected region left or right, reusing already computed values.
 */
static void MoveLR(int offset) {
  int i;
  int j;
  int width = canvas_width;

  if (offset > 0) {
    /* Move the invariant picture left by offset pixels. */
    for (i = 0; i < width; ++i) {
      for (j = 0; j < width - offset; ++j) {
        pixmap[i * width + j] = pixmap[i * width + j + offset];
      }
    }
    /* Recompute the newly revealed right portion */
    Compute(width - offset, width, x_right,
            0, width, y_top,
            (int) canvas_width, pixmap);
  } else {
    /* Move the invariant picture right by offset pixels. */
    for (i = 0; i < width; ++i) {
      for (j = width - 1; j >= -offset; --j) {
        pixmap[i * width + j] = pixmap[i * width + j + offset];
      }
    }
    /* Recompute the newly revealed left portion */
    Compute(0, -offset, new_x_left,
            0, width, y_top,
            (int) canvas_width, pixmap);
  }
}

/*
 * Select the region of the X-Y plane to be viewed and compute.
 */
NaClSrpcError SetRegion(NaClSrpcChannel *channel,
                        NaClSrpcArg **in_args,
                        NaClSrpcArg **out_args) {
  struct work_item p;
  p.kind = WORK_SET_REGION;
  p.u.set_region.new_x_left   = in_args[0]->u.dval;
  p.u.set_region.new_y_top    = in_args[1]->u.dval;
  p.u.set_region.new_x_right  = in_args[2]->u.dval;
  p.u.set_region.new_y_bottom = in_args[3]->u.dval;
  work_put(&p);
  return NACL_SRPC_RESULT_OK;
}

void do_SetRegion(struct work_item *p) {
  double new_xstep;
  double new_ystep;
  new_x_left   = p->u.set_region.new_x_left;
  new_y_top    = p->u.set_region.new_y_top;
  new_x_right  = p->u.set_region.new_x_right;
  new_y_bottom = p->u.set_region.new_y_bottom;

  new_xstep = (new_x_right - new_x_left) / canvas_width;
  new_ystep = (new_y_bottom - new_y_top) / canvas_width;

  if (new_x_left == x_left && new_x_right == x_right) {
    if (new_y_top == y_top && new_y_bottom == y_bottom) {
      /* No change */
    } else if (new_ystep == ystep) {
      /* Vertical pan */
      /* Note that ystep is negative */
      MoveUD(-(new_y_top - y_top) / ystep);
      y_top = new_y_top;
      y_bottom = new_y_bottom;
    } else {
      /* Vertical zoom (and possibly pan) */
      y_top = new_y_top;
      y_bottom = new_y_bottom;
      ystep = new_ystep;
      Compute(0, canvas_width, x_left,
              0, canvas_width, y_top,
              (int) canvas_width, pixmap);
    }
  } else if (new_y_top == y_top && new_y_bottom == y_bottom) {
    if (new_x_left == x_left && new_x_right == x_right) {
      /* No change */
    } else if (new_xstep == xstep) {
      /* Horizontal pan */
      MoveLR((new_x_right - x_right) / xstep);
      x_left = new_x_left;
      x_right = new_x_right;
    } else {
      /* Horizontal zoom (and possibly pan) */
      x_left = new_x_left;
      x_right = new_x_right;
      xstep = new_xstep;
      Compute(0, canvas_width, x_left,
              0, canvas_width, y_top,
              (int) canvas_width, pixmap);
    }
  } else if (FPEQUAL((x_right - x_left) / (y_top - y_bottom),
                     (new_x_right - new_x_left) / (new_y_top - new_y_bottom)) &&
             ((x_right - x_left) / (y_top - y_bottom) >=
              (new_x_right - new_x_left) / (new_y_top - new_y_bottom))) {
    /* Uniform zoom. */
    BuildMask(canvas_width, pixmap, mask);
    x_left = new_x_left;
    y_top = new_y_top;
    x_right = new_x_right;
    y_bottom = new_y_bottom;
    xstep = new_xstep;
    ystep = new_ystep;
    Compute(0, canvas_width, x_left,
            0, canvas_width, y_top,
            (int) canvas_width, pixmap);
  } else {
    /* General move. */
    x_left = new_x_left;
    y_top = new_y_top;
    x_right = new_x_right;
    y_bottom = new_y_bottom;
    xstep = new_xstep;
    ystep = new_ystep;
    Compute(0, canvas_width, x_left,
            0, canvas_width, y_top,
            (int) canvas_width, pixmap);
  }
}

NACL_SRPC_METHOD("set_region:dddd:", SetRegion);

/*
 * Display the pixmap, using the color palette with a possible shift.
 */
NaClSrpcError MandelDisplay(NaClSrpcChannel *channel,
                            NaClSrpcArg** in_args,
                            NaClSrpcArg** out_args) {
  struct work_item p;
  p.kind = WORK_DISPLAY;
  work_put(&p);
  return NACL_SRPC_RESULT_OK;
}

void do_MandelDisplay(struct work_item *wp) {
  Pixel* p = (Pixel*) display_mem;
  char* q = pixmap;
  int i;
  int j;

  /* Render the color values according to the palette. */
  for (i = 0; i < canvas_width; ++i) {
    for (j = 0; j < canvas_width; ++j) {
      /* TODO(robertm): verify signedness issues */
      *p = palette[palette_select][(int) *q];
      ++p;
      ++q;
    }
  }
  nacl_video_update(display_mem);
}

NACL_SRPC_METHOD("display::", MandelDisplay);

/*
 * To implement "psychedelic mode" we select one of 256 palettes
 * (actually rotations of the same palette) by using palette_select.
 * ShiftColors can set it to the default value (0) by passing zero
 * or simply bump it by one when anything else is passed.
 */
NaClSrpcError ShiftColors(NaClSrpcChannel *channel,
                          NaClSrpcArg** in_args,
                          NaClSrpcArg** out_args) {
  struct work_item p;
  p.kind = WORK_SHIFT_COLOR; p.u.shift_color = in_args[0]->u.ival;
  work_put(&p);
  return NACL_SRPC_RESULT_OK;
}

void do_ShiftColors(struct work_item *p) {
  int shift_color = p->u.shift_color;

  if (0 == shift_color) {
    /* Reset to the default color palette */
    palette_select = 0;
  } else {
    palette_select = (palette_select + 1) % 256;
  }
}

NACL_SRPC_METHOD("shift_colors:i:", ShiftColors);



/*
 * The main program.
 */

int main(int argc, char* argv[]) {
  int retval;
  struct work_item *item;
  if (!NaClSrpcModuleInit()) {
    return 1;
  }
  if (!NaClSrpcAcceptClientOnThread(__kNaClSrpcHandlers)) {
    return 1;
  }
  /* Initialize the graphics subsystem.  */
  retval = nacl_multimedia_init(NACL_SUBSYSTEM_VIDEO | NACL_SUBSYSTEM_EMBED);
  if (0 != retval) {
    return 1;
  }
  retval = nacl_video_init(BASEWIDTH, BASEHEIGHT);
  if (0 != retval) {
    return 1;
  }
  /* Process requests from the UI.  */
  while (NULL != (item = work_get())) {
    switch (item->kind) {
#define D(k,m) case k: m(item); break
      D(WORK_SETUP, do_SetupGlobals);
      D(WORK_SHUTDOWN, do_ShutdownSharedMemory);
      D(WORK_SET_REGION, do_SetRegion);
      D(WORK_DISPLAY, do_MandelDisplay);
      D(WORK_SHIFT_COLOR, do_ShiftColors);
    }
    work_mark_done(item);
  }
  /* Shutdown and return.  */
  nacl_video_shutdown();
  nacl_multimedia_shutdown();
  NaClSrpcModuleFini();
  return 0;
}
