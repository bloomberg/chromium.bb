// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_NP_UTILS_NPN_TEST_STUB_H_
#define O3D_GPU_PLUGIN_NP_UTILS_NPN_TEST_STUB_H_

#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"

// Simple implementation of subset of the NPN functions for testing.

NPIdentifier NPN_GetStringIdentifier(const NPUTF8* name);
void* NPN_MemAlloc(size_t size);
void NPN_MemFree(void* p);
NPObject* NPN_CreateObject(NPP npp, NPClass* cl);
NPObject* NPN_RetainObject(NPObject* object);
void NPN_ReleaseObject(NPObject* object);
void NPN_ReleaseVariantValue(NPVariant* variant);
bool NPN_Invoke(NPP npp, NPObject* object, NPIdentifier name,
                const NPVariant* args, uint32_t num_args, NPVariant* result);

#endif  // O3D_GPU_PLUGIN_NP_UTILS_NPN_TEST_STUB_H_
