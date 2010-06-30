/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// PPAPI-based implementation of the interface to call browser functionality.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PPAPI_BROWSER_INTERFACE_PPAPI_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PPAPI_BROWSER_INTERFACE_PPAPI_H_

#include <string>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/plugin/srpc/browser_interface.h"

namespace plugin {

class PortableHandle;
class ScriptableHandle;


// Encapsulates an interface for communication with the browser
// from a PPAPI plugin.
class BrowserInterfacePpapi : public BrowserInterface {
 public:
  BrowserInterfacePpapi() {}
  virtual ~BrowserInterfacePpapi() {}

  // Convert a string to an identifier.
  virtual uintptr_t StringToIdentifier(const nacl::string& str);
  // Convert an identifier to a string.
  virtual nacl::string IdentifierToString(uintptr_t ident);

  // Pops up an alert box. Returns false if that failed for any reason.
  virtual bool Alert(InstanceIdentifier instance_id,
                     const nacl::string& text);

  // Evaluate a JavaScript string in the browser.
  virtual bool EvalString(InstanceIdentifier plugin_identifier,
                          const nacl::string& handler_string);

  // Gets the origin of the current page.
  virtual bool GetOrigin(InstanceIdentifier instance_id,
                         nacl::string* origin);

  // Creates a browser scriptable handle for a given portable handle.
  virtual ScriptableHandle* NewScriptableHandle(PortableHandle* handle);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(BrowserInterfacePpapi);
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PPAPI_BROWSER_INTERFACE_PPAPI_H_
