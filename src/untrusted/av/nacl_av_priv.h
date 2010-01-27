/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl low-level runtime library interfaces.
 */

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_AV_NACL_AV_PRIV_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_AV_NACL_AV_PRIV_H_

#define NACL_EVENT_RING_BUFFER_SIZE 64
#define NACL_VIDEO_SHARE_REVISION 0x101
#define NACL_VIDEO_SHARE_PIXEL_PAD 1024

/* do not ever change this size */
#define NACL_VIDEO_SHARE_HEADER_SIZE 16384

/* NaClVideoShare is a shared memory block existing between
 * untrusted NaCl code and the trusted SRPC plugin.
 */
struct NaClVideoShare {
  union {
    struct {
      /* misc */
      int                  revision;
      int                  map_size;                      /* rev 0x100 */
      /* event queue */
      volatile int         event_read_index;              /* rev 0x100 */
      volatile int         event_write_index;             /* rev 0x100 */
      volatile int         event_eof;                     /* rev 0x100 */
      union NaClMultimediaEvent                           /* rev 0x100 */
                           event_queue[NACL_EVENT_RING_BUFFER_SIZE];
      /* some misc event states (previously only used by plugin) */
      volatile int         reserved0;                     /* rev 0x100 */
      volatile int         reserved1;                     /* rev 0x100 */
      volatile uint16_t    reserved2;                     /* rev 0x100 */
      volatile uint16_t    reserved3;                     /* rev 0x100 */
      volatile int         reserved4;                     /* rev 0x100 */

      /* video backing store */
      volatile int         video_width;                   /* rev 0x100 */
      volatile int         video_height;                  /* rev 0x100 */
      volatile int         video_reserverd;               /* rev 0x100 */
      volatile int         video_size;                    /* rev 0x100 */
      /* --- revision 0x100 stops here.  Do not change the above. ---  */
      volatile int         video_ready;                   /* rev 0x101 */

      /*   later revisions requiring new fields should add them here   */
    } h;

    /* do not make above larger than NACL_VIDEO_SHARE_HEADER_SIZE */
    volatile char        header[NACL_VIDEO_SHARE_HEADER_SIZE];
  } u;
  uint8_t              video_pixels[NACL_VIDEO_SHARE_PIXEL_PAD];
  /* !!! do not place any additional structure members here */
  /* !!! this area is reserved for variable length video_pixels[...] */
};

#endif  /* NATIVE_CLIENT_SRC_UNTRUSTED_AV_NACL_AV_PRIV_H_ */
