// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl-NPAPI Interface

#ifndef NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_POINTER_TRANSLATIONS_H_
#define NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_POINTER_TRANSLATIONS_H_

#include "third_party/npapi/bindings/npruntime.h"

namespace nacl {

// NPAPI uses a number of pointer types that need to be passed between
// browser and the NaCl module.

void WireFormatInit();
void WireFormatFini();

NPP WireFormatToNPP(int32_t wire_npp);
int32_t NPPToWireFormat(NPP npp);
#ifdef __native_client__
// NPP_New creates a local NPP that serves as the local copy of the browser NPP.
void SetLocalNPP(int32_t wire_npp, NPP npp);
#endif  // __native_client__

NPIdentifier WireFormatToNPIdentifier(int32_t wire_id);
int32_t NPIdentifierToWireFormat(NPIdentifier id);

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_POINTER_TRANSLATIONS_H_
