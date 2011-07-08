/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


// Portable interface for browser interaction

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_BROWSER_INTERFACE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_BROWSER_INTERFACE_H_

#include <stdio.h>
#include <map>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/plugin/api_defines.h"

namespace plugin {

class ErrorInfo;
class ScriptableHandle;
class PortableHandle;

// BrowserInterface represents the interface to the browser from
// the plugin, independent of whether it is the PPAPI instance or not.
// I.e., when the plugin needs to request an alert, it uses these interfaces.
class BrowserInterface {
 public:
  virtual ~BrowserInterface() { }

  // Functions for communication with the browser.

  // Convert a string to an identifier.
  virtual uintptr_t StringToIdentifier(const nacl::string& str) = 0;
  // Convert an identifier to a string.
  virtual nacl::string IdentifierToString(uintptr_t ident) = 0;

  // Pops up an alert box.  Returns false if that failed for any reason.
  virtual bool Alert(InstanceIdentifier instance_id,
                     const nacl::string& text) = 0;

  // Evaluate a JavaScript string in the browser.
  virtual bool EvalString(InstanceIdentifier instance_id,
                          const nacl::string& str) = 0;

  // Gets the full URL of the current page.
  virtual bool GetFullURL(InstanceIdentifier instance_id,
                          nacl::string* full_url) = 0;

  // Write to the JavaScript console. Currently works in Chrome only, generates
  // an alert in other browsers.
  virtual bool AddToConsole(InstanceIdentifier instance_id,
                            const nacl::string& text) = 0;

  // Creates a browser scriptable handle for a given portable handle.
  // If handle is NULL, returns NULL.
  virtual ScriptableHandle* NewScriptableHandle(PortableHandle* handle) = 0;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_BROWSER_INTERFACE_H_
