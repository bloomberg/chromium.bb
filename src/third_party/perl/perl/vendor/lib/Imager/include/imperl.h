/*
  This header file defines types that Imager's typemap uses to convert to
  perl types.

  This is meant for use in XS code, not in normal C source.
 */
#ifndef IMAGER_IMPERL_H
#define IMAGER_IMPERL_H

#include "imdatatypes.h"

typedef i_color* Imager__Color;
typedef i_fcolor* Imager__Color__Float;
typedef i_img*   Imager__ImgRaw;
typedef int undef_neg_int;
typedef i_img * Imager;

#ifdef HAVE_LIBTT
typedef TT_Fonthandle* Imager__Font__TT;
#endif

/* for the fill objects
   Since a fill object may later have dependent images, (or fills!)
   we need perl wrappers - oh well
*/
#define IFILL_DESTROY(fill) i_fill_destroy(fill);
typedef i_fill_t* Imager__FillHandle;

typedef io_glue *Imager__IO;

#endif
