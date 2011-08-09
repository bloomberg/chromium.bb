/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/srpc/nacl_srpc_ppapi_plugin_internal.h"

#include <stdio.h>

/*
 * this function is replaceable in tests to insert code before the
 * NaClPluginLowLevelInitializationCompleteInternal function is
 * invoked, e.g., to emulate accesses made by ld.so to open manifest
 * files.
 */
void NaClPluginLowLevelInitializationComplete(void) {
  NaClPluginLowLevelInitializationCompleteInternal();
}
