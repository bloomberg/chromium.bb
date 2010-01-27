/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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
  size_t  (*Read)(struct Gio  *vself,
                  void        *buf,
                  size_t      count);  /* returns nbytes read */
  size_t  (*Write)(struct Gio *vself,
                   void       *buf,
                   size_t     count);  /* returns nbytes written */
  size_t  (*Seek)(struct Gio  *vself,
                  off_t       offset,
                  int         whence);  /* returns -1 on error */
  int     (*Flush)(struct Gio *vself);  /* only used for write */
  int     (*Close)(struct Gio *vself);  /* returns -1 on error */
  void    (*Dtor)(struct Gio  *vself);  /* implicit close */
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

size_t GioFileRead(struct Gio  *vself,
                   void        *buf,
                   size_t      count);

size_t GioFileWrite(struct Gio *vself,
                    void       *buf,
                    size_t     count);

size_t GioFileSeek(struct Gio  *vself,
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

size_t GioMemoryFileRead(struct Gio  *vself,
                         void        *buf,
                         size_t      count);

size_t GioMemoryFileWrite(struct Gio *vself,
                          void       *buf,
                          size_t     count);

size_t GioMemoryFileSeek(struct Gio  *vself,
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

size_t gprintf(struct Gio  *gp,
               char const  *fmt,
               ...);

size_t gvprintf(struct Gio *gp,
                char const *fmt,
                va_list    ap);

EXTERN_C_END

#endif
