/* -*- c++ -*- */
/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_NONNACL_UTIL_SEL_LDR_LAUNCHER_ZYGOTE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_NONNACL_UTIL_SEL_LDR_LAUNCHER_ZYGOTE_H_

#include "native_client/src/include/build_config.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher_zygote_base.h"

#if NACL_LINUX || NACL_OSX
# include "native_client/src/trusted/nonnacl_util/posix/sel_ldr_launcher_zygote_posix.h"
namespace nacl {
typedef ZygotePosix Zygote;
}
#elif NACL_WINDOWS
# include "native_client/src/trusted/nonnacl_util/win/sel_ldr_launcher_zygote_win.h"
namespace nacl {
typedef ZygoteWin Zygote;
}
#else
# error "What OS?!?"
#endif

#endif
