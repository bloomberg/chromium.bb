#ifndef _DATATYPES_H_
#define _DATATYPES_H_

#include <stddef.h>
#include "imconfig.h"

#define MAXCHANNELS 4

/*
=item im_context_t
=category Data Types

Imager's per-thread context.

=cut
*/

typedef struct im_context_tag *im_context_t;

/*
=item im_slot_t
=category Data Types

Represents a slot in the context object.

=cut
*/

typedef ptrdiff_t im_slot_t;
typedef void (*im_slot_destroy_t)(void *);

/* just so we can use our own input typemap */
typedef double im_double;
typedef float im_float;

/* used for palette indices in some internal code (which might be 
   exposed at some point
*/
typedef unsigned char i_palidx;

/* We handle 2 types of sample, this is hopefully the most common, and the
   smaller of the ones we support */
typedef unsigned char i_sample_t;

typedef struct { i_sample_t gray_color; } gray_color;
typedef struct { i_sample_t r,g,b; } rgb_color;
typedef struct { i_sample_t r,g,b,a; } rgba_color;
typedef struct { i_sample_t c,m,y,k; } cmyk_color;

typedef int undef_int; /* special value to put in typemaps to retun undef on 0 and 1 on 1 */

/*
=item i_img_dim
=category Data Types
=synopsis i_img_dim x, y;
=order 90

A signed integer type that represents an image dimension or ordinate.

May be larger than int on some platforms.

=cut
*/

typedef ptrdiff_t i_img_dim;

/*
=item i_img_dim_u
=category Data Types
=synopsis i_img_dim_u limit;
=order 90

An unsigned variant of L</i_img_dim>.

=cut
*/

typedef size_t i_img_dim_u;

#define i_img_dim_MAX ((i_img_dim)(~(i_img_dim_u)0 >> 1))

/*
=item i_color
=category Data Types
=synopsis i_color black;
=synopsis black.rgba.r = black.rgba.g = black.rgba.b = black.rgba.a = 0;

Type for 8-bit/sample color.

Samples as per;

  i_color c;

i_color is a union of:

=over

=item *

gray - contains a single element gray_color, eg. C<c.gray.gray_color>

=item *

C<rgb> - contains three elements C<r>, C<g>, C<b>, eg. C<c.rgb.r>

=item *

C<rgba> - contains four elements C<r>, C<g>, C<b>, C<a>, eg. C<c.rgba.a>

=item *

C<cmyk> - contains four elements C<c>, C<m>, C<y>, C<k>,
eg. C<c.cmyk.y>.  Note that Imager never uses CMYK colors except when
reading/writing files.

=item *

channels - an array of four channels, eg C<c.channels[2]>.

=back

=cut
*/

typedef union {
  gray_color gray;
  rgb_color rgb;
  rgba_color rgba;
  cmyk_color cmyk;
  i_sample_t channel[MAXCHANNELS];
  unsigned int ui;
} i_color;

/* this is the larger sample type, it should be able to accurately represent
   any sample size we use */
typedef double i_fsample_t;

typedef struct { i_fsample_t gray_color; } i_fgray_color_t;
typedef struct { i_fsample_t r, g, b; } i_frgb_color_t;
typedef struct { i_fsample_t r, g, b, a; } i_frgba_color_t;
typedef struct { i_fsample_t c, m, y, k; } i_fcmyk_color_t;

/*
=item i_fcolor
=category Data Types

This is the double/sample color type.

Its layout exactly corresponds to i_color.

=cut
*/

typedef union {
  i_fgray_color_t gray;
  i_frgb_color_t rgb;
  i_frgba_color_t rgba;
  i_fcmyk_color_t cmyk;
  i_fsample_t channel[MAXCHANNELS];
} i_fcolor;

typedef enum {
  i_direct_type, /* direct colour, keeps RGB values per pixel */
  i_palette_type /* keeps a palette index per pixel */
} i_img_type_t;

typedef enum { 
  /* bits per sample, not per pixel */
  /* a paletted image might have one bit per sample */
  i_8_bits = 8,
  i_16_bits = 16,
  i_double_bits = sizeof(double) * 8
} i_img_bits_t;

