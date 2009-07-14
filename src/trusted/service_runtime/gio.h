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
 * NaCl Generic I/O interface.
 */

#ifndef SERVICE_RUNTIME_GIO_H__
#define SERVICE_RUNTIME_GIO_H__ 1

/* is this needed? - maybe for size_t */
#include "native_client/src/include/portability.h"

#include <stdarg.h>
#include <stdio.h>

EXTERN_C_BEGIN

struct Gio;  /* fwd */

struct GioVtbl {
  int   (*Read)(struct Gio  *vself,
                void        *buf,
                size_t      count);  /* returns nbytes read */
  int   (*Write)(struct Gio *vself,
                 void       *buf,
                 size_t     count);  /* returns nbytes written */
  off_t (*Seek)(struct Gio  *vself,
                off_t       offset,
                int         whence);  /* returns -1 on error */
  int   (*Flush)(struct Gio *vself);  /* only used for write */
  int   (*Close)(struct Gio *vself);  /* returns -1 on error */
  void  (*Dtor)(struct Gio  *vself);  /* implicit close */
};

struct Gio {
  struct GioVtbl const    *vtbl;
};

struct GioFile {
  struct Gio  base;
  FILE        *iop;
};

int GioFileCtor(struct GioFile  *self,
                char const      *fname,
                char const      *mode);

int GioFileRead(struct Gio  *vself,
                void        *buf,
                size_t      count);

int GioFileWrite(struct Gio *vself,
                 void       *buf,
                 size_t     count);

off_t GioFileSeek(struct Gio  *vself,
                  off_t       offset,
                  int         whence);

int GioFileFlush(struct Gio *vself);

int GioFileClose(struct Gio *vself);

void  GioFileDtor(struct Gio  *vself);

int GioFileRefCtor(struct GioFile *self,
                   FILE           *iop);

struct GioMemoryFile {
  struct Gio  base;
  char        *buffer;
  size_t      len;
  size_t      curpos;
  /* invariant: 0 <= curpos && curpos <= len */
  /* if curpos == len, everything has been read */
};

int GioMemoryFileCtor(struct GioMemoryFile  *self,
                      char                  *buffer,
                      size_t                len);

int GioMemoryFileRead(struct Gio  *vself,
                      void        *buf,
                      size_t      count);

int GioMemoryFileWrite(struct Gio *vself,
                       void       *buf,
                       size_t     count);

off_t GioMemoryFileSeek(struct Gio  *vself,
                        off_t       offset,
                        int         whence);

int GioMemoryFileFlush(struct Gio *vself);

int GioMemoryFileClose(struct Gio *vself);

void  GioMemoryFileDtor(struct Gio  *vself);

struct GioMemoryFileSnapshot {
  struct GioMemoryFile  base;
};

int GioMemoryFileSnapshotCtor(struct GioMemoryFileSnapshot  *self,
                              char                          *fn);

void  GioMemoryFileSnapshotDtor(struct Gio                    *vself);

#define ggetc(gp) ({ char ch; (*gp->vtbl->Read)(gp, &ch, 1) == 1 ? ch : EOF;})

int fggetc(struct Gio   *gp);

int gprintf(struct Gio  *gp,
            char const  *fmt,
            ...);

int gvprintf(struct Gio *gp,
             char const *fmt,
             va_list    ap);

EXTERN_C_END

#endif
