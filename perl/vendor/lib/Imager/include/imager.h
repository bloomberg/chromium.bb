#ifndef _IMAGE_H_
#define _IMAGE_H_

#include "imconfig.h"
#include "imio.h"
#include "iolayer.h"
#include "log.h"
#include "stackmach.h"


#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#ifdef SUNOS
#include <strings.h>
#endif

#ifndef PI
#define PI 3.14159265358979323846
#endif

#ifndef MAXINT
#define MAXINT 2147483647
#endif

#include "imdatatypes.h"

undef_int i_has_format(char *frmt);

/* constructors and destructors */

i_color *ICL_new_internal(            unsigned char r,unsigned char g,unsigned char b,unsigned char a);
i_color *ICL_set_internal(i_color *cl,unsigned char r,unsigned char g,unsigned char b,unsigned char a);
void     ICL_info        (const i_color *cl);
void     ICL_DESTROY     (i_color *cl);
void     ICL_add         (i_color *dst, i_color *src, int ch);

extern i_fcolor *i_fcolor_new(double r, double g, double b, double a);
extern void i_fcolor_destroy(i_fcolor *cl);

extern void i_rgb_to_hsvf(i_fcolor *color);
extern void i_hsv_to_rgbf(i_fcolor *color);
extern void i_rgb_to_hsv(i_color *color);
extern void i_hsv_to_rgb(i_color *color);

i_img *IIM_new(i_img_dim x,i_img_dim y,int ch);
#define i_img_8_new IIM_new
void   IIM_DESTROY(i_img *im);
i_img *i_img_new( void );
i_img *i_img_empty(i_img *im,i_img_dim x,i_img_dim y);
i_img *i_img_empty_ch(i_img *im,i_img_dim x,i_img_dim y,int ch);
void   i_img_exorcise(i_img *im);
void   i_img_destroy(i_img *im);
i_img *i_img_alloc(void);
void i_img_init(i_img *im);

void   i_img_info(i_img *im,i_img_dim *info);

extern i_img *i_sametype(i_img *im, i_img_dim xsize, i_img_dim ysize);
extern i_img *i_sametype_chans(i_img *im, i_img_dim xsize, i_img_dim ysize, int channels);

i_img *i_img_pal_new(i_img_dim x, i_img_dim y, int ch, int maxpal);

/* Image feature settings */

void   i_img_setmask    (i_img *im,int ch_mask);
int    i_img_getmask    (i_img *im);
int    i_img_getchannels(i_img *im);
i_img_dim i_img_get_width(i_img *im);
i_img_dim i_img_get_height(i_img *im);

/* Base functions */

extern int i_ppix(i_img *im,i_img_dim x,i_img_dim y, const i_color *val);
extern int i_gpix(i_img *im,i_img_dim x,i_img_dim y,i_color *val);
extern int i_ppixf(i_img *im,i_img_dim x,i_img_dim y, const i_fcolor *val);
extern int i_gpixf(i_img *im,i_img_dim x,i_img_dim y,i_fcolor *val);

#define i_ppix(im, x, y, val) (((im)->i_f_ppix)((im), (x), (y), (val)))
#define i_gpix(im, x, y, val) (((im)->i_f_gpix)((im), (x), (y), (val)))
#define i_ppixf(im, x, y, val) (((im)->i_f_ppixf)((im), (x), (y), (val)))
#define i_gpixf(im, x, y, val) (((im)->i_f_gpixf)((im), (x), (y), (val)))

extern i_img_dim i_plin(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_color *vals);
extern i_img_dim i_glin(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_color *vals);
extern i_img_dim i_plinf(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, const i_fcolor *vals);
extern i_img_dim i_glinf(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fcolor *vals);
extern i_img_dim i_gsamp(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_sample_t *samp, 
                   const int *chans, int chan_count);
extern i_img_dim i_gsampf(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fsample_t *samp, 
                   const int *chans, int chan_count);
