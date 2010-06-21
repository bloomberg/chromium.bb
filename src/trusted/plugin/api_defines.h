/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// Portable interface typedefs for browser interaction

// TODO(sehr): an ifdef for which (pepper v2 or npapi or ...) API to use.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_API_DEFINES_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_API_DEFINES_H_

// #include <stdio.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"

namespace plugin {

typedef NPP InstanceIdentifier;
typedef NPObject BrowserScriptableObject;

}  // plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_API_DEFINES_H_
