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

#include <stdlib.h>

#include "native_client/src/trusted/gio/gio.h"

int gvprintf(struct Gio *gp,
             char const *fmt,
             va_list    ap) {
  size_t  bufsz = 1024;
  char    *buf = malloc(bufsz);
  int     rv;

  if (!buf) return -1;

  while ((rv = vsnprintf(buf, bufsz, fmt, ap)) < 0 || (unsigned) rv >= bufsz) {
    free(buf);
    bufsz *= 2;
    buf = malloc(bufsz);
    if (!buf) {
      return -1;
    }
  }
  if (rv >= 0) {
    rv = (*gp->vtbl->Write)(gp, buf, rv);
  }
  free(buf);

  return rv;
}


int gprintf(struct Gio *gp,
            char const *fmt, ...) {
  va_list ap;
  int     rv;

  va_start(ap, fmt);
  rv = gvprintf(gp, fmt, ap);
  va_end(ap);

  return rv;
}
