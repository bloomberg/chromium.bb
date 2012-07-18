#ifndef IMAGER_RENDERT_H
#define IMAGER_RENDERT_H

#include "imdatatypes.h"

struct i_render_tag {
  int magic;
  i_img *im;

  i_img_dim line_width;
  i_color *line_8;
  i_fcolor *line_double;

  i_img_dim fill_width;
  i_color *fill_line_8;
  i_fcolor *fill_line_double;
};

#endif
