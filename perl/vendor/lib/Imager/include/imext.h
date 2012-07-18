#ifndef IMAGER_IMEXT_H_
#define IMAGER_IMEXT_H_

#include "imexttypes.h"
#include "immacros.h"

extern im_ext_funcs *imager_function_ext_table;

#define DEFINE_IMAGER_CALLBACKS im_ext_funcs *imager_function_ext_table

#ifndef IMAGER_MIN_API_LEVEL
#define IMAGER_MIN_API_LEVEL IMAGER_API_LEVEL
#endif

#define PERL_INITIALIZE_IMAGER_CALLBACKS \
  do {  \
    imager_function_ext_table = INT2PTR(im_ext_funcs *, SvIV(get_sv(PERL_FUNCTION_TABLE_NAME, 1))); \
    if (!imager_function_ext_table) \
      croak("Imager API function table not found!"); \
    if (imager_function_ext_table->version != IMAGER_API_VERSION) {  \
      croak("Imager API version incorrect loaded %d vs expected %d", \
	    imager_function_ext_table->version, IMAGER_API_VERSION); \
    } \
    if (imager_function_ext_table->level < IMAGER_MIN_API_LEVEL) \
      croak("API level %d below minimum of %d", imager_function_ext_table->level, IMAGER_MIN_API_LEVEL); \
  } while (0)

/* just for use here */
#define im_extt imager_function_ext_table

#ifdef IMAGER_DEBUG_MALLOC

#define mymalloc(size) ((im_extt->f_mymalloc_file_line)((size), __FILE__, __LINE__))
#define myrealloc(ptr, size) ((im_extt->f_myrealloc_file_line)((ptr), (size), __FILE__, __LINE__))
#define myfree(ptr) ((im_extt->f_myfree_file_line)((ptr), __FILE__, __LINE__))

#else

#define mymalloc(size) ((im_extt->f_mymalloc)(size))
#define myfree(size) ((im_extt->f_myfree)(size))
#define myrealloc(block, newsize) ((im_extt->f_myrealloc)((block), (newsize)))

#endif

#define i_img_8_new(xsize, ysize, channels) ((im_extt->f_i_img_8_new)((xsize), (ysize), (channels)))
#define i_img_16_new(xsize, ysize, channels) ((im_extt->f_i_img_16_new)((xsize), (ysize), (channels)))
#define i_img_double_new(xsize, ysize, channels) ((im_extt->f_i_img_double_new)((xsize), (ysize), (channels)))
#define i_img_pal_new(xsize, ysize, channels, maxpal) ((im_extt->f_i_img_pal_new)((xsize), (ysize), (channels), (maxpal)))

#define i_img_destroy(im) ((im_extt->f_i_img_destroy)(im))
#define i_sametype(im, xsize, ysize) ((im_extt->f_i_sametype)((im), (xsize), (ysize)))
#define i_sametype_chans(im, xsize, ysize, channels) ((im_extt->f_i_sametype_chans)((im), (xsize), (ysize), (channels)))
#define i_img_info(im, info) ((im_extt->f_i_img_info)((im), (info)))

#ifndef IMAGER_DIRECT_IMAGE_CALLS
#define IMAGER_DIRECT_IMAGE_CALLS 1
#endif

#if IMAGER_DIRECT_IMAGE_CALLS
#define i_ppix(im, x, y, val) (((im)->i_f_ppix)((im), (x), (y), (val)))
#define i_gpix(im, x, y, val) (((im)->i_f_gpix)((im), (x), (y), (val)))
#define i_ppixf(im, x, y, val) (((im)->i_f_ppixf)((im), (x), (y), (val)))
#define i_gpixf(im, x, y, val) (((im)->i_f_gpixf)((im), (x), (y), (val)))
#define i_plin(im, l, r, y, val) (((im)->i_f_plin)(im, l, r, y, val))
#define i_glin(im, l, r, y, val) (((im)->i_f_glin)(im, l, r, y, val))
#define i_plinf(im, l, r, y, val) (((im)->i_f_plinf)(im, l, r, y, val))
#define i_glinf(im, l, r, y, val) (((im)->i_f_glinf)(im, l, r, y, val))

#define i_gsamp(im, l, r, y, samps, chans, count) \
  (((im)->i_f_gsamp)((im), (l), (r), (y), (samps), (chans), (count)))