extern i_img_dim i_gpal(i_img *im, i_img_dim x, i_img_dim r, i_img_dim y, i_palidx *vals);
extern i_img_dim i_ppal(i_img *im, i_img_dim x, i_img_dim r, i_img_dim y, const i_palidx *vals);
extern int i_addcolors(i_img *im, const i_color *colors, int count);
extern int i_getcolors(i_img *im, int i, i_color *, int count);
extern int i_colorcount(i_img *im);
extern int i_maxcolors(i_img *im);
extern int i_findcolor(i_img *im, const i_color *color, i_palidx *entry);
extern int i_setcolors(i_img *im, int index, const i_color *colors, 
                       int count);

#define i_plin(im, l, r, y, val) (((im)->i_f_plin)(im, l, r, y, val))
#define i_glin(im, l, r, y, val) (((im)->i_f_glin)(im, l, r, y, val))
#define i_plinf(im, l, r, y, val) (((im)->i_f_plinf)(im, l, r, y, val))
#define i_glinf(im, l, r, y, val) (((im)->i_f_glinf)(im, l, r, y, val))

#define i_gsamp(im, l, r, y, samps, chans, count) \
  (((im)->i_f_gsamp)((im), (l), (r), (y), (samps), (chans), (count)))
#define i_gsampf(im, l, r, y, samps, chans, count) \
  (((im)->i_f_gsampf)((im), (l), (r), (y), (samps), (chans), (count)))

#define i_gsamp_bits(im, l, r, y, samps, chans, count, bits) \
  (((im)->i_f_gsamp_bits) ? ((im)->i_f_gsamp_bits)((im), (l), (r), (y), (samps), (chans), (count), (bits)) : -1)
#define i_psamp_bits(im, l, r, y, samps, chans, count, bits) \
  (((im)->i_f_psamp_bits) ? ((im)->i_f_psamp_bits)((im), (l), (r), (y), (samps), (chans), (count), (bits)) : -1)

#define i_findcolor(im, color, entry) \
  (((im)->i_f_findcolor) ? ((im)->i_f_findcolor)((im), (color), (entry)) : 0)

#define i_gpal(im, l, r, y, vals) \
  (((im)->i_f_gpal) ? ((im)->i_f_gpal)((im), (l), (r), (y), (vals)) : 0)
#define i_ppal(im, l, r, y, vals) \
  (((im)->i_f_ppal) ? ((im)->i_f_ppal)((im), (l), (r), (y), (vals)) : 0)
#define i_addcolors(im, colors, count) \
  (((im)->i_f_addcolors) ? ((im)->i_f_addcolors)((im), (colors), (count)) : -1)
#define i_getcolors(im, index, color, count) \
  (((im)->i_f_getcolors) ? \
   ((im)->i_f_getcolors)((im), (index), (color), (count)) : 0)
#define i_setcolors(im, index, color, count) \
  (((im)->i_f_setcolors) ? \
   ((im)->i_f_setcolors)((im), (index), (color), (count)) : 0)
#define i_colorcount(im) \
  (((im)->i_f_colorcount) ? ((im)->i_f_colorcount)(im) : -1)
#define i_maxcolors(im) \
  (((im)->i_f_maxcolors) ? ((im)->i_f_maxcolors)(im) : -1)
#define i_findcolor(im, color, entry) \
  (((im)->i_f_findcolor) ? ((im)->i_f_findcolor)((im), (color), (entry)) : 0)

#define i_img_virtual(im) ((im)->virtual)
#define i_img_type(im) ((im)->type)
#define i_img_bits(im) ((im)->bits)

extern i_fill_t *i_new_fill_solidf(const i_fcolor *c, int combine);
extern i_fill_t *i_new_fill_solid(const i_color *c, int combine);
extern i_fill_t *
i_new_fill_hatch(const i_color *fg, const i_color *bg, int combine, int hatch, 
                 const unsigned char *cust_hatch, i_img_dim dx, i_img_dim dy);
extern i_fill_t *
i_new_fill_hatchf(const i_fcolor *fg, const i_fcolor *bg, int combine, int hatch, 
                  const unsigned char *cust_hatch, i_img_dim dx, i_img_dim dy);
extern i_fill_t *
i_new_fill_image(i_img *im, const double *matrix, i_img_dim xoff, i_img_dim yoff, int combine);
extern i_fill_t *i_new_fill_opacity(i_fill_t *, double alpha_mult);
extern void i_fill_destroy(i_fill_t *fill);

