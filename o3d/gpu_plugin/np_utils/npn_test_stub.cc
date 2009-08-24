// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include <set>
#include <string>

#include "o3d/gpu_plugin/np_utils/npn_test_stub.h"

// Simple implementation of subset of the NPN functions for testing.

namespace {
  std::set<std::string> names;
}

NPIdentifier NPN_GetStringIdentifier(const NPUTF8* name) {
  std::set<std::string>::iterator it = names.find(name);
  if (it == names.end()) {
    it = names.insert(name).first;
  }
  return const_cast<NPUTF8*>((*it).c_str());
}

void* NPN_MemAlloc(size_t size) {
  return malloc(size);
}

void NPN_MemFree(void* p) {
  free(p);
}

NPObject* NPN_CreateObject(NPP npp, NPClass* cl) {
  NPObject* object = cl->allocate(npp, cl);
  object->referenceCount = 1;
  object->_class = cl;
  return object;
}

NPObject* NPN_RetainObject(NPObject* object) {
  ++object->referenceCount;
  return object;
}

void NPN_ReleaseObject(NPObject* object) {
  --object->referenceCount;
  if (object->referenceCount == 0) {
    object->_class->deallocate(object);
  }
}

void NPN_ReleaseVariantValue(NPVariant* variant) {
  if (NPVARIANT_IS_STRING(*variant)) {
    NPN_MemFree(const_cast<NPUTF8*>(variant->value.stringValue.UTF8Characters));
  } else if (NPVARIANT_IS_OBJECT(*variant)) {
    NPN_ReleaseObject(NPVARIANT_TO_OBJECT(*variant));
  }
}

bool NPN_Invoke(NPP npp, NPObject* object, NPIdentifier name,
                const NPVariant* args, uint32_t num_args, NPVariant* result) {
  return object->_class->invoke(object, name, args, num_args, result);
}
