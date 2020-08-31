/* 
   Imager "functions" implemented as macros 

   I suppose these could go in imdatatypes, but they aren't types.
*/
#ifndef IMAGER_IMMACROS_H_
#define IMAGER_IMMACROS_H_

/*
=item i_img_has_alpha(C<im>)

=category Image Information

Return true if the image has an alpha channel.

=cut
*/

#define i_img_has_alpha(im) (i_img_alpha_channel((im), NULL))

/*
=item i_psamp(im, left, right, y, samples, channels, channel_count)
=category Drawing

Writes sample values from C<samples> to C<im> for the horizontal line
(left, y) to (right-1, y) inclusive for the channels specified by
C<channels>, an array of C<int> with C<channel_count> elements.

If C<channels> is C<NULL> then the first C<channels_count> channels
are written to for each pixel.

Returns the number of samples written, which should be (right - left)
* channel_count.  If a channel not in the image is in channels, left
is negative, left is outside the image or y is outside the image,
returns -1 and pushes an error.

=cut
*/

#define i_psamp(im, l, r, y, samps, chans, count) \
  (((im)->i_f_psamp)((im), (l), (r), (y), (samps), (chans), (count)))

/*
=item i_psampf(im, left, right, y, samples, channels, channel_count)
=category Drawing

Writes floating point sample values from C<samples> to C<im> for the
horizontal line (left, y) to (right-1, y) inclusive for the channels
specified by C<channels>, an array of C<int> with C<channel_count>
elements.

If C<channels> is C<NULL> then the first C<channels_count> channels
are written to for each pixel.

Returns the number of samples written, which should be (right - left)
* channel_count.  If a channel not in the image is in channels, left
is negative, left is outside the image or y is outside the image,
returns -1 and pushes an error.

=cut
*/

#define i_psampf(im, l, r, y, samps, chans, count) \
  (((im)->i_f_psampf)((im), (l), (r), (y), (samps), (chans), (count)))

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

#endif

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

#define pIMCTX im_context_t my_im_ctx

#ifdef IMAGER_NO_CONTEXT
#define dIMCTXctx(ctx) pIMCTX = (ctx)
#define dIMCTX dIMCTXctx(im_get_context())
#define dIMCTXim(im) dIMCTXctx((im)->context)
#define dIMCTXio(io) dIMCTXctx((io)->context)
#define aIMCTX my_im_ctx
#else
#define aIMCTX im_get_context()
#endif

#define i_img_8_new(xsize, ysize, channels) im_img_8_new(aIMCTX, (xsize), (ysize), (channels))
#define i_img_16_new(xsize, ysize, channels) im_img_16_new(aIMCTX, (xsize), (ysize), (channels))
#define i_img_double_new(xsize, ysize, channels) im_img_double_new(aIMCTX, (xsize), (ysize), (channels))
#define i_img_pal_new(xsize, ysize, channels, maxpal) im_img_pal_new(aIMCTX, (xsize), (ysize), (channels), (maxpal))

#define i_img_alloc() im_img_alloc(aIMCTX)
#define i_img_init(im) im_img_init(aIMCTX, im)

#define i_set_image_file_limits(width, height, bytes) im_set_image_file_limits(aIMCTX, width, height, bytes)
#define i_get_image_file_limits(width, height, bytes) im_get_image_file_limits(aIMCTX, width, height, bytes)
#define i_int_check_image_file_limits(width, height, channels, sample_size) im_int_check_image_file_limits(aIMCTX, width, height, channels, sample_size)

#define i_clear_error() im_clear_error(aIMCTX)
#define i_push_errorvf(code, fmt, args) im_push_errorvf(aIMCTX, code, fmt, args)
#define i_push_error(code, msg) im_push_error(aIMCTX, code, msg)
#define i_errors() im_errors(aIMCTX)

#define io_new_fd(fd) im_io_new_fd(aIMCTX, (fd))
#define io_new_bufchain() im_io_new_bufchain(aIMCTX)
#define io_new_buffer(data, len, closecb, closectx) im_io_new_buffer(aIMCTX, (data), (len), (closecb), (closectx))
#define io_new_cb(p, readcb, writecb, seekcb, closecb, destroycb) \
  im_io_new_cb(aIMCTX, (p), (readcb), (writecb), (seekcb), (closecb), (destroycb))

#endif
