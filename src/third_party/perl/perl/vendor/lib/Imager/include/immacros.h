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

#define i_img_has_alpha(im) ((im)->channels == 2 || (im)->channels == 4)

/*
=item i_img_color_channels(C<im>)

=category Image Information

The number of channels holding color information.

=cut
*/

#define i_img_color_channels(im) (i_img_has_alpha(im) ? (im)->channels - 1 : (im)->channels)

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

#endif
