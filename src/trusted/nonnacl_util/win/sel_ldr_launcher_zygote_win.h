/* -*- c++ -*- */
/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_NONNACL_UTIL_SEL_LDR_LAUNCHER_WIN_ZYGOTE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_NONNACL_UTIL_SEL_LDR_LAUNCHER_WIN_ZYGOTE_H_

#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher_zygote_base.h"

namespace nacl {
class ZygoteWin : public ZygoteBase {
 public:
  ZygoteWin() {}
  virtual ~ZygoteWin() {}
  virtual bool Init() { return true; }
};
}

#endif