#define i_gsampf(im, l, r, y, samps, chans, count) \
  (((im)->i_f_gsampf)((im), (l), (r), (y), (samps), (chans), (count)))

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
#else
#define i_ppix(im, x, y, val) ((im_extt->f_i_ppix)((im), (x), (y), (val)))
#define i_gpix(im, x, y, val) ((im_extt->f_i_gpix)((im), (x), (y), (val)))
#define i_ppixf(im, x, y, val) ((im_extt->f_i_ppixf)((im), (x), (y), (val)))
#define i_gpixf(im, x, y, val) ((im_extt->f_i_gpixf)((im), (x), (y), (val)))
#define i_plin(im, l, r, y, val) ((im_extt->f_i_plin)((im), (l), (r), (y), (val)))
#define i_glin(im, l, r, y, val) ((im_extt->f_i_glin)((im), (l), (r), (y), (val)))
#define i_plinf(im, l, r, y, val) ((im_extt->f_i_plinf)((im), (l), (r), (y), (val)))
#define i_glinf(im, l, r, y, val) ((im_extt->f_i_glinf)((im), (l), (r), (y), (val)))
#define i_gsamp(im, l, r, y, samps, chans, count) \
  ((im_extt->f_i_gsamp)((im), (l), (r), (y), (samps), (chans), (count)))
#define i_gsampf(im, l, r, y, samps, chans, count) \
  ((im_extt->f_i_gsampf)((im), (l), (r), (y), (samps), (chans), (count)))

#endif

#define i_gsamp_bits(im, l, r, y, samps, chans, count, bits) \
  (((im)->i_f_gsamp_bits) ? ((im)->i_f_gsamp_bits)((im), (l), (r), (y), (samps), (chans), (count), (bits)) : -1)
#define i_psamp_bits(im, l, r, y, samps, chans, count, bits) \
  (((im)->i_f_psamp_bits) ? ((im)->i_f_psamp_bits)((im), (l), (r), (y), (samps), (chans), (count), (bits)) : -1)

#define i_new_fill_solid(c, combine) ((im_extt->f_i_new_fill_solid)((c), (combine)))
#define i_new_fill_solidf(c, combine) ((im_extt->f_i_new_fill_solidf)((c), (combine)))
#define i_new_fill_hatch(fg, bg, combine, hatch, cust_hatch, dx, dy) \
  ((im_extt->f_i_new_fill_hatch)((fg), (bg), (combine), (hatch), (cust_hatch), (dx), (dy)))
#define i_new_fill_hatchf(fg, bg, combine, hatch, cust_hatch, dx, dy) \
  ((im_extt->f_i_new_fill_hatchf)((fg), (bg), (combine), (hatch), (cust_hatch), (dx), (dy)))
#define i_new_fill_image(im, matrix, xoff, yoff, combine) \
  ((im_extt->f_i_new_fill_image)((im), (matrix), (xoff), (yoff), (combine)))
#define i_new_fill_fount(xa, ya, xb, yb, type, repeat, combine, super_sample, ssample_param, count, segs) \
  ((im_extt->f_i_new_fill_fount)((xa), (ya), (xb), (yb), (type), (repeat), (combine), (super_sample), (ssample_param), (count), (segs)))
#define i_fill_destroy(fill) ((im_extt->f_i_fill_destroy)(fill))

#define i_quant_makemap(quant, imgs, count) \
  ((im_extt->f_i_quant_makemap)((quant), (imgs), (count)))
#define i_quant_translate(quant, img) \
  ((im_extt->f_i_quant_translate)((quant), (img)))
#define i_quant_transparent(quant, indices, img, trans_index) \
  ((im_extt->f_i_quant_transparent)((quant), (indices), (img), (trans_index)))

#define i_clear_error() ((im_extt->f_i_clear_error)())
#define i_push_error(code, msg) ((im_extt->f_i_push_error)((code), (msg)))
#define i_push_errorf (im_extt->f_i_push_errorf)
#define i_push_errorvf(code, fmt, list) \
  ((im_extt->f_i_push_errorvf)((code), (fmt), (list)))

#define i_tags_new(tags) ((im_extt->f_i_tags_new)(tags))
#define i_tags_set(tags, name, data, size) \
  ((im_extt->f_i_tags_set)((tags), (name), (data), (size)))
#define i_tags_setn(tags, name, idata) \
  ((im_extt->f_i_tags_setn)((tags), (name), (idata)))
#define i_tags_destroy(tags) ((im_extt->f_i_tags_destroy)(tags))
#define i_tags_find(tags, name, start, entry) \
  ((im_extt->f_i_tags_find)((tags), (name), (start), (entry)))
#define i_tags_findn(tags, code, start, entry) \
  ((im_extt->f_i_tags_findn)((tags), (code), (start), (entry)))