float i_gpix_pch(i_img *im,i_img_dim x,i_img_dim y,int ch);

/* functions for drawing primitives */

void i_box         (i_img *im,i_img_dim x1,i_img_dim y1,i_img_dim x2,i_img_dim y2,const i_color *val);
void i_box_filled  (i_img *im,i_img_dim x1,i_img_dim y1,i_img_dim x2,i_img_dim y2,const i_color *val);
int i_box_filledf  (i_img *im,i_img_dim x1,i_img_dim y1,i_img_dim x2,i_img_dim y2,const i_fcolor *val);
void i_box_cfill(i_img *im, i_img_dim x1, i_img_dim y1, i_img_dim x2, i_img_dim y2, i_fill_t *fill);
void i_line        (i_img *im,i_img_dim x1,i_img_dim y1,i_img_dim x2,i_img_dim y2,const i_color *val, int endp);
void i_line_aa     (i_img *im,i_img_dim x1,i_img_dim y1,i_img_dim x2,i_img_dim y2,const i_color *val, int endp);
void i_arc         (i_img *im,i_img_dim x,i_img_dim y,double rad,double d1,double d2,const i_color *val);
int i_arc_out(i_img *im,i_img_dim x,i_img_dim y,i_img_dim rad,double d1,double d2,const i_color *val);
int i_arc_out_aa(i_img *im,i_img_dim x,i_img_dim y,i_img_dim rad,double d1,double d2,const i_color *val);
void i_arc_aa         (i_img *im, double x, double y, double rad, double d1, double d2, const i_color *val);
void i_arc_cfill(i_img *im,i_img_dim x,i_img_dim y,double rad,double d1,double d2,i_fill_t *fill);
void i_arc_aa_cfill(i_img *im,double x,double y,double rad,double d1,double d2,i_fill_t *fill);
void i_circle_aa   (i_img *im,double x, double y,double rad,const i_color *val);
int i_circle_out   (i_img *im,i_img_dim x, i_img_dim y, i_img_dim rad,const i_color *val);
int i_circle_out_aa   (i_img *im,i_img_dim x, i_img_dim y, i_img_dim rad,const i_color *val);
void i_copyto      (i_img *im,i_img *src,i_img_dim x1,i_img_dim y1,i_img_dim x2,i_img_dim y2,i_img_dim tx,i_img_dim ty);
void i_copyto_trans(i_img *im,i_img *src,i_img_dim x1,i_img_dim y1,i_img_dim x2,i_img_dim y2,i_img_dim tx,i_img_dim ty,const i_color *trans);
i_img* i_copy        (i_img *src);
int  i_rubthru     (i_img *im, i_img *src, i_img_dim tx, i_img_dim ty, i_img_dim src_minx, i_img_dim src_miny, i_img_dim src_maxx, i_img_dim src_maxy);
extern int 
i_compose_mask(i_img *out, i_img *src, i_img *mask,
	       i_img_dim out_left, i_img_dim out_top, i_img_dim src_left, i_img_dim src_top,
	       i_img_dim mask_left, i_img_dim mask_top, i_img_dim width, i_img_dim height,
	       int combine, double opacity);
extern int 
i_compose(i_img *out, i_img *src,
	       i_img_dim out_left, i_img_dim out_top, i_img_dim src_left, i_img_dim src_top,
	       i_img_dim width, i_img_dim height, int combine, double opacity);

extern i_img *
i_combine(i_img **src, const int *channels, int in_count);

undef_int i_flipxy (i_img *im, int direction);
extern i_img *i_rotate90(i_img *im, int degrees);
extern i_img *i_rotate_exact(i_img *im, double amount);
extern i_img *i_rotate_exact_bg(i_img *im, double amount, const i_color *backp, const i_fcolor *fbackp);
extern i_img *i_matrix_transform(i_img *im, i_img_dim xsize, i_img_dim ysize, const double *matrix);
extern i_img *i_matrix_transform_bg(i_img *im, i_img_dim xsize, i_img_dim ysize, const double *matrix,  const i_color *backp, const i_fcolor *fbackp);

