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
 * NaCl file system mini XDR-like serialization of basic file system types.
 */
#include "native_client/src/shared/platform/nacl_log.h"

#include "native_client/src/trusted/service_runtime/include/machine/_types.h"
#include "native_client/src/trusted/service_runtime/fs/fs.h"
#include "native_client/src/trusted/service_runtime/fs/xdr.h"

/*
 * XDR externalization / internalization code for larger data types
 * for which the encoder/decoder should not be inlined.
 */

/* macro used in all externalization/internalization functions */
#define Ext(T, mem) do {                          \
    bump += nacl_ext_ ## T(buf + bump, dp->mem);  \
  } while (0)

#define Int(T, mem) do {                          \
    bump += nacl_int_ ## T(buf + bump, &dp->mem); \
  } while (0)

#define S(T, mem) do { bump += sizeof(T); } while (0)

#define G(Tag, Macro) \
  size_t NaCl ## Macro ## Tag(char *buf, struct Tag *dp) { \
    size_t bump = 0;                                       \
    Macro(dev_t, dev);                                     \
    Macro(ino_t, ino);                                     \
    Macro(mode_t, mode);                                   \
    Macro(uid_t, uid);                                     \
    Macro(gid_t, gid);                                     \
    Macro(nlink_t, nlink);                                 \
    Macro(off_t, size);                                    \
    Macro(time_t, atime);                                  \
    Macro(time_t, mtime);                                  \
    Macro(time_t, ctime);                                  \
                                                           \
    return bump;                                           \
  }

#define F(StructTag) G(StructTag, Ext) G(StructTag, Int)

F(NaClFileAttr)

#undef F
#undef G
