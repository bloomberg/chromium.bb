#ifndef IMAGER_IMEXTTYPES_H_
#define IMAGER_IMEXTTYPES_H_

/* keep this file simple - apidocs.perl parses it. */

#include "imdatatypes.h"
#include <stdarg.h>

/*
 IMAGER_API_VERSION is similar to the version number in the third and
 fourth bytes of TIFF files - if it ever changes then the API has changed
 too much for any application to remain compatible.

 Version 2 changed the types of some parameters and pointers.  A
 simple recompile should be enough in most cases.

 Version 3 changed the behaviour of some of the I/O layer functions,
 and in some cases the initial seek position when calling file
 readers.  Switching away from calling readcb etc to i_io_read() etc
 should fix your code.

 Version 4 added i_psamp() and i_psampf() pointers to the i_img
 structure.

 Version 5 changed the return types of i_get_file_background() and
 i_get_file_backgroundf() from void to int.

*/
#define IMAGER_API_VERSION 5

/*
 IMAGER_API_LEVEL is the level of the structure.  New function pointers
 will always remain at the end (unless IMAGER_API_VERSION changes), and
 will result in an increment of IMAGER_API_LEVEL.
*/

#define IMAGER_API_LEVEL 10

typedef struct {
  int version;
  int level;

  /* IMAGER_API_LEVEL 1 functions */
  void * (*f_mymalloc)(size_t size);
  void (*f_myfree)(void *block);
  void * (*f_myrealloc)(void *block, size_t newsize);
  void* (*f_mymalloc_file_line)(size_t size, char* file, int line);
  void  (*f_myfree_file_line)(void *p, char*file, int line);
  void* (*f_myrealloc_file_line)(void *p, size_t newsize, char* file,int line);

  i_img *(*f_i_img_8_new)(i_img_dim xsize, i_img_dim ysize, int channels); /* SKIP */
  i_img *(*f_i_img_16_new)(i_img_dim xsize, i_img_dim ysize, int channels);  /* SKIP */
  i_img *(*f_i_img_double_new)(i_img_dim xsize, i_img_dim ysize, int channels); /* SKIP */
  i_img *(*f_i_img_pal_new)(i_img_dim xsize, i_img_dim ysize, int channels, int maxpal); /* SKIP */
  void (*f_i_img_destroy)(i_img *im);
  i_img *(*f_i_sametype)(i_img *im, i_img_dim xsize, i_img_dim ysize);
  i_img *(*f_i_sametype_chans)(i_img *im, i_img_dim xsize, i_img_dim ysize, int channels);
  void (*f_i_img_info)(i_img *im, i_img_dim *info);

  int (*f_i_ppix)(i_img *im, i_img_dim x, i_img_dim y, const i_color *val);
  int (*f_i_gpix)(i_img *im, i_img_dim x, i_img_dim y, i_color *val);
  int (*f_i_ppixf)(i_img *im, i_img_dim x, i_img_dim y, const i_fcolor *val);
  int (*f_i_gpixf)(i_img *im, i_img_dim x, i_img_dim y, i_fcolor *val);
  i_img_dim (*f_i_plin)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_color *vals);
  i_img_dim (*f_i_glin)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_color *vals);
  i_img_dim (*f_i_plinf)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_fcolor *vals);
  i_img_dim (*f_i_glinf)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fcolor *vals);
  i_img_dim (*f_i_gsamp)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_sample_t *samp, 
                   const int *chans, int chan_count);
  i_img_dim (*f_i_gsampf)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fsample_t *samp, 
                   const int *chans, int chan_count);
  i_img_dim (*f_i_gpal)(i_img *im, i_img_dim x, i_img_dim r, i_img_dim y, i_palidx *vals);
  i_img_dim (*f_i_ppal)(i_img *im, i_img_dim x, i_img_dim r, i_img_dim y, const i_palidx *vals);
  int (*f_i_addcolors)(i_img *im, const i_color *colors, int count);
  int (*f_i_getcolors)(i_img *im, int i, i_color *, int count);
  int (*f_i_colorcount)(i_img *im);
  int (*f_i_maxcolors)(i_img *im);
  int (*f_i_findcolor)(i_img *im, const i_color *color, i_palidx *entry);
  int (*f_i_setcolors)(i_img *im, int index, const i_color *colors, 
                       int count);

  i_fill_t *(*f_i_new_fill_solid)(const i_color *c, int combine);
  i_fill_t *(*f_i_new_fill_solidf)(const i_fcolor *c, int combine);

  i_fill_t *(*f_i_new_fill_hatch)(const i_color *fg, const i_color *bg, int combine, 
                                  int hatch, const unsigned char *cust_hatch, 
                                  i_img_dim dx, i_img_dim dy);
  i_fill_t *(*f_i_new_fill_hatchf)(const i_fcolor *fg, const i_fcolor *bg, int combine, 
                                  int hatch, const unsigned char *cust_hatch, 
                                  i_img_dim dx, i_img_dim dy);
  i_fill_t *(*f_i_new_fill_image)(i_img *im, const double *matrix, i_img_dim xoff, 
                                i_img_dim yoff, int combine);
  i_fill_t *(*f_i_new_fill_fount)(double xa, double ya, double xb, double yb, 
                 i_fountain_type type, i_fountain_repeat repeat, 
                 int combine, int super_sample, double ssample_param, 
                 int count, i_fountain_seg *segs);  

  void (*f_i_fill_destroy)(i_fill_t *fill);

  void (*f_i_quant_makemap)(i_quantize *quant, i_img **imgs, int count);
  i_palidx * (*f_i_quant_translate)(i_quantize *quant, i_img *img);
  void (*f_i_quant_transparent)(i_quantize *quant, i_palidx *indices, 
                                i_img *img, i_palidx trans_index);

  void (*f_i_clear_error)(void); /* SKIP */
  void (*f_i_push_error)(int code, char const *msg); /* SKIP */
  void (*f_i_push_errorf)(int code, char const *fmt, ...) I_FORMAT_ATTR(2,3);
  void (*f_i_push_errorvf)(int code, char const *fmt, va_list); /* SKIP */
  
  void (*f_i_tags_new)(i_img_tags *tags);
  int (*f_i_tags_set)(i_img_tags *tags, char const *name, char const *data, 
                      int size);
  int (*f_i_tags_setn)(i_img_tags *tags, char const *name, int idata);
  void (*f_i_tags_destroy)(i_img_tags *tags);
  int (*f_i_tags_find)(i_img_tags *tags, char const *name, int start, 
                       int *entry);
  int (*f_i_tags_findn)(i_img_tags *tags, int code, int start, int *entry);
  int (*f_i_tags_delete)(i_img_tags *tags, int entry);
  int (*f_i_tags_delbyname)(i_img_tags *tags, char const *name);
  int (*f_i_tags_delbycode)(i_img_tags *tags, int code);
  int (*f_i_tags_get_float)(i_img_tags *tags, char const *name, int code, 
                            double *value);
  int (*f_i_tags_set_float)(i_img_tags *tags, char const *name, int code, 
                            double value);
  int (*f_i_tags_set_float2)(i_img_tags *tags, char const *name, int code,
                             double value, int places);
  int (*f_i_tags_get_int)(i_img_tags *tags, char const *name, int code, 
                          int *value);
  int (*f_i_tags_get_string)(i_img_tags *tags, char const *name, int code,
                             char *value, size_t value_size);
  int (*f_i_tags_get_color)(i_img_tags *tags, char const *name, int code,
                            i_color *value);
  int (*f_i_tags_set_color)(i_img_tags *tags, char const *name, int code,
                            i_color const *value);

  void (*f_i_box)(i_img *im, i_img_dim x1, i_img_dim y1, i_img_dim x2, i_img_dim y2, const i_color *val);
  void (*f_i_box_filled)(i_img *im, i_img_dim x1, i_img_dim y1, i_img_dim x2, i_img_dim y2, const i_color *val);
  void (*f_i_box_cfill)(i_img *im, i_img_dim x1, i_img_dim y1, i_img_dim x2, i_img_dim y2, i_fill_t *fill);
  void (*f_i_line)(i_img *im, i_img_dim x1, i_img_dim y1, i_img_dim x2, i_img_dim y2, const i_color *val, int endp);
  void (*f_i_line_aa)(i_img *im, i_img_dim x1, i_img_dim y1, i_img_dim x2, i_img_dim y2, const i_color *val, int endp);
  void (*f_i_arc)(i_img *im, i_img_dim x, i_img_dim y, double rad, double d1, double d2, const i_color *val);
  void (*f_i_arc_aa)(i_img *im, double x, double y, double rad, double d1, double d2, const i_color *val);
  void (*f_i_arc_cfill)(i_img *im, i_img_dim x, i_img_dim y, double rad, double d1, double d2, i_fill_t *val);
  void (*f_i_arc_aa_cfill)(i_img *im, double x, double y, double rad, double d1, double d2, i_fill_t *fill);
  void (*f_i_circle_aa)(i_img *im, double x, double y, double rad, const i_color *val);
  int (*f_i_flood_fill)(i_img *im, i_img_dim seedx, i_img_dim seedy, const i_color *dcol);
  int (*f_i_flood_cfill)(i_img *im, i_img_dim seedx, i_img_dim seedy, i_fill_t *fill);

  void (*f_i_copyto)(i_img *im, i_img *src, i_img_dim x1, i_img_dim y1, i_img_dim x2, i_img_dim y2, i_img_dim tx, i_img_dim ty);
  void (*f_i_copyto_trans)(i_img *im, i_img *src, i_img_dim x1, i_img_dim y1, i_img_dim x2, i_img_dim y2, i_img_dim tx, i_img_dim ty, const i_color *trans);
  i_img *(*f_i_copy)(i_img *im);
  int (*f_i_rubthru)(i_img *im, i_img *src, i_img_dim tx, i_img_dim ty, i_img_dim src_minx, i_img_dim src_miny, i_img_dim src_maxx, i_img_dim src_maxy);

  /* IMAGER_API_LEVEL 2 functions */
  int (*f_i_set_image_file_limits)(i_img_dim width, i_img_dim height, size_t bytes); /* SKIP */
  int (*f_i_get_image_file_limits)(i_img_dim *width, i_img_dim *height, size_t *bytes); /* SKIP */
  int (*f_i_int_check_image_file_limits)(i_img_dim width, i_img_dim height, int channels, size_t sample_size); /* SKIP */
  int (*f_i_flood_fill_border)(i_img *im, i_img_dim seedx, i_img_dim seedy, const i_color *dcol, const i_color *border);
  int (*f_i_flood_cfill_border)(i_img *im, i_img_dim seedx, i_img_dim seedy, i_fill_t *fill, const i_color *border);

  /* IMAGER_API_LEVEL 3 functions */
  void (*f_i_img_setmask)(i_img *im, int ch_mask);
  int (*f_i_img_getmask)(i_img *im);
  int (*f_i_img_getchannels)(i_img *im);
  i_img_dim (*f_i_img_get_width)(i_img *im);
  i_img_dim (*f_i_img_get_height)(i_img *im);
  void (*f_i_lhead)(const char *file, int line_number);
  void (*f_i_loog)(int level, const char *msg, ...) I_FORMAT_ATTR(2,3);

  /* IMAGER_API_LEVEL 4 functions will be added here */
  i_img *(*f_i_img_alloc)(void); /* SKIP */
  void (*f_i_img_init)(i_img *); /* SKIP */

  /* IMAGER_API_LEVEL 5 functions will be added here */
  /* added i_psampf?_bits macros */
  int (*f_i_img_is_monochrome)(i_img *, int *zero_is_white);
  int (*f_i_gsamp_bg)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_sample_t *samples,
		      int out_channels, i_color const * bg);
  int (*f_i_gsampf_bg)(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fsample_t *samples,
		      int out_channels, i_fcolor const * bg);
  int (*f_i_get_file_background)(i_img *im, i_color *bg);
  int (*f_i_get_file_backgroundf)(i_img *im, i_fcolor *bg);
  unsigned long (*f_i_utf8_advance)(char const **p, size_t *len);
  i_render *(*f_i_render_new)(i_img *im, i_img_dim width);
  void (*f_i_render_delete)(i_render *r);
  void (*f_i_render_color)(i_render *r, i_img_dim x, i_img_dim y,
			   i_img_dim width, unsigned char const *src,
			   i_color const *color);
  void (*f_i_render_fill)(i_render *r, i_img_dim x, i_img_dim y,
			  i_img_dim width, unsigned char const *src,
			  i_fill_t *fill);
  void (*f_i_render_line)(i_render *r, i_img_dim x, i_img_dim y,
			  i_img_dim width, const i_sample_t *src,
			  i_color *line, i_fill_combine_f combine);
  void (*f_i_render_linef)(i_render *r, i_img_dim x, i_img_dim y,
			  i_img_dim width, const double *src,
			  i_fcolor *line, i_fill_combinef_f combine);

  /* Level 6 lost to mis-numbering */
  /* IMAGER_API_LEVEL 7 */
  int (*f_i_io_getc_imp)(io_glue *ig);
  int (*f_i_io_peekc_imp)(io_glue *ig);
  ssize_t (*f_i_io_peekn)(io_glue *ig, void *buf, size_t size);
  int (*f_i_io_putc_imp)(io_glue *ig, int c);
  ssize_t (*f_i_io_read)(io_glue *, void *buf, size_t size);
  ssize_t (*f_i_io_write)(io_glue *, const void *buf, size_t size);
  off_t (*f_i_io_seek)(io_glue *, off_t offset, int whence);
  int (*f_i_io_flush)(io_glue *ig);
  int (*f_i_io_close)(io_glue *ig);
  int (*f_i_io_set_buffered)(io_glue *ig, int buffered);
  ssize_t (*f_i_io_gets)(io_glue *ig, char *, size_t, int);

  i_io_glue_t *(*f_io_new_fd)(int fd); /* SKIP */
  i_io_glue_t *(*f_io_new_bufchain)(void); /* SKIP */
  i_io_glue_t *(*f_io_new_buffer)(const char *data, size_t len, i_io_closebufp_t closecb, void *closedata); /* SKIP */
  i_io_glue_t *(*f_io_new_cb)(void *p, i_io_readl_t readcb, i_io_writel_t writecb, i_io_seekl_t seekcb, i_io_closel_t closecb, i_io_destroyl_t destroycb); /* SKIP */
  size_t (*f_io_slurp)(i_io_glue_t *ig, unsigned char **c);
  void (*f_io_glue_destroy)(i_io_glue_t *ig);

  /* IMAGER_API_LEVEL 8 */
  i_img *(*f_im_img_8_new)(im_context_t ctx, i_img_dim xsize, i_img_dim ysize, int channels);
  i_img *(*f_im_img_16_new)(im_context_t ctx, i_img_dim xsize, i_img_dim ysize, int channels);
  i_img *(*f_im_img_double_new)(im_context_t ctx, i_img_dim xsize, i_img_dim ysize, int channels);
  i_img *(*f_im_img_pal_new)(im_context_t ctx, i_img_dim xsize, i_img_dim ysize, int channels, int maxpal);

  void (*f_im_clear_error)(im_context_t ctx);
  void (*f_im_push_error)(im_context_t ctx, int code, char const *msg);
  void (*f_im_push_errorvf)(im_context_t ctx, int code, char const *fmt, va_list);
  void (*f_im_push_errorf)(im_context_t , int code, char const *fmt, ...) I_FORMAT_ATTR(3,4);

  int (*f_im_set_image_file_limits)(im_context_t ctx, i_img_dim width, i_img_dim height, size_t bytes);
  int (*f_im_get_image_file_limits)(im_context_t ctx, i_img_dim *width, i_img_dim *height, size_t *bytes);
  int (*f_im_int_check_image_file_limits)(im_context_t ctx, i_img_dim width, i_img_dim height, int channels, size_t sample_size);

  i_img *(*f_im_img_alloc)(im_context_t ctx);
  void (*f_im_img_init)(im_context_t ctx, i_img *);

  i_io_glue_t *(*f_im_io_new_fd)(im_context_t ctx, int fd);
  i_io_glue_t *(*f_im_io_new_bufchain)(im_context_t ctx);
  i_io_glue_t *(*f_im_io_new_buffer)(im_context_t ctx, const char *data, size_t len, i_io_closebufp_t closecb, void *closedata);
  i_io_glue_t *(*f_im_io_new_cb)(im_context_t ctx, void *p, i_io_readl_t readcb, i_io_writel_t writecb, i_io_seekl_t seekcb, i_io_closel_t closecb, i_io_destroyl_t destroycb);

  im_context_t (*f_im_get_context)(void);

  void (*f_im_lhead)( im_context_t, const char *file, int line  );
  void (*f_im_loog)(im_context_t, int level,const char *msg, ... ) I_FORMAT_ATTR(3,4);
  void (*f_im_context_refinc)(im_context_t, const char *where);
  void (*f_im_context_refdec)(im_context_t, const char *where);
  i_errmsg *(*f_im_errors)(im_context_t);
  i_mutex_t (*f_i_mutex_new)(void);
  void (*f_i_mutex_destroy)(i_mutex_t m);
  void (*f_i_mutex_lock)(i_mutex_t m);
  void (*f_i_mutex_unlock)(i_mutex_t m);
  im_slot_t (*f_im_context_slot_new)(im_slot_destroy_t);
  int (*f_im_context_slot_set)(im_context_t, im_slot_t, void *);
  void *(*f_im_context_slot_get)(im_context_t, im_slot_t);

  /* IMAGER_API_LEVEL 9 */
  int (*f_i_poly_poly_aa)(i_img *im, int count, const i_polygon_t *polys,
			  i_poly_fill_mode_t mode, const i_color *val);
  int (*f_i_poly_poly_aa_cfill)(i_img *im, int count, const i_polygon_t *polys,
				i_poly_fill_mode_t mode, i_fill_t *fill);
  int (*f_i_poly_aa_m)(i_img *im, int l, const double *x, const double *y,
		       i_poly_fill_mode_t mode, const i_color *val);
  int (*f_i_poly_aa_cfill_m)(i_img *im, int l, const double *x, 
			     const double *y, i_poly_fill_mode_t mode,
			     i_fill_t *fill);

  int (*f_i_img_alpha_channel)(i_img *im, int *channel);
  i_color_model_t (*f_i_img_color_model)(i_img *im);
  int (*f_i_img_color_channels)(i_img *im);

  /* IMAGER_API_LEVEL 10 functions will be added here */
  int (*f_im_decode_exif)(i_img *im, const unsigned char *data, size_t length);

  /* IMAGER_API_LEVEL 11 functions will be added here */
} im_ext_funcs;

#define PERL_FUNCTION_TABLE_NAME "Imager::__ext_func_table"

#endif
