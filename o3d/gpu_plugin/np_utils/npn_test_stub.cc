// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include <set>
#include <string>

#include "o3d/gpu_plugin/np_utils/npn_funcs.h"
#include "o3d/gpu_plugin/np_utils/npn_test_stub.h"
#include "webkit/glue/plugins/nphostapi.h"

// Simple implementation of subset of the NPN functions for testing.

namespace o3d {
namespace gpu_plugin {

namespace {
std::set<std::string> names;

NPIdentifier GetStringIdentifier(const NPUTF8* name) {
  std::set<std::string>::iterator it = names.find(name);
  if (it == names.end()) {
    it = names.insert(name).first;
  }
  return const_cast<NPUTF8*>((*it).c_str());
}

void* MemAlloc(size_t size) {
  return malloc(size);
}

void MemFree(void* p) {
  free(p);
}

NPObject* CreateObject(NPP npp, NPClass* cl) {
  NPObject* object = cl->allocate(npp, cl);
  object->referenceCount = 1;
  object->_class = cl;
  return object;
}

NPObject* RetainObject(NPObject* object) {
  ++object->referenceCount;
  return object;
}

void ReleaseObject(NPObject* object) {
  --object->referenceCount;
  if (object->referenceCount == 0) {
    object->_class->deallocate(object);
  }
}

void ReleaseVariantValue(NPVariant* variant) {
  if (NPVARIANT_IS_STRING(*variant)) {
    NPN_MemFree(const_cast<NPUTF8*>(variant->value.stringValue.UTF8Characters));
  } else if (NPVARIANT_IS_OBJECT(*variant)) {
    gpu_plugin::NPN_ReleaseObject(NPVARIANT_TO_OBJECT(*variant));
  }
}

bool Invoke(NPP npp, NPObject* object, NPIdentifier name,
            const NPVariant* args, uint32_t num_args, NPVariant* result) {
  return object->_class->invoke(object, name, args, num_args, result);
}
}  // namespace anonymous

void InitializeNPNTestStub() {
  static NPNetscapeFuncs funcs = {
    sizeof(NPNetscapeFuncs),
    NP_VERSION_MINOR,
    NULL,  // geturl
    NULL,  // posturl
    NULL,  // requestread
    NULL,  // newstream
    NULL,  // write
    NULL,  // destroystream
    NULL,  // status
    NULL,  // uagent
    MemAlloc,
    MemFree,
    NULL,  // memflush
    NULL,  // reloadplugins
    NULL,  // getJavaEnv
    NULL,  // getJavaPeer
    NULL,  // geturlnotify
    NULL,  // posturlnotify
    NULL,  // getvalue
    NULL,  // setvalue
    NULL,  // invalidaterect
    NULL,  // invalidateregion
    NULL,  // forceredraw
    GetStringIdentifier,
    NULL,  // getstringidentifiers
    NULL,  // getintidentifier
    NULL,  // identifierisstring
    NULL,  // utf8fromidentifier
    NULL,  // intfromidentifier
    CreateObject,
    RetainObject,
    ReleaseObject,
    Invoke,
    NULL,  // invokeDefault
    NULL,  // evaluate
    NULL,  // getproperty
    NULL,  // setproperty
    NULL,  // removeproperty
    NULL,  // hasproperty
    NULL,  // hasmethod
    ReleaseVariantValue,
  };
  SetBrowserFuncs(&funcs);
}

void ShutdownNPNTestStub() {
  SetBrowserFuncs(NULL);
}

}  // namespace gpu_plugin
}  // namespace o3d