typedef struct {
  char *name; /* name of a given tag, might be NULL */
  int code; /* number of a given tag, -1 if it has no meaning */
  char *data; /* value of a given tag if it's not an int, may be NULL */
  int size; /* size of the data */
  int idata; /* value of a given tag if data is NULL */
} i_img_tag;

typedef struct {
  int count; /* how many tags have been set */
  int alloc; /* how many tags have been allocated for */
  i_img_tag *tags;
} i_img_tags;

typedef struct i_img_ i_img;
typedef int (*i_f_ppix_t)(i_img *im, i_img_dim x, i_img_dim y, const i_color *pix);
typedef int (*i_f_ppixf_t)(i_img *im, i_img_dim x, i_img_dim y, const i_fcolor *pix);
typedef i_img_dim (*i_f_plin_t)(i_img *im, i_img_dim x, i_img_dim r, i_img_dim y, const i_color *vals);
typedef i_img_dim (*i_f_plinf_t)(i_img *im, i_img_dim x, i_img_dim r, i_img_dim y, const i_fcolor *vals);
typedef int (*i_f_gpix_t)(i_img *im, i_img_dim x, i_img_dim y, i_color *pix);
typedef int (*i_f_gpixf_t)(i_img *im, i_img_dim x, i_img_dim y, i_fcolor *pix);
typedef i_img_dim (*i_f_glin_t)(i_img *im, i_img_dim x, i_img_dim r, i_img_dim y, i_color *vals);
typedef i_img_dim (*i_f_glinf_t)(i_img *im, i_img_dim x, i_img_dim r, i_img_dim y, i_fcolor *vals);

typedef i_img_dim (*i_f_gsamp_t)(i_img *im, i_img_dim x, i_img_dim r, i_img_dim y, i_sample_t *samp,
                           const int *chans, int chan_count);
typedef i_img_dim (*i_f_gsampf_t)(i_img *im, i_img_dim x, i_img_dim r, i_img_dim y, i_fsample_t *samp,
                            const int *chan, int chan_count);

typedef i_img_dim (*i_f_gpal_t)(i_img *im, i_img_dim x, i_img_dim r, i_img_dim y, i_palidx *vals);
typedef i_img_dim (*i_f_ppal_t)(i_img *im, i_img_dim x, i_img_dim r, i_img_dim y, const i_palidx *vals);
typedef int (*i_f_addcolors_t)(i_img *im, const i_color *colors, int count);
typedef int (*i_f_getcolors_t)(i_img *im, int i, i_color *, int count);
typedef int (*i_f_colorcount_t)(i_img *im);
typedef int (*i_f_maxcolors_t)(i_img *im);
typedef int (*i_f_findcolor_t)(i_img *im, const i_color *color, i_palidx *entry);
typedef int (*i_f_setcolors_t)(i_img *im, int index, const i_color *colors, 
                              int count);

typedef void (*i_f_destroy_t)(i_img *im);

typedef i_img_dim (*i_f_gsamp_bits_t)(i_img *im, i_img_dim x, i_img_dim r, i_img_dim y, unsigned *samp,
                           const int *chans, int chan_count, int bits);
typedef i_img_dim (*i_f_psamp_bits_t)(i_img *im, i_img_dim x, i_img_dim r, i_img_dim y, unsigned const *samp,
				 const int *chans, int chan_count, int bits);
typedef i_img_dim
(*i_f_psamp_t)(i_img *im, i_img_dim x, i_img_dim r, i_img_dim y, 
		const i_sample_t *samp, const int *chan, int chan_count);
typedef i_img_dim
(*i_f_psampf_t)(i_img *im, i_img_dim x, i_img_dim r, i_img_dim y,
		const i_fsample_t *samp, const int *chan, int chan_count);

