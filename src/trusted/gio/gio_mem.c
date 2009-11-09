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
 * NaCl Generic I/O interface implementation: memory buffer-based I/O.
 */
#include "native_client/src/include/portability.h"

#include <string.h>
#include <errno.h>

#include "native_client/src/trusted/gio/gio.h"

/*
 * Memory file is just read/write from/to an in-memory buffer.  Once
 * the buffer is consumed, there is no refilling/flushing.
 */

struct GioVtbl const    kGioMemoryFileVtbl = {
  GioMemoryFileRead,
  GioMemoryFileWrite,
  GioMemoryFileSeek,
  GioMemoryFileFlush,
  GioMemoryFileClose,
  GioMemoryFileDtor,
};


int GioMemoryFileCtor(struct GioMemoryFile  *self,
                      char                  *buffer,
                      size_t                len) {
  self->buffer = buffer;
  self->len = len;
  self->curpos = 0;

  self->base.vtbl = &kGioMemoryFileVtbl;
  return 1;
}


int GioMemoryFileRead(struct Gio  *vself,
                      void        *buf,
                      size_t      count) {
  struct GioMemoryFile    *self = (struct GioMemoryFile *) vself;
  size_t                  remain;
  size_t                  newpos;

  /* 0 <= self->curpos && self->curpos <= self->len */
  remain = self->len - self->curpos;
  /* 0 <= remain <= self->len */
  if (count > remain) {
    count = remain;
  }
  /* 0 <= count && count <= remain */
  if (0 == count) {
    return 0;
  }
  newpos = self->curpos + count;
  /* self->curpos <= newpos && newpos <= self->len */

  memcpy(buf, self->buffer + self->curpos, count);
  self->curpos = newpos;
  return count;
}


int GioMemoryFileWrite(struct Gio *vself,
                       void       *buf,
                       size_t     count) {
  struct GioMemoryFile  *self = (struct GioMemoryFile *) vself;
  size_t                remain;
  size_t                newpos;

  /* 0 <= self->curpos && self->curpos <= self->len */
  remain = self->len - self->curpos;
  /* 0 <= remain <= self->len */
  if (count > remain) {
    count = remain;
  }
  /* 0 <= count && count <= remain */
  if (0 == count) {
    return 0;
  }
  newpos = self->curpos + count;
  /* self->curpos <= newpos && newpos <= self->len */

  memcpy(self->buffer + self->curpos, buf, count);
  self->curpos = newpos;
  /* we never extend a memory file */
  return count;
}


off_t GioMemoryFileSeek(struct Gio  *vself,
                        off_t       offset,
                        int         whence) {
  struct GioMemoryFile  *self = (struct GioMemoryFile *) vself;
  size_t                new_pos;

  switch (whence) {
    case SEEK_SET:
      new_pos = offset;
      break;
    case SEEK_CUR:
      new_pos = self->curpos + offset;
      break;
    case SEEK_END:
      new_pos = self->len + offset;
      break;
    default:
      errno = EINVAL;
      return -1;
  }
  if (new_pos > self->len) {
    errno = EINVAL;
    return -1;
  }
  self->curpos = new_pos;
  return new_pos;
}


int GioMemoryFileClose(struct Gio *vself) {
  UNREFERENCED_PARAMETER(vself);
  return 0;
}


int GioMemoryFileFlush(struct Gio   *vself) {
  UNREFERENCED_PARAMETER(vself);
  return 0;
}


void  GioMemoryFileDtor(struct Gio    *vself) {
  UNREFERENCED_PARAMETER(vself);
  return;
}
