/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// PPAPI-based implementation of the interface for a scriptable object.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PPAPI_SCRIPTABLE_HANDLE_PPAPI_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PPAPI_SCRIPTABLE_HANDLE_PPAPI_H_

#include "native_client/src/trusted/plugin/scriptable_handle.h"
#include "ppapi/cpp/scriptable_object.h"

namespace plugin {

class PortableHandle;

// Encapsulates a browser scriptable object for a PPAPI plugin.
class ScriptableHandlePpapi : public pp::ScriptableObject,
                              public ScriptableHandle {
 public:
  // Factory method for creation.
  static ScriptableHandlePpapi* New(PortableHandle* handle);

  // TODO(polina): override pp::ScriptableObject methods.

  // Implement ScriptableHandle methods.

  // Add a browser reference to this object.
  virtual ScriptableHandle* AddRef();
  // Remove a browser reference to this object.
  // Delete the object when the ref count becomes 0.
  virtual void Unref();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ScriptableHandlePpapi);
  // Prevent construction and destruction from outside the class:
  // must use factory New() and base's Delete() methods instead.
  explicit ScriptableHandlePpapi(PortableHandle* handle);
  virtual ~ScriptableHandlePpapi();
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PPAPI_SCRIPTABLE_HANDLE_PPAPI_H_
