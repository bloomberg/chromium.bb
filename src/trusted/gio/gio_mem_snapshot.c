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
 * NaCl Generic I/O interface implementation: in-memory snapshot of a file.
 */

#include "native_client/src/include/portability.h"
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>

#include "native_client/src/trusted/gio/gio.h"

struct GioVtbl const  kGioMemoryFileSnapshotVtbl = {
  GioMemoryFileRead,
  GioMemoryFileWrite,
  GioMemoryFileSeek,
  GioMemoryFileFlush,
  GioMemoryFileClose,
  GioMemoryFileSnapshotDtor,
};


int   GioMemoryFileSnapshotCtor(struct GioMemoryFileSnapshot  *self,
                                char                          *fn) {
  FILE            *iop;
  struct stat     stbuf;
  char            *buffer;

  ((struct Gio *) self)->vtbl = (struct GioVtbl *) NULL;
  if (0 == (iop = fopen(fn, "rb"))) {
    return 0;
  }
  if (fstat(fileno(iop), &stbuf) == -1) {
 abort0:
    fclose(iop);
    return 0;
  }
  if (0 == (buffer = malloc(stbuf.st_size))) {
    goto abort0;
  }
  if (fread(buffer, 1, stbuf.st_size, iop) != (size_t) stbuf.st_size) {
 abort1:
    free(buffer);
    goto abort0;
  }
  if (GioMemoryFileCtor(&self->base, buffer, stbuf.st_size) == 0) {
    goto abort1;
  }
  (void) fclose(iop);

  ((struct Gio *) self)->vtbl = &kGioMemoryFileSnapshotVtbl;
  return 1;
}


void GioMemoryFileSnapshotDtor(struct Gio                     *vself) {
  struct GioMemoryFileSnapshot  *self = (struct GioMemoryFileSnapshot *)
      vself;
  free(self->base.buffer);
  GioMemoryFileDtor(vself);
}
