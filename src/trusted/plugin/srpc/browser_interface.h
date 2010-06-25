/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// Portable interface for browser interaction

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_BROWSER_INTERFACE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_BROWSER_INTERFACE_H_

#include <stdio.h>
#include <map>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/trusted/plugin/api_defines.h"

namespace nacl {

class NPModule;

}  // namespace

namespace plugin {

class ScriptableHandle;
class PortableHandle;

// BrowserInterface represents the interface to the browser from
// the plugin, independent of whether it is the ActiveX or NPAPI instance.
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

  // Returns true iff |filename| appears to be a valid ELF file;
  // returns an informative error message otherwise.
  virtual bool MightBeElfExecutable(const nacl::string& filename,
                                    nacl::string* error) = 0;

  // Returns true iff the first |size| bytes of |buffer| appear to be
  // a valid ELF file; returns an informative error message otherwise.
  virtual bool MightBeElfExecutable(const char* buffer,
                                    size_t size,
                                    nacl::string* error) = 0;

  // Evaluate a JavaScript string in the browser.
  virtual bool EvalString(InstanceIdentifier plugin_identifier,
                          const nacl::string& str) = 0;

  // Gets the origin of the current page.
  virtual bool GetOrigin(InstanceIdentifier instance_id,
                         nacl::string* origin) = 0;

  // Creates a browser scriptable handle for a given portable handle.
  // If handle is NULL, returns NULL.
  virtual ScriptableHandle* NewScriptableHandle(PortableHandle* handle) = 0;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_BROWSER_INTERFACE_H_
