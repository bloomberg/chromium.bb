#include "imager.h"

static char *i_format_list[]={
#ifdef HAVE_LIBJPEG
  "jpeg",
#endif
#ifdef HAVE_LIBTIFF
  "tiff",
#endif
#ifdef HAVE_LIBPNG
  "png",
#endif
#ifdef HAVE_LIBGIF
  "gif",
#endif
#ifdef HAVE_LIBT1
  "t1",
#endif
#ifdef HAVE_LIBTT
  "tt",
#endif
#ifdef HAVE_WIN32
  "w32",
#endif
#ifdef HAVE_FT2
  "ft2",
#endif
  "raw",
  "pnm",
  "bmp",
  "tga",
  "ifs",
  NULL};

