/* -*- c++ -*- */
/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_NONNACL_UTIL_SEL_LDR_LAUNCHER_ZYGOTE_BASE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_NONNACL_UTIL_SEL_LDR_LAUNCHER_ZYGOTE_BASE_H_

namespace nacl {
class ZygoteBase {
 public:
  ZygoteBase() {}
  virtual ~ZygoteBase() {}
  virtual bool Init() = 0;
};
}

#endif