/*
=item i_img
=category Data Types
=synopsis i_img *img;
=order 10

This is Imager's image type.

It contains the following members:

=over

=item *

C<channels> - the number of channels in the image

=item *

C<xsize>, C<ysize> - the width and height of the image in pixels

=item *

C<bytes> - the number of bytes used to store the image data.  Undefined
where virtual is non-zero.

=item *

C<ch_mask> - a mask of writable channels.  eg. if this is 6 then only
channels 1 and 2 are writable.  There may be bits set for which there
are no channels in the image.

=item *

C<bits> - the number of bits stored per sample.  Should be one of
i_8_bits, i_16_bits, i_double_bits.

=item *

C<type> - either i_direct_type for direct color images, or i_palette_type
for paletted images.

=item *

C<virtual> - if zero then this image is-self contained.  If non-zero
then this image could be an interface to some other implementation.

=item *

C<idata> - the image data.  This should not be directly accessed.  A new
image implementation can use this to store its image data.
i_img_destroy() will myfree() this pointer if it's non-null.

=item *

C<tags> - a structure storing the image's tags.  This should only be
accessed via the i_tags_*() functions.

=item *

C<ext_data> - a pointer for use internal to an image implementation.
This should be freed by the image's destroy handler.

=item *

C<im_data> - data internal to Imager.  This is initialized by
i_img_init().

=item *

i_f_ppix, i_f_ppixf, i_f_plin, i_f_plinf, i_f_gpix, i_f_gpixf,
i_f_glin, i_f_glinf, i_f_gsamp, i_f_gampf - implementations for each
of the required image functions.  An image implementation should
initialize these between calling i_img_alloc() and i_img_init().

=item *

i_f_gpal, i_f_ppal, i_f_addcolors, i_f_getcolors, i_f_colorcount,
i_f_maxcolors, i_f_findcolor, i_f_setcolors - implementations for each
paletted image function.

=item *

i_f_destroy - custom image destruction function.  This should be used
to release memory if necessary.

=item *

i_f_gsamp_bits - implements i_gsamp_bits() for this image.

=item *

i_f_psamp_bits - implements i_psamp_bits() for this image.

=item *

i_f_psamp - implements psamp() for this image.

=item *

i_f_psampf - implements psamp() for this image.

=item *

C<im_data> - image specific data internal to Imager.

=item *

C<context> - the Imager API context this image belongs to.

=back

=cut
*/

struct i_img_ {
  int channels;
  i_img_dim xsize,ysize;
  size_t bytes;
  unsigned int ch_mask;
  i_img_bits_t bits;
  i_img_type_t type;
  int virtual; /* image might not keep any data, must use functions */
  unsigned char *idata; /* renamed to force inspection of existing code */
                        /* can be NULL if virtual is non-zero */
  i_img_tags tags;

  void *ext_data;

  /* interface functions */
  i_f_ppix_t i_f_ppix;
  i_f_ppixf_t i_f_ppixf;
  i_f_plin_t i_f_plin;
  i_f_plinf_t i_f_plinf;
  i_f_gpix_t i_f_gpix;
  i_f_gpixf_t i_f_gpixf;
  i_f_glin_t i_f_glin;
  i_f_glinf_t i_f_glinf;
  i_f_gsamp_t i_f_gsamp;
  i_f_gsampf_t i_f_gsampf;
  
  /* only valid for type == i_palette_type */
  i_f_gpal_t i_f_gpal;
  i_f_ppal_t i_f_ppal;
  i_f_addcolors_t i_f_addcolors;
  i_f_getcolors_t i_f_getcolors;
  i_f_colorcount_t i_f_colorcount;
  i_f_maxcolors_t i_f_maxcolors;
  i_f_findcolor_t i_f_findcolor;
  i_f_setcolors_t i_f_setcolors;

  i_f_destroy_t i_f_destroy;

  /* as of 0.61 */
  i_f_gsamp_bits_t i_f_gsamp_bits;
  i_f_psamp_bits_t i_f_psamp_bits;

  /* as of 0.88 */
  i_f_psamp_t i_f_psamp;
  i_f_psampf_t i_f_psampf;

  void *im_data;

  /* 0.91 */
  im_context_t context;
};

/* ext_data for paletted images
 */
typedef struct {
  int count; /* amount of space used in palette (in entries) */
  int alloc; /* amount of space allocated for palette (in entries) */
  i_color *pal;
  int last_found;
} i_img_pal_ext;

/* Helper datatypes
  The types in here so far are:

  doubly linked bucket list - pretty efficient
  octtree - no idea about goodness
  
  needed: hashes.

*/

/* bitmap mask */

struct i_bitmap {
  i_img_dim xsize,ysize;
  char *data;
};

