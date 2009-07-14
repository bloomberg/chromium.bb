/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
