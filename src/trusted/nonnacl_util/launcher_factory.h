/* -*- c++ -*- */
/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_NONNACL_UTIL_LAUNCHER_FACTORY_H_
#define NATIVE_CLIENT_SRC_TRUSTED_NONNACL_UTIL_LAUNCHER_FACTORY_H_

#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"

namespace nacl {

class ZygoteBase;

class SelLdrLauncherStandaloneFactory {
 public:
  // Caller must ensure that zygote's lifetime is at least that of
  // this factory.  Thread-safety of |zygote_interface| usage is the
  // responsibility of the interface implementation -- the
  // SelLdrLauncherStandaloneFactory object's
  // MakeSelLdrLauncherStandalone factory function is re-entrant and
  // safe for use by multiple threads, but once the
  // SelLdrLauncherStandalone objects has been constructed, the
  // launcher objects may use the zygote_interface simultaneously from
  // different threads.
  explicit SelLdrLauncherStandaloneFactory(ZygoteBase* zygote_interface);
  virtual ~SelLdrLauncherStandaloneFactory();

  // The factory's lifetime must be at least that of all manufactured
  // SelLdrLauncherStandalone objects created by the factory.
  virtual SelLdrLauncherStandalone* MakeSelLdrLauncherStandalone();

 private:
  ZygoteBase* zygote_;

  friend class SelLdrLauncherStandaloneProxy;
};

}  // namespace nacl
#endif
