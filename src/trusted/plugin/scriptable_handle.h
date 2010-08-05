/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// The browser scriptable container class.  The methods on this class
// are defined in the specific API directories.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SCRIPTABLE_HANDLE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SCRIPTABLE_HANDLE_H_

#include "native_client/src/trusted/plugin/scriptable_handle.h"

#include <stdio.h>
#include <string.h>

#include <set>

#include "native_client/src/include/checked_cast.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/plugin/utility.h"

namespace plugin {

// Forward declarations for externals.
class PortableHandle;

// ScriptableHandle encapsulates objects that are scriptable from the browser.
class ScriptableHandle {
 public:
  // Check that a pointer is to a validly created ScriptableHandle.
  static bool is_valid(const ScriptableHandle* handle);

  // Get the contained object.
  PortableHandle* handle() const { return handle_; }
  // Set the contained object.
  void set_handle(PortableHandle* handle) { handle_ = handle; }

  // Add a browser reference to this object.
  virtual ScriptableHandle* AddRef() = 0;
  // Remove a browser reference to this object.
  // Delete the object when the ref count becomes 0.
  virtual void Unref() = 0;

 protected:
  explicit ScriptableHandle(PortableHandle* handle);
  virtual ~ScriptableHandle();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ScriptableHandle);
  PortableHandle* handle_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SCRIPTABLE_HANDLE_H_
