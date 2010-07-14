/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// NPAPI implementation of the interface to call browser functionality.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NPAPI_BROWSER_IMPL_NPAPI_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NPAPI_BROWSER_IMPL_NPAPI_H_

#include <stdio.h>
#include <map>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/trusted/plugin/api_defines.h"
#include "native_client/src/trusted/plugin/srpc/browser_interface.h"

namespace nacl {

class NPModule;

}  // namespace

namespace plugin {

class BrowserImplNpapi : public BrowserInterface {
 public:
  BrowserImplNpapi() {}
  virtual ~BrowserImplNpapi() {}

  // Functions for communication with the browser.
  virtual uintptr_t StringToIdentifier(const nacl::string& str);
  // Convert an identifier to a string.
  virtual nacl::string IdentifierToString(uintptr_t ident);

  // Pops up an alert box.  Returns false if that failed for any reason.
  virtual bool Alert(InstanceIdentifier instance_id,
                     const nacl::string& text);

  // Evaluate a JavaScript string in the browser.
  virtual bool EvalString(InstanceIdentifier plugin_identifier,
                          const nacl::string& handler_string);

  // Gets the origin of current page.
  virtual bool GetOrigin(InstanceIdentifier instance_id,
                         nacl::string* origin);

  // Creates a browser scriptable handle for a given portable handle.
  virtual ScriptableHandle* NewScriptableHandle(PortableHandle* handle);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(BrowserImplNpapi);
};

// Convert from the API-independent instance identifier to the NPAPI NPP.
NPP InstanceIdentifierToNPP(InstanceIdentifier id);
// Convert from the NPAPI NPP type to the API-independent instance identifier.
InstanceIdentifier NPPToInstanceIdentifier(NPP npp);

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NPAPI_BROWSER_IMPL_NPAPI_H_