struct i_bitmap* btm_new(i_img_dim xsize,i_img_dim ysize);
void btm_destroy(struct i_bitmap *btm);
int btm_test(struct i_bitmap *btm,i_img_dim x,i_img_dim y);
void btm_set(struct i_bitmap *btm,i_img_dim x,i_img_dim y);


/* Stack/Linked list */

struct llink {
  struct llink *p,*n;
  void *data;
  int fill;		/* Number used in this link */
};

struct llist {
  struct llink *h,*t;
  int multip;		/* # of copies in a single chain  */
  size_t ssize;		/* size of each small element     */
  int count;           /* number of elements on the list */
};


/* Lists */

struct llist *llist_new( int multip, size_t ssize );
void llist_destroy( struct llist *l );
void llist_push( struct llist *l, const void *data );
void llist_dump( struct llist *l );
int llist_pop( struct llist *l,void *data );




/* Octtree */

struct octt {
  struct octt *t[8]; 
  int cnt;
};

struct octt *octt_new(void);
int octt_add(struct octt *ct,unsigned char r,unsigned char g,unsigned char b);
void octt_dump(struct octt *ct);
void octt_count(struct octt *ct,int *tot,int max,int *overflow);
void octt_delete(struct octt *ct);
void octt_histo(struct octt *ct, unsigned int **col_usage_it_adr);

/* font bounding box results */
enum bounding_box_index_t {
  BBOX_NEG_WIDTH,
  BBOX_GLOBAL_DESCENT,
  BBOX_POS_WIDTH,
  BBOX_GLOBAL_ASCENT,
  BBOX_DESCENT,
  BBOX_ASCENT,
  BBOX_ADVANCE_WIDTH,
  BBOX_RIGHT_BEARING,
  BOUNDING_BOX_COUNT
};

/*
=item i_polygon_t
=category Data Types

Represents a polygon.  Has the following members:

=over

=item *

C<x>, C<y> - arrays of x and y locations of vertices.

=item *

C<count> - the number of entries in the C<x> and C<y> arrays.

=back

=cut
*/

typedef struct i_polygon_tag {
  const double *x;
  const double *y;
  size_t count;
} i_polygon_t;

/*
=item i_poly_fill_mode_t
=category Data Types

Control how polygons are filled.  Has the following values:

=over

=item *

C<i_pfm_evenodd> - simple even-odd fills.

=item *

C<i_pfm_nonzero> - non-zero winding rule fills.

=back

=cut
*/

typedef enum i_poly_fill_mode_tag {
  i_pfm_evenodd,
  i_pfm_nonzero
} i_poly_fill_mode_t;

/* Generic fills */
struct i_fill_tag;

typedef void (*i_fill_with_color_f)
(struct i_fill_tag *fill, i_img_dim x, i_img_dim y, i_img_dim width, int channels, 
      i_color *data);
typedef void (*i_fill_with_fcolor_f)
     (struct i_fill_tag *fill, i_img_dim x, i_img_dim y, i_img_dim width, int channels,
      i_fcolor *data);
typedef void (*i_fill_destroy_f)(struct i_fill_tag *fill);

/* combine functions modify their target and are permitted to modify
   the source to prevent having to perform extra copying/memory
   allocations, etc
   The out array has I<channels> channels.

   The in array has I<channels> channels + an alpha channel if one
   isn't included in I<channels>.
*/

typedef void (*i_fill_combine_f)(i_color *out, i_color *in, int channels, 
                                 i_img_dim count);
typedef void (*i_fill_combinef_f)(i_fcolor *out, i_fcolor *in, int channels,
                                  i_img_dim count);

/* fountain fill types */
typedef enum {
  i_fst_linear,
  i_fst_curved,
  i_fst_sine,
  i_fst_sphere_up,
  i_fst_sphere_down,
  i_fst_end
} i_fountain_seg_type;
typedef enum {
  i_fc_direct,
  i_fc_hue_up,
  i_fc_hue_down,
  i_fc_end
} i_fountain_color;
typedef struct {
  double start, middle, end;
  i_fcolor c[2];
  i_fountain_seg_type type;
  i_fountain_color color;
} i_fountain_seg;
typedef enum {
  i_fr_none,
  i_fr_sawtooth,
  i_fr_triangle,
  i_fr_saw_both,
  i_fr_tri_both
} i_fountain_repeat;
typedef enum {
  i_ft_linear,
  i_ft_bilinear,
  i_ft_radial,
  i_ft_radial_square,
  i_ft_revolution,
  i_ft_conical,
  i_ft_end
} i_fountain_type;
typedef enum {
  i_fts_none,
  i_fts_grid,
  i_fts_random,
  i_fts_circle
} i_ft_supersample;

