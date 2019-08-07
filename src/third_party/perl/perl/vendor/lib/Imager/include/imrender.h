#ifndef IMAGER_IMRENDER_H
#define IMAGER_IMRENDER_H

#include "rendert.h"

extern void
i_render_init(i_render *r, i_img *im, i_img_dim width);
extern void
i_render_done(i_render *r);
extern void
i_render_color(i_render *r, i_img_dim x, i_img_dim y, i_img_dim width,
	       unsigned char const *src, i_color const *color);
extern void
i_render_fill(i_render *r, i_img_dim x, i_img_dim y, i_img_dim width,
	      unsigned char const *src, i_fill_t *fill);
extern void
i_render_line(i_render *r, i_img_dim x, i_img_dim y, i_img_dim width,
	      const i_sample_t *src, i_color *line, i_fill_combine_f combine);
extern void
i_render_linef(i_render *r, i_img_dim x, i_img_dim y, i_img_dim width,
	       const double *src, i_fcolor *line, i_fill_combinef_f combine);

extern i_render *
i_render_new(i_img *im, i_img_dim width);
extern void
i_render_delete(i_render *r);

#endif
