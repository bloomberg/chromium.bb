/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>

/*
 * Native Client Generic I/O backend using seekless operations on
 * POSIXesque file descriptors.  This is immune to whether the file
 * descriptor is shared among multiple processes that might be moving
 * its file offset independently.
 */

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/gio/gio.h"


static const struct GioVtbl kGioPioVtbl = {
  GioPioDtor,
  GioPioRead,
  GioPioWrite,
  GioPioSeek,
  GioPioFlush,
  GioPioClose,
};


int GioPioCtor(struct GioPio *self, int fd) {
  self->fd = fd;
  self->pos = 0;
  self->base.vtbl = &kGioPioVtbl;
  return 1;
}

#if NACL_WINDOWS
static HANDLE GioPioWinSetup(struct GioPio *self, OVERLAPPED *ov) {
  ov->Offset = self->pos;
  ov->OffsetHigh = 0;
  ov->hEvent = 0;
  return (HANDLE) _get_osfhandle(self->fd);
}
#endif

ssize_t GioPioRead(struct Gio *vself, void *buf, size_t count) {
  struct GioPio *self = (struct GioPio *) vself;
  ssize_t ret;

#if NACL_WINDOWS
  OVERLAPPED ov;
  DWORD nread;
  HANDLE fh = GioPioWinSetup(self, &ov);
  if (ReadFile(fh, buf, (DWORD) count, &nread, &ov)) {
    ret = nread;
  } else if (GetLastError() == ERROR_HANDLE_EOF) {
    ret = 0;
  } else {
    errno = EIO;
    ret = -1;
  }
#else
  ret = pread(self->fd, buf, count, self->pos);
#endif

  if (ret > 0)
    self->pos += (off_t) ret;

  return ret;
}


ssize_t GioPioWrite(struct Gio *vself, const void *buf, size_t count) {
  struct GioPio *self = (struct GioPio *) vself;
  ssize_t ret;

#if NACL_WINDOWS
  OVERLAPPED ov;
  DWORD nread;
  HANDLE fh = GioPioWinSetup(self, &ov);
  if (WriteFile(fh, buf, (DWORD) count, &nread, &ov)) {
    ret = nread;
  } else {
    errno = EIO;
    ret = -1;
  }
#else
  ret = pwrite(self->fd, buf, count, self->pos);
#endif

  if (ret > 0)
    self->pos += (off_t) ret;

  return ret;
}


off_t GioPioSeek(struct Gio *vself, off_t offset, int whence) {
  struct GioPio *self = (struct GioPio *) vself;

  switch (whence) {
    case SEEK_SET:
      break;
    case SEEK_CUR:
      offset += self->pos;
      break;
    case SEEK_END:
      offset = lseek(self->fd, offset, SEEK_END);
      if (offset < 0)
        return -1;
      break;
    default:
      errno = EINVAL;
      return -1;
  }

  if (offset < 0) {
    errno = EINVAL;
    return -1;
  }

  self->pos = offset;
  return offset;
}


int GioPioFlush(struct Gio *vself) {
  UNREFERENCED_PARAMETER(vself);
  return 0;
}


int GioPioClose(struct Gio *vself) {
  struct GioPio *self = (struct GioPio *) vself;

  if (CLOSE(self->fd) == 0) {
    self->fd = -1;
    return 0;
  }

  return -1;
}


void GioPioDtor(struct Gio *vself) {
  struct GioPio *self = (struct GioPio *) vself;
  if (-1 != self->fd) {
    (void) CLOSE(self->fd);
  }
}