#define i_tags_delete(tags, entry) \
  ((im_extt->f_i_tags_delete)((tags), (entry)))
#define i_tags_delbyname(tags, name) \
  ((im_extt->f_i_tags_delbyname)((tags), (name)))
#define i_tags_delbycode(tags, code) \
  ((im_extt->f_i_tags_delbycode)((tags), (code)))
#define i_tags_get_float(tags, name, code, value) \
  ((im_extt->f_i_tags_get_float)((tags), (name), (code), (value)))
#define i_tags_set_float(tags, name, code, value) \
  ((im_extt->f_i_tags_set_float)((tags), (name), (code), (value)))
#define i_tags_set_float2(tags, name, code, value, places) \
  ((im_extt->f_i_tags_set_float2)((tags), (name), (code), (value), (places)))
#define i_tags_get_int(tags, name, code, value) \
  ((im_extt->f_i_tags_get_int)((tags), (name), (code), (value)))
#define i_tags_get_string(tags, name, code, value, value_size) \
  ((im_extt->f_i_tags_get_string)((tags), (name), (code), (value), (value_size)))
#define i_tags_get_color(tags, name, code, value) \
  ((im_extt->f_i_tags_get_color)((tags), (name), (code), (value)))
#define i_tags_set_color(tags, name, code, value) \
  ((im_extt->f_i_tags_set_color)((tags), (name), (code), (value)))

#define i_box(im, x1, y1, x2, y2, val) ((im_extt->f_i_box)((im), (x1), (y1), (x2), (y2), (val)))
#define i_box_filled(im, x1, y1, x2, y2, val) ((im_extt->f_i_box_filled)((im), (x1), (y1), (x2), (y2), (val)))
#define i_box_cfill(im, x1, y1, x2, y2, fill) ((im_extt->f_i_box_cfill)((im), (x1), (y1), (x2), (y2), (fill)))
#define i_line(im, x1, y1, x2, y2, val, endp) ((im_extt->f_i_line)((im), (x1), (y1), (x2), (y2), (val), (endp)))
#define i_line_aa(im, x1, y1, x2, y2, val, endp) ((im_extt->f_i_line_aa)((im), (x1), (y1), (x2), (y2), (val), (endp)))
#define i_arc(im, x, y, rad, d1, d2, val) ((im_extt->f_i_arc)((im), (x), (y), (rad), (d1), (d2), (val)))
#define i_arc_aa(im, x, y, rad, d1, d2, val) ((im_extt->f_i_arc_aa)((im), (x), (y), (rad), (d1), (d2), (val)))
#define i_arc_cfill(im, x, y, rad, d1, d2, fill) ((im_extt->f_i_arc_cfill)((im), (x), (y), (rad), (d1), (d2), (fill)))
#define i_arc_aa_cfill(im, x, y, rad, d1, d2, fill) ((im_extt->f_i_arc_aa_cfill)((im), (x), (y), (rad), (d1), (d2), (fill)))
#define i_circle_aa(im, x, y, rad, val) ((im_extt->f_i_circle_aa)((im), (x), (y), (rad), (val)))
#define i_flood_fill(im, seedx, seedy, dcol) ((im_extt->f_i_flood_fill)((im), (seedx), (seedy), (dcol)))
#define i_flood_cfill(im, seedx, seedy, fill) ((im_extt->f_i_flood_cfill)((im), (seedx), (seedy), (fill)))
#define i_flood_fill_border(im, seedx, seedy, dcol, border) ((im_extt->f_i_flood_fill_border)((im), (seedx), (seedy), (dcol), (border)))
#define i_flood_cfill_border(im, seedx, seedy, fill, border) ((im_extt->f_i_flood_cfill_border)((im), (seedx), (seedy), (fill), (border)))

#define i_copyto(im, src, x1, y1, x2, y2, tx, ty) \
  ((im_extt->f_i_copyto)((im), (src), (x1), (y1), (x2), (y2), (tx), (ty)))
#define i_copyto_trans(im, src, x1, y1, x2, y2, tx, ty, trans) \
  ((im_extt->f_i_copyto_trans)((im), (src), (x1), (y1), (x2), (y2), (tx), (ty), (trans)))
#define i_copy(im) ((im_extt->f_i_copy)(im))
#define i_rubthru(im, src, tx, ty, src_minx, src_miny, src_maxx, src_maxy) \
  ((im_extt->f_i_rubthru)((im), (src), (tx), (ty), (src_minx), (src_miny), (src_maxx), (src_maxy)))

