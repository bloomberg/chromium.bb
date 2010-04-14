// Copyright (c) 2009 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl module Pepper extensions interface.

#include "native_client/src/shared/npruntime/npextensions.h"

#include <stdlib.h>
#include <inttypes.h>
#include <nacl/nacl_inttypes.h>
#include <utility>

#include "native_client/src/shared/npruntime/device2d.h"
#include "native_client/src/shared/npruntime/device3d.h"
#include "native_client/src/shared/npruntime/audio.h"
#include "native_client/src/include/portability_string.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "third_party/npapi/bindings/npapi_extensions.h"

namespace {

NPDevice* AcquireDevice(NPP instance, NPDeviceID device) {
  switch (device) {
    case NPPepper2DDevice:
      return const_cast<NPDevice*>(nacl::GetDevice2D());
    case NPPepper3DDevice:
      return const_cast<NPDevice*>(nacl::GetDevice3D());
    case NPPepperAudioDevice:
      return const_cast<NPDevice*>(nacl::GetAudio());
    default:
      return NULL;
  }
}

}  // namespace

namespace nacl {

const struct NPNExtensions* GetNPNExtensions() {
  static const struct NPNExtensions kExtensions = {
    AcquireDevice
  };

  return &kExtensions;
}

}  // namespace nacl