/*
=item i_fill_t
=category Data Types
=synopsis i_fill_t *fill;

This is the "abstract" base type for Imager's fill types.

Unless you're implementing a new fill type you'll typically treat this
as an opaque type.

=cut
*/

typedef struct i_fill_tag
{
  /* called for 8-bit/sample image (and maybe lower) */
  /* this may be NULL, if so call fill_with_fcolor */
  i_fill_with_color_f f_fill_with_color;

  /* called for other sample sizes */
  /* this must be non-NULL */
  i_fill_with_fcolor_f f_fill_with_fcolor;

  /* called if non-NULL to release any extra resources */
  i_fill_destroy_f destroy;

  /* if non-zero the caller will fill data with the original data
     from the image */
  i_fill_combine_f combine;
  i_fill_combinef_f combinef;
} i_fill_t;

typedef enum {
  ic_none,
  ic_normal,
  ic_multiply,
  ic_dissolve,
  ic_add,
  ic_subtract,
  ic_diff,
  ic_lighten,
  ic_darken,
  ic_hue,
  ic_sat,
  ic_value,
  ic_color
} i_combine_t;

/*
=item i_mutex_t
X<i_mutex>
=category mutex
=synopsis i_mutex_t mutex;

Opaque type for Imager's mutex API.

=cut
 */
typedef struct i_mutex_tag *i_mutex_t;

/*
   describes an axis of a MM font.
   Modelled on FT2's FT_MM_Axis.
   It would be nice to have a default entry too, but FT2 
   doesn't support it.
*/
typedef struct i_font_mm_axis_tag {
  char const *name;
  int minimum;
  int maximum;
} i_font_mm_axis;

#define IM_FONT_MM_MAX_AXES 4

/* 
   multiple master information for a font, if any 
   modelled on FT2's FT_Multi_Master.
*/
typedef struct i_font_mm_tag {
  int num_axis;
  int num_designs; /* provided but not necessarily useful */
  i_font_mm_axis axis[IM_FONT_MM_MAX_AXES];
} i_font_mm;

#ifdef HAVE_LIBTT

struct TT_Fonthandle_;

typedef struct TT_Fonthandle_ TT_Fonthandle;

#endif

/*
=item i_transp
=category Data Types

An enumerated type for controlling how transparency is handled during
quantization.

This has the following possible values:

=over

=item *

C<tr_none> - ignore the alpha channel

=item *

C<tr_threshold> - simple transparency thresholding.

=item *

C<tr_errdiff> - use error diffusion to control which pixels are
transparent.

=item *

C<tr_ordered> - use ordered dithering to control which pixels are
transparent.

=back

=cut
*/

/* transparency handling for quantized output */
typedef enum i_transp_tag {
  tr_none, /* ignore any alpha channel */
  tr_threshold, /* threshold the transparency - uses tr_threshold */
  tr_errdiff, /* error diffusion */
  tr_ordered /* an ordered dither */
} i_transp;

/*
=item i_make_colors
=category Data Types

An enumerated type used to control the method used for produce the
color map:

=over

=item *

C<mc_none> - the user supplied map is used.

=item *

C<mc_web_map> - use the classic web map.  Any existing fixed colors
are ignored.

=item *

C<mc_median_cut> - use median cut

=item *

C<mono> - use a fixed black and white map.

=item *

C<gray> - 256 step gray map.

=item *

C<gray4> - 4 step gray map.

=item *

C<gray16> - 16 step gray map.

=back

=cut
*/

typedef enum i_make_colors_tag {
  mc_none, /* user supplied colour map only */
  mc_web_map, /* Use the 216 colour web colour map */
  mc_addi, /* Addi's algorithm */
  mc_median_cut, /* median cut - similar to giflib, hopefully */
  mc_mono, /* fixed mono color map */
  mc_gray, /* 256 gray map */
  mc_gray4, /* four step gray map */
  mc_gray16, /* sixteen step gray map */
  mc_mask = 0xFF /* (mask for generator) */
} i_make_colors;

