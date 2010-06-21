/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// The API specific scriptable object class.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NPAPI_SCRIPTABLE_IMPL_NPAPI_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NPAPI_SCRIPTABLE_IMPL_NPAPI_H_

#include <stdio.h>
#include <string.h>

#include <set>

#include "native_client/src/include/checked_cast.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/trusted/plugin/api_defines.h"
#include "native_client/src/trusted/plugin/srpc/scriptable_handle.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"

namespace plugin {

// Forward declarations for externals.
class PortableHandle;

// ScriptableImplNpapi encapsulates objects that are scriptable from NPAPI.
class ScriptableImplNpapi: public NPObject, public plugin::ScriptableHandle {
 public:
  // Create a new ScriptableImplNpapi.
  static ScriptableImplNpapi* New(PortableHandle* handle);

  // Add a browser reference to this object.
  virtual ScriptableHandle* AddRef();
  // Remove a browser reference to this object.
  virtual void Unref();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ScriptableImplNpapi);
  explicit ScriptableImplNpapi(PortableHandle* handle);
  // We need an allocator "helper" to allow construction from the NPAPI
  // class table while making the constructor private.
  static NPObject* Allocate(NPP npp, NPClass* theClass);
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NPAPI_SCRIPTABLE_IMPL_NPAPI_H_