void i_bezier_multi(i_img *im,int l,const double *x,const double *y,const i_color *val);
int i_poly_aa     (i_img *im,int l,const double *x,const double *y,const i_color *val);
int i_poly_aa_cfill(i_img *im,int l,const double *x,const double *y,i_fill_t *fill);

undef_int i_flood_fill  (i_img *im,i_img_dim seedx,i_img_dim seedy, const i_color *dcol);
undef_int i_flood_cfill(i_img *im, i_img_dim seedx, i_img_dim seedy, i_fill_t *fill);
undef_int i_flood_fill_border  (i_img *im,i_img_dim seedx,i_img_dim seedy, const i_color *dcol, const i_color *border);
undef_int i_flood_cfill_border(i_img *im, i_img_dim seedx, i_img_dim seedy, i_fill_t *fill, const i_color *border);


/* image processing functions */

int i_gaussian    (i_img *im, double stdev);
int i_conv        (i_img *im,const double *coeff,int len);
void i_unsharp_mask(i_img *im, double stddev, double scale);

/* colour manipulation */
extern i_img *i_convert(i_img *src, const double *coeff, int outchan, int inchan);
extern void i_map(i_img *im, unsigned char (*maps)[256], unsigned int mask);

float i_img_diff   (i_img *im1,i_img *im2);
double i_img_diffd(i_img *im1,i_img *im2);
int i_img_samef(i_img *im1,i_img *im2, double epsilon, const char *what);

/* font routines */

undef_int i_init_fonts( int t1log );

#ifdef HAVE_LIBTT

undef_int i_init_tt( void );
TT_Fonthandle* i_tt_new(const char *fontname);
void i_tt_destroy( TT_Fonthandle *handle );
undef_int i_tt_cp( TT_Fonthandle *handle,i_img *im,i_img_dim xb,i_img_dim yb,int channel,double points,char const* txt,size_t len,int smooth, int utf8, int align);
undef_int i_tt_text( TT_Fonthandle *handle, i_img *im, i_img_dim xb, i_img_dim yb, const i_color *cl, double points, char const* txt, size_t len, int smooth, int utf8, int align);
undef_int i_tt_bbox( TT_Fonthandle *handle, double points,const char *txt,size_t len,i_img_dim cords[6], int utf8);
size_t i_tt_has_chars(TT_Fonthandle *handle, char const *text, size_t len, int utf8, char *out);
void i_tt_dump_names(TT_Fonthandle *handle);
size_t i_tt_face_name(TT_Fonthandle *handle, char *name_buf, 
                   size_t name_buf_size);
size_t i_tt_glyph_name(TT_Fonthandle *handle, unsigned long ch, char *name_buf,
                    size_t name_buf_size);

#endif  /* End of freetype headers */

extern void i_quant_makemap(i_quantize *quant, i_img **imgs, int count);
extern i_palidx *i_quant_translate(i_quantize *quant, i_img *img);
extern void i_quant_transparent(i_quantize *quant, i_palidx *indices, i_img *img, i_palidx trans_index);

extern i_img *i_img_pal_new(i_img_dim x, i_img_dim y, int channels, int maxpal);
extern i_img *i_img_to_pal(i_img *src, i_quantize *quant);
extern i_img *i_img_to_rgb(i_img *src);
extern i_img *i_img_masked_new(i_img *targ, i_img *mask, i_img_dim x, i_img_dim y, 
                               i_img_dim w, i_img_dim h);
extern i_img *i_img_16_new(i_img_dim x, i_img_dim y, int ch);
extern i_img *i_img_to_rgb16(i_img *im);
extern i_img *i_img_double_new(i_img_dim x, i_img_dim y, int ch);
extern i_img *i_img_to_drgb(i_img *im);

extern int i_img_is_monochrome(i_img *im, int *zero_is_white);
extern int i_get_file_background(i_img *im, i_color *bg);
extern int i_get_file_backgroundf(i_img *im, i_fcolor *bg);

const char * i_test_format_probe(io_glue *data, int length);


i_img   * i_readraw_wiol(io_glue *ig, i_img_dim x, i_img_dim y, int datachannels, int storechannels, int intrl);
undef_int i_writeraw_wiol(i_img* im, io_glue *ig);