/*
=item i_translate
=category Data Types

An enumerated type that controls how colors are translated:

=over

=item *

C<pt_giflib> - obsolete, forces C<make_colors> to use median cut and
acts like C<pt_closest>.

=item *

C<pt_closest> - always use the closest color.

=item *

C<pt_perturb> - add random values to each sample and find the closest
color.

=item *

C<pt_errdiff> - error diffusion dither.

=back

=cut
*/

/* controls how we translate the colours */
typedef enum i_translate_tag {
  pt_giflib, /* get gif lib to do it (ignores make_colours) */
  pt_closest, /* just use the closest match within the hashbox */
  pt_perturb, /* randomly perturb the data - uses perturb_size*/
  pt_errdiff /* error diffusion dither - uses errdiff */
} i_translate;

/*
=item i_errdiff
=category Data Types

Controls the type of error diffusion to use:

=over

=item *

C<ed_floyd> - floyd-steinberg

=item *

C<ed_jarvis> - Jarvis, Judice and Ninke 

=item *

C<ed_stucki> - Stucki

=item *

C<ed_custom> - not usable for transparency dithering, allows a custom
error diffusion map to be used.

=item *

C<ed_bidir> - or with the error diffusion type to use alternate
directions on each line of the dither.

=back

=cut
*/

/* Which error diffusion map to use */
typedef enum i_errdiff_tag {
  ed_floyd, /* floyd-steinberg */
  ed_jarvis, /* Jarvis, Judice and Ninke */
  ed_stucki, /* Stucki */
  ed_custom, /* the map found in ed_map|width|height|orig */
  ed_mask = 0xFF, /* mask to get the map */
  ed_bidir = 0x100 /* change direction for each row */
} i_errdiff;

/*
=item i_ord_dith
=category Data Types

Which ordered dither map to use, currently only available for
transparency.  Values are:

=over

=item *

C<od_random> - a pre-generated random map.

=item *

C<od_dot8> - large dot dither.

=item *

C<od_dot4> - smaller dot dither

=item *

C<od_hline> - horizontal line dither.

=item *

C<od_vline> - vertical line dither.

=item *

C<od_slashline> - C</> line dither.

=item *

C<od_backline> - C<\> line dither.

=item *

C<od_tiny> - small checkbox dither

=item *

C<od_custom> - custom dither map.

=back

=cut

   I don't know of a way to do ordered dither of an image against some 
   general palette
 */
typedef enum i_ord_dith_tag
{
  od_random, /* sort of random */
  od_dot8, /* large dot */
  od_dot4,
  od_hline,
  od_vline,
  od_slashline, /* / line dither */
  od_backline, /* \ line dither */
  od_tiny, /* small checkerbox */
  od_custom /* custom 8x8 map */
} i_ord_dith;

