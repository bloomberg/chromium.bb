#include "imager.h"

typedef struct {
  i_img_dim min,max;
}  minmax;

typedef struct {
  minmax *data;
  i_img_dim lines;
} i_mmarray;

/* FIXME: Merge this into datatypes.{c,h} */

void i_mmarray_cr(i_mmarray *ar,i_img_dim l);
void i_mmarray_dst(i_mmarray *ar);
void i_mmarray_add(i_mmarray *ar,i_img_dim x,i_img_dim y);
int i_mmarray_gmin(i_mmarray *ar,i_img_dim y);
int i_mmarray_getm(i_mmarray *ar,i_img_dim y);
void i_mmarray_info(i_mmarray *ar);
#if 0
void i_mmarray_render(i_img *im,i_mmarray *ar,i_color *val);
#endif
