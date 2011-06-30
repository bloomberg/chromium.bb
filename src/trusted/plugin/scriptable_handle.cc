/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// Scriptable handle implementation.

#include "native_client/src/trusted/plugin/scriptable_handle.h"

#include <stdio.h>
#include <string.h>

#include <set>

#include "native_client/src/include/checked_cast.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/plugin/browser_interface.h"
#include "native_client/src/trusted/plugin/utility.h"


namespace {

// For security we keep track of the set of scriptable handles that were
// created.

std::set<const plugin::ScriptableHandle*>* g_ValidHandles = 0;

}  // namespace

namespace plugin {

ScriptableHandle::ScriptableHandle(PortableHandle* handle) : handle_(handle) {
  PLUGIN_PRINTF(("ScriptableHandle::ScriptableHandle (this=%p, handle=%p)\n",
                 static_cast<void*>(this),
                 static_cast<void*>(handle)));
  // Initialize the set.
  // BUG: this is not thread safe.  We may leak sets, or worse, may not
  // correctly insert valid handles into the set.
  // TODO(sehr): use pthread_once or similar initialization.
  if (NULL == g_ValidHandles) {
    g_ValidHandles = new(std::nothrow) std::set<const ScriptableHandle*>;
    if (NULL == g_ValidHandles) {
      return;
    }
  }
  // Remember the scriptable handle.
  g_ValidHandles->insert(this);
}

ScriptableHandle::~ScriptableHandle() {
  PLUGIN_PRINTF(("ScriptableHandle::~ScriptableHandle (this=%p)\n",
                 static_cast<void*>(this)));
  // If the set was empty, just return.
  if (NULL == g_ValidHandles) {
    return;
  }
  // Remove the scriptable handle from the set of valid handles.
  g_ValidHandles->erase(this);
  // Handle deletion has been moved into derived classes.
  PLUGIN_PRINTF(("ScriptableHandle::~ScriptableHandle (this=%p, return)\n",
                  static_cast<void*>(this)));
}

// Check that an object is a validly created ScriptableHandle.
bool ScriptableHandle::is_valid(const ScriptableHandle* handle) {
  PLUGIN_PRINTF(("ScriptableHandle::is_valid (handle=%p)\n",
                 static_cast<void*>(const_cast<ScriptableHandle*>(handle))));
  if (NULL == g_ValidHandles) {
    PLUGIN_PRINTF(("ScriptableHandle::is_valid (return 0)\n"));
    return false;
  }
  size_t count =
      g_ValidHandles->count(static_cast<const ScriptableHandle*>(handle));
  PLUGIN_PRINTF(("ScriptableHandle::is_valid (handle=%p, count=%"
                 NACL_PRIuS")\n",
                 static_cast<void*>(const_cast<ScriptableHandle*>(handle)),
                 count));
  return 0 != count;
}

}  // namespace plugin
