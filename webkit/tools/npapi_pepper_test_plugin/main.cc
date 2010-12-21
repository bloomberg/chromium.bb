// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(__GNUC__) && __GNUC__ >= 4
#define EXPORT __attribute__((visibility ("default")))
#else
#define EXPORT
#endif

#include "webkit/glue/plugins/test/plugin_client.h"

extern "C" {
EXPORT NPError API_CALL NP_GetEntryPoints(NPPluginFuncs* nppFuncs) {
  return NPAPIClient::PluginClient::GetEntryPoints(nppFuncs);
}

EXPORT NPError API_CALL NP_Shutdown() {
  return NPAPIClient::PluginClient::Shutdown();
}

#if defined(OS_POSIX) && !defined(OS_MACOSX)
EXPORT NPError API_CALL NP_Initialize(NPNetscapeFuncs* npnFuncs,
                                      NPPluginFuncs* nppFuncs) {
  NPError error = NPAPIClient::PluginClient::Initialize(npnFuncs);
  if (error == NPERR_NO_ERROR) {
    error = NP_GetEntryPoints(nppFuncs);
  }
  return error;
}

EXPORT NPError API_CALL NP_GetValue(NPP instance, NPPVariable variable,
                                    void* value) {
  NPError err = NPERR_NO_ERROR;

  switch (variable) {
    case NPPVpluginNameString:
      *(static_cast<const char**>(value)) = "NPAPI Pepper Test Plugin";
      break;
    case NPPVpluginDescriptionString:
      *(static_cast<const char**>(value)) = "NPAPI Pepper Test Plugin";
      break;
    case NPPVpluginNeedsXEmbed:
      *(static_cast<NPBool*>(value)) = true;
      break;
    default:
      err = NPERR_GENERIC_ERROR;
      break;
  }

  return err;
}

EXPORT const char* API_CALL NP_GetMIMEDescription() {
  return "pepper-application/x-pepper-test:pepper:NPAPI Pepper Test Plugin";
}
#else  // OS_POSIX
EXPORT NPError API_CALL NP_Initialize(NPNetscapeFuncs* npnFuncs) {
  return NPAPIClient::PluginClient::Initialize(npnFuncs);
}
#endif  // OS_POSIX

} // extern "C"

