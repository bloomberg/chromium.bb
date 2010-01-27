/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/* @file
 *
 * Effector subclass used only for service runtime's NaClAppDtor
 * address space tear down.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_EFFECTOR_CLEANUP_H_
#define NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_EFFECTOR_CLEANUP_H_

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/portability.h"

#include "native_client/src/trusted/desc/nacl_desc_effector.h"

EXTERN_C_BEGIN

/*
 * This effector is a degenerate case used only during shutdown, and
 * as such doesn't do much.
 */

struct NaClDescEffectorCleanup {
  struct NaClDescEffector base;
};

extern int NaClDescEffectorCleanupCtor(struct NaClDescEffectorCleanup *self);

EXTERN_C_END

#endif  // NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_EFFECTOR_CLEANUP_H_
