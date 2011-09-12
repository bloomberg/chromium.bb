/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/ppruntime.h"

// PPAPI plugins are actually "hosted" by ppruntime.  This is because the
// library needs to start an SRPC loop to dispatch to the stubs.
//
// This definition is weak to allow customers to override it when
// initialization is needed before the main PPAPI processing happens.

int __attribute__ ((weak)) main(int argc, char* argv[]) {
  return PpapiPluginMain();
}