i_img   * i_readpnm_wiol(io_glue *ig, int allow_incomplete);
i_img   ** i_readpnm_multi_wiol(io_glue *ig, int *count, int allow_incomplete);
undef_int i_writeppm_wiol(i_img *im, io_glue *ig);

extern int    i_writebmp_wiol(i_img *im, io_glue *ig);
extern i_img *i_readbmp_wiol(io_glue *ig, int allow_incomplete);

int tga_header_verify(unsigned char headbuf[18]);

i_img   * i_readtga_wiol(io_glue *ig, int length);
undef_int i_writetga_wiol(i_img *img, io_glue *ig, int wierdpack, int compress, char *idstring, size_t idlen);

i_img   * i_readrgb_wiol(io_glue *ig, int length);
undef_int i_writergb_wiol(i_img *img, io_glue *ig, int wierdpack, int compress, char *idstring, size_t idlen);

i_img * i_scaleaxis(i_img *im, double Value, int Axis);
i_img * i_scale_nn(i_img *im, double scx, double scy);
i_img * i_scale_mixing(i_img *src, i_img_dim width, i_img_dim height);
i_img * i_haar(i_img *im);
int     i_count_colors(i_img *im,int maxc);
int i_get_anonymous_color_histo(i_img *im, unsigned int **col_usage, int maxc);

i_img * i_transform(i_img *im, int *opx, int opxl, int *opy,int opyl,double parm[],int parmlen);

struct rm_op;
i_img * i_transform2(i_img_dim width, i_img_dim height, int channels,
		     struct rm_op *ops, int ops_count, 
		     double *n_regs, int n_regs_count, 
		     i_color *c_regs, int c_regs_count, 
		     i_img **in_imgs, int in_imgs_count);

/* filters */

void i_contrast(i_img *im, float intensity);
void i_hardinvert(i_img *im);
void i_hardinvertall(i_img *im);
void i_noise(i_img *im, float amount, unsigned char type);
void i_bumpmap(i_img *im,i_img *bump,int channel,i_img_dim light_x,i_img_dim light_y,i_img_dim strength);
void i_bumpmap_complex(i_img *im, i_img *bump, int channel, i_img_dim tx, i_img_dim ty, double Lx, double Ly, 
		       double Lz, float cd, float cs, float n, i_color *Ia, i_color *Il, i_color *Is);
void i_postlevels(i_img *im,int levels);
void i_mosaic(i_img *im,i_img_dim size);
void i_watermark(i_img *im,i_img *wmark,i_img_dim tx,i_img_dim ty,int pixdiff);
void i_autolevels(i_img *im,float lsat,float usat,float skew);
void i_radnoise(i_img *im,i_img_dim xo,i_img_dim yo,double rscale,double ascale);
void i_turbnoise(i_img *im,double xo,double yo,double scale);
void i_gradgen(i_img *im, int num, i_img_dim *xo, i_img_dim *yo, i_color *ival, int dmeasure);
int i_nearest_color(i_img *im, int num, i_img_dim *xo, i_img_dim *yo, i_color *ival, int dmeasure);
i_img *i_diff_image(i_img *im, i_img *im2, double mindist);
int
i_fountain(i_img *im, double xa, double ya, double xb, double yb, 
           i_fountain_type type, i_fountain_repeat repeat, 
           int combine, int super_sample, double ssample_param,
           int count, i_fountain_seg *segs);
extern i_fill_t *
i_new_fill_fount(double xa, double ya, double xb, double yb, 
                 i_fountain_type type, i_fountain_repeat repeat, 
                 int combine, int super_sample, double ssample_param, 
                 int count, i_fountain_seg *segs);

/* Debug only functions */

void malloc_state( void );

/* this is sort of obsolete now */

