// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl module Pepper extensions interface, audio API

#ifndef NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_AUDIO_H_
#define NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_AUDIO_H_

#ifndef __native_client__
#error "This file is only for inclusion in Native Client modules"
#endif  // __native_client__

#include <stdint.h>
#include <inttypes.h>
#include <nacl/npapi.h>
#include <nacl/npapi_extensions.h>
#include "native_client/src/shared/npruntime/npclosure.h"

namespace nacl {

extern const struct NPDevice* GetAudio();

bool DoAudioCallback(nacl::NPClosureTable* closure_table,
                     int32_t number,
                     int shm_desc,
                     int32_t shm_size,
                     int sync_desc);
}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_AUDIO_H_