#define i_set_image_file_limits(max_width, max_height, max_bytes) \
  ((im_extt->f_i_set_image_file_limits)((max_width), (max_height), (max_bytes)))
#define i_get_image_file_limits(pmax_width, pmax_height, pmax_bytes) \
  ((im_extt->f_i_get_image_file_limits)((pmax_width), (pmax_height), (pmax_bytes)))
#define i_int_check_image_file_limits(width, height, channels, sample_size) \
  ((im_extt->f_i_int_check_image_file_limits)((width), (height), (channels), (sample_size)))

#define i_img_setmask(img, mask) ((im_extt->f_i_img_setmask)((img), (mask)))
#define i_img_getmask(img) ((im_extt->f_i_img_getmask)(img))
#define i_img_getchannels(img) ((im_extt->f_i_img_getchannels)(img))
#define i_img_get_width(img) ((im_extt->f_i_img_get_width)(img))
#define i_img_get_height(img) ((im_extt->f_i_img_get_height)(img))
#define i_lhead(file, line) ((im_extt->f_i_lhead)((file), (line)))
#define i_loog (im_extt->f_i_loog)

#define i_img_alloc() ((im_extt->f_i_img_alloc)())
#define i_img_init(img) ((im_extt->f_i_img_init)(img))

#define i_img_is_monochrome(img, zero_is_white) ((im_extt->f_i_img_is_monochrome)((img), (zero_is_white)))

#define i_gsamp_bg(im, l, r, y, samples, out_channels, bg) \
  ((im_extt->f_i_gsamp_bg)((im), (l), (r), (y), (samples), (out_channels), (bg)))
#define i_gsampf_bg(im, l, r, y, samples, out_channels, bg) \
  ((im_extt->f_i_gsampf_bg)((im), (l), (r), (y), (samples), (out_channels), (bg)))
#define i_get_file_background(im, bg) \
  ((im_extt->f_i_get_file_background)((im), (bg)))
#define i_get_file_backgroundf(im, bg) \
  ((im_extt->f_i_get_file_backgroundf)((im), (bg)))

#define i_utf8_advance(p, s) ((im_extt->f_i_utf8_advance)((p), (s)))

#define i_render_new(im, width) ((im_extt->f_i_render_new)((im), (width)))
#define i_render_delete(r) ((im_extt->f_i_render_delete)(r))
#define i_render_color(r, x, y, width, src, color) \
  ((im_extt->f_i_render_color)((r), (x), (y), (width), (src), (color)))
#define i_render_fill(r, x, y, width, src, fill) \
  ((im_extt->f_i_render_fill)((r), (x), (y), (width), (src), (fill)))
#define i_render_line(r, x, y, width, src, line, combine) \
  ((im_extt->f_i_render_line)((r), (x), (y), (width), (src), (line), (combine)))
#define i_render_linef(r, x, y, width, src, line, combine) \
  ((im_extt->f_i_render_linef)((r), (x), (y), (width), (src), (line), (combine)))

#define i_io_getc_imp (im_extt->f_i_io_getc_imp)
#define i_io_peekc_imp (im_extt->f_i_io_peekc_imp)
#define i_io_peekn (im_extt->f_i_io_peekn)
#define i_io_putc_imp (im_extt->f_i_io_putc_imp)
#define i_io_read (im_extt->f_i_io_read)
#define i_io_write (im_extt->f_i_io_write)
#define i_io_seek (im_extt->f_i_io_seek)
#define i_io_flush (im_extt->f_i_io_flush)
#define i_io_close (im_extt->f_i_io_close)
#define i_io_set_buffered (im_extt->f_i_io_set_buffered)
#define i_io_gets (im_extt->f_i_io_gets)
#define io_new_fd(fd) ((im_extt->f_io_new_fd)(fd))
#define io_new_bufchain() ((im_extt->f_io_new_bufchain)())
#define io_new_buffer(data, len, closecb, closedata) \
  ((im_extt->f_io_new_buffer)((data), (len), (closecb), (closedata)))
#define io_new_cb(p, readcb, writecb, seekcb, closecb, destroycb) \
  ((im_extt->f_io_new_cb)((p), (readcb), (writecb), (seekcb), (closecb), (destroycb)))
#define io_slurp(ig, datap) ((im_extt->f_io_slurp)((ig), (datap)))
#define io_glue_destroy(ig) ((im_extt->f_io_glue_destroy)(ig))

#ifdef IMAGER_LOG
#define mm_log(x) { i_lhead(__FILE__,__LINE__); i_loog x; } 
#else
#define mm_log(x)
#endif


#endif