typedef struct {
  undef_int (*i_has_format)(char *frmt);
  i_color*(*ICL_set)(i_color *cl,unsigned char r,unsigned char g,unsigned char b,unsigned char a);
  void (*ICL_info)(const i_color *cl);

  i_img*(*i_img_new)( void );
  i_img*(*i_img_empty)(i_img *im,i_img_dim x,i_img_dim y);
  i_img*(*i_img_empty_ch)(i_img *im,i_img_dim x,i_img_dim y,int ch);
  void(*i_img_exorcise)(i_img *im);

  void(*i_img_info)(i_img *im,i_img_dim *info);
  
  void(*i_img_setmask)(i_img *im,int ch_mask);
  int (*i_img_getmask)(i_img *im);
  
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

#include "imerror.h"

/* image tag processing */
extern void i_tags_new(i_img_tags *tags);
extern int i_tags_addn(i_img_tags *tags, char const *name, int code, 
                       int idata);
extern int i_tags_add(i_img_tags *tags, char const *name, int code, 
                      char const *data, int size, int idata);
extern int i_tags_set(i_img_tags *tags, char const *name,
                      char const *data, int size);
extern int i_tags_setn(i_img_tags *tags, char const *name, int idata);
                       
extern void i_tags_destroy(i_img_tags *tags);
extern int i_tags_find(i_img_tags *tags, char const *name, int start, 
                       int *entry);
extern int i_tags_findn(i_img_tags *tags, int code, int start, int *entry);
extern int i_tags_delete(i_img_tags *tags, int entry);
extern int i_tags_delbyname(i_img_tags *tags, char const *name);
extern int i_tags_delbycode(i_img_tags *tags, int code);
extern int i_tags_get_float(i_img_tags *tags, char const *name, int code, 
			    double *value);
extern int i_tags_set_float(i_img_tags *tags, char const *name, int code, 
			    double value);
extern int i_tags_set_float2(i_img_tags *tags, char const *name, int code, 
			    double value, int places);
extern int i_tags_get_int(i_img_tags *tags, char const *name, int code, 
                          int *value);
extern int i_tags_get_string(i_img_tags *tags, char const *name, int code, 
			     char *value, size_t value_size);
extern int i_tags_get_color(i_img_tags *tags, char const *name, int code, 
                            i_color *value);
extern int i_tags_set_color(i_img_tags *tags, char const *name, int code, 
                            i_color const *value);
extern void i_tags_print(i_img_tags *tags);

/* image file limits */
extern int
i_set_image_file_limits(i_img_dim width, i_img_dim height, size_t bytes);
extern int
i_get_image_file_limits(i_img_dim *width, i_img_dim *height, size_t *bytes);
extern int
i_int_check_image_file_limits(i_img_dim width, i_img_dim height, int channels, size_t sample_size);

/* memory allocation */
void* mymalloc(size_t size);
void  myfree(void *p);
void* myrealloc(void *p, size_t newsize);
void* mymalloc_file_line (size_t size, char* file, int line);
void  myfree_file_line   (void *p, char*file, int line);
void* myrealloc_file_line(void *p, size_t newsize, char* file,int line);

#ifdef IMAGER_DEBUG_MALLOC

#define mymalloc(x) (mymalloc_file_line((x), __FILE__, __LINE__))
#define myrealloc(x,y) (myrealloc_file_line((x),(y), __FILE__, __LINE__))
#define myfree(x) (myfree_file_line((x), __FILE__, __LINE__))

void  malloc_state       (void);
void  bndcheck_all       (void);

#else

void  malloc_state(void);

#endif /* IMAGER_MALLOC_DEBUG */

#include "imrender.h"
#include "immacros.h"

extern void
i_adapt_colors(int dest_channels, int src_channels, i_color *colors, 
	       size_t count);
extern void
i_adapt_fcolors(int dest_channels, int src_channels, i_fcolor *colors, 
	       size_t count);

extern void
i_adapt_colors_bg(int dest_channels, int src_channels, i_color *colors, 
		  size_t count, i_color const *bg);
extern void
i_adapt_fcolors_bg(int dest_channels, int src_channels, i_fcolor *colors, 
		   size_t count, i_fcolor const *bg);

extern int
i_gsamp_bg(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_sample_t *samples, 
	   int out_channels, i_color const *bg);

extern int
i_gsampf_bg(i_img *im, i_img_dim l, i_img_dim r, i_img_dim y, i_fsample_t *samples, 
	   int out_channels, i_fcolor const *bg);

#include "imio.h"

#endif
