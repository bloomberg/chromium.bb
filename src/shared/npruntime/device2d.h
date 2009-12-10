// Copyright (c) 2009 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl module Pepper extensions interface, 2D graphics API

#ifndef NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_DEVICE2D_H_
#define NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_DEVICE2D_H_

#ifndef __native_client__
#error "This file is only for inclusion in Native Client modules"
#endif  // __native_client__

#include <nacl/npapi.h>
#include <nacl/npapi_extensions.h>

namespace nacl {

extern const struct NPDevice* GetDevice2D();

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_DEVICE2D_H_
