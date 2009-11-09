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
#include "native_client/src/include/portability.h"

#include "native_client/src/trusted/gio/gio.h"

struct GioVtbl const    kGioFileVtbl = {
  GioFileRead,
  GioFileWrite,
  GioFileSeek,
  GioFileFlush,
  GioFileClose,
  GioFileDtor,
};


int GioFileCtor(struct GioFile  *self,
                char const      *fname,
                char const      *mode) {
  self->base.vtbl = (struct GioVtbl *) NULL;
  self->iop = fopen(fname, mode);
  if (NULL == self->iop) {
    return 0;
  }
  self->base.vtbl = &kGioFileVtbl;
  return 1;
}


int GioFileRefCtor(struct GioFile   *self,
                   FILE             *iop) {
  self->iop = iop;

  self->base.vtbl = &kGioFileVtbl;
  return 1;
}


int GioFileRead(struct Gio  *vself,
                void        *buf,
                size_t      count) {
  struct GioFile  *self = (struct GioFile *) vself;
  return fread(buf, 1, count, self->iop);
}


int GioFileWrite(struct Gio *vself,
                 void       *buf,
                 size_t     count) {
  struct GioFile  *self = (struct GioFile *) vself;
  return fwrite(buf, 1, count, self->iop);
}


off_t GioFileSeek(struct Gio  *vself,
                  off_t       offset,
                  int         whence) {
  struct GioFile  *self = (struct GioFile *) vself;
  int             ret;
  ret = fseek(self->iop, offset, whence);
  if (-1 == ret) return -1;
  return ftell(self->iop);
}


int GioFileFlush(struct Gio *vself) {
  struct GioFile  *self = (struct GioFile *) vself;

  return fflush(self->iop);
}


int GioFileClose(struct Gio *vself){
  struct GioFile  *self = (struct GioFile *) vself;
  int             rv;
  rv = (EOF == fclose(self->iop)) ? -1 : 0;
  if (0 == rv) {
    self->iop = (FILE *) 0;
  }
  return rv;
}


void  GioFileDtor(struct Gio  *vself) {
  struct GioFile  *self = (struct GioFile *) vself;
  if (0 != self->iop) {
    (void) fclose(self->iop);
  }
}


int fggetc(struct Gio   *gp) {
  char    ch;

  return (*gp->vtbl->Read)(gp, &ch, 1) == 1 ? ch : EOF;
}
