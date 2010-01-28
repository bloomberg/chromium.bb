/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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
