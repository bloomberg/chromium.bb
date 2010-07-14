/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// Portable interface typedefs for browser interaction

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_API_DEFINES_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_API_DEFINES_H_

#include "native_client/src/include/portability.h"

namespace plugin {

// Each time a plugin instance is created, it is given a unique identifier.
// This value is never zero for a valid instance.
typedef int64_t InstanceIdentifier;

}  // plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_API_DEFINES_H_
