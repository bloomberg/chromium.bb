#include "imdatatypes.h"

#ifndef IMAGER_EXT_H
#define IMAGER_EXT_H

/* structures for passing data between Imager-plugin and the Imager-module */

typedef struct {
  char *name;
  void (*iptr)(void* ptr);
  char *pcode;
} func_ptr;


typedef struct {
  int (*getstr)(void *hv_t,char* key,char **store);
  int (*getint)(void *hv_t,char *key,int *store);
  int (*getdouble)(void *hv_t,char* key,double *store);
  int (*getvoid)(void *hv_t,char* key,void **store);
  int (*getobj)(void *hv_t,char* key,char* type,void **store);
} UTIL_table_t;

typedef struct {
  undef_int (*i_has_format)(char *frmt);
  i_color*(*ICL_set)(i_color *cl,unsigned char r,unsigned char g,unsigned char b,unsigned char a);
  void (*ICL_info)(const i_color *cl);

  im_context_t (*im_get_context_f)(void);
  i_img*(*im_img_empty_ch_f)(im_context_t, i_img *im,i_img_dim x,i_img_dim y,int ch);
  void(*i_img_exorcise_f)(i_img *im);

  void(*i_img_info_f)(i_img *im,i_img_dim *info);
  
  void(*i_img_setmask_f)(i_img *im,int ch_mask);
  int (*i_img_getmask_f)(i_img *im);
  
  /*
  int (*i_ppix)(i_img *im,i_img_dim x,i_img_dim y,i_color *val);
  int (*i_gpix)(i_img *im,i_img_dim x,i_img_dim y,i_color *val);
  */
  void(*i_box)(i_img *im,i_img_dim x1,i_img_dim y1,i_img_dim x2,i_img_dim y2,const i_color *val);
  void(*i_line)(i_img *im,i_img_dim x1,i_img_dim y1,i_img_dim x2,i_img_dim y2,const i_color *val,int endp);
  void(*i_arc)(i_img *im,i_img_dim x,i_img_dim y,double rad,double d1,double d2,const i_color *val);
  void(*i_copyto)(i_img *im,i_img *src,i_img_dim x1,i_img_dim y1,i_img_dim x2,i_img_dim y2,i_img_dim tx,i_img_dim ty);
  void(*i_copyto_trans)(i_img *im,i_img *src,i_img_dim x1,i_img_dim y1,i_img_dim x2,i_img_dim y2,i_img_dim tx,i_img_dim ty,const i_color *trans);
  int(*i_rubthru)(i_img *im,i_img *src,i_img_dim tx,i_img_dim ty, i_img_dim src_minx, i_img_dim src_miny, i_img_dim src_maxx, i_img_dim src_maxy);

} symbol_table_t;

#endif