/*
=item i_quantize
=category Data Types

A structure type used to supply image quantization, ie. when
converting a direct color image to a paletted image.

This has the following members:

=over

=item *

C<transp> - how to handle transparency, see L</i_transp>.

=item *

C<threshold> - when C<transp> is C<tr_threshold>, this is the alpha
level at which pixels become transparent.

=item *

C<tr_errdiff> - when C<transp> is C<tr_errdiff> this controls the type
of error diffusion to be done.  This may not be C<ed_custom> for this
member.

=item *

C<tr_orddith> - when C<transp> is C<tr_ordered> this controls the
patten used for dithering transparency.

=item *

C<tr_custom> - when C<tr_orddith> is C<tr_custom> this is the ordered
dither mask.

=item *

C<make_colors> - the method used to generate the color palette, see
L</i_make_colors>.

=item *

C<mc_colors> - an array of C<mc_size> L</i_color> entries used to
define the fixed colors (controlled by C<mc_count> and to return the
generated color list.

=item *

C<mc_size> - the size of the buffer allocated to C<mc_colors> in
C<sizeof(i_color)> units.

=item *

C<mc_count> - the number of initialized colors in C<mc_colors>.

=item *

C<translate> - how RGB colors are translated to palette indexes, see
L</i_translate>.

=item *

C<errdiff> - when C<translate> is C<pt_errdiff> this controls the type
of error diffusion to be done.

=item *

C<ed_map>, C<ed_width>, C<ed_height>, C<ed_orig> - when C<errdiff> is
C<ed_custom> this controls the error diffusion map.  C<ed_map> is an
array of C<ed_width * ed_height> integers.  C<ed_orig> is the position
of the current pixel in the error diffusion map, always on the top
row.

=item *

C<perturb> - the amount to perturb pixels when C<translate> is
C<mc_perturb>.

=back

=cut
*/
typedef struct i_quantize_tag {
  int version;

  /* how to handle transparency */
  i_transp transp;
  /* the threshold at which to make pixels opaque */
  int tr_threshold;
  i_errdiff tr_errdiff;
  i_ord_dith tr_orddith;
  unsigned char tr_custom[64];
  
  /* how to make the colour map */
  i_make_colors make_colors;

  /* any existing colours 
     mc_existing is an existing colour table
     mc_count is the number of existing colours
     mc_size is the total size of the array that mc_existing points
     at - this must be at least 256
  */
  i_color *mc_colors;
  int mc_size;
  int mc_count;

  /* how we translate the colours */
  i_translate translate;

  /* the error diffusion map to use if translate is mc_errdiff */
  i_errdiff errdiff;
  /* the following define the error diffusion values to use if 
     errdiff is ed_custom.  ed_orig is the column on the top row that
     represents the current 
  */
  int *ed_map;
  int ed_width, ed_height, ed_orig;

  /* the amount of perturbation to use for translate is mc_perturb */
  int perturb;
  /* version 2 members after here */
} i_quantize;

/* distance measures used by some filters */
enum {
  i_dmeasure_euclidean = 0,
  i_dmeasure_euclidean_squared = 1,
  i_dmeasure_manhatten = 2,
  i_dmeasure_limit = 2,
};

#include "iolayert.h"

/* error message information returned by im_errors() */

typedef struct {
  char *msg;
  int code;
} i_errmsg;

typedef struct i_render_tag i_render;

/*
=item i_color_model_t
=category Data Types
=order 95

Returned by L</i_img_color_model(im)> to indicate the color model of
the image.

An enumerated type with the following possible values:

=over

=item *

C<icm_unknown> - the image has no usable color data.  In future
versions of Imager this will be returned in a few limited cases,
eg. when the source image is CMYK and the user has requested no color
translation is done.

=item *

C<icm_gray> - gray scale with no alpha channel.

=item *

C<icm_gray_alpha> - gray scale with an alpha channel.

=item *

C<icm_rgb> - RGB

=item *

C<icm_rgb_alpha> - RGB with an alpha channel.

=back

=cut
*/

typedef enum {
  icm_unknown,
  icm_gray,
  icm_gray_alpha,
  icm_rgb,
  icm_rgb_alpha
} i_color_model_t;

#ifdef IMAGER_FORMAT_ATTR
#define I_FORMAT_ATTR(format_index, va_index) \
  __attribute ((format (printf, format_index, va_index)))
#else
#define I_FORMAT_ATTR(format_index, va_index)
#endif

#ifdef _MSC_VER
#  ifndef vsnprintf
#  define vsnprintf _vsnprintf
#  endif
#  ifndef snprintf
#  define snprintf _snprintf
#  endif
#endif

/*
=item i_DF
=category Data Types
=synopsis printf("left %" i_DF "\n", i_DFc(x));
=order 95

This is a constant string that can be used with functions like
printf() to format i_img_dim values after they're been cast with i_DFc().

Does not include the leading C<%>.

=cut

=item i_DFc
=category Data Types
=order 95

Cast an C<i_img_dim> value to a type for use with the i_DF format
string.

=cut

=item i_DFp
=category Data Types
=synopsis printf("point (" i_DFp ")\n", i_DFcp(x, y));
=order 95

Format a pair of C<i_img_dim> values.  This format string I<does>
include the leading C<%>.

=cut

=item i_DFcp
=category Data Types
=order 95

Casts two C<i_img_dim> values for use with the i_DF (or i_DFp) format.

=cut
 */

#define i_DFc(x) ((i_dim_format_t)(x))
#define i_DFcp(x, y) i_DFc(x), i_DFc(y)
#define i_DFp "%" i_DF ", %" i_DF

#endif

