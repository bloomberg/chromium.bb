/* imexif.h - interface to Exif handling */
#ifndef IMAGER_IMEXIF_H
#define IMAGER_IMEXIF_H

#include <stddef.h>
#include "imdatatypes.h"

extern int im_decode_exif(i_img *im, const unsigned char *data, size_t length);

#endif /* ifndef IMAGER_IMEXIF_H */
