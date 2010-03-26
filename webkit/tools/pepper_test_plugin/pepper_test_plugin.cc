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
EXPORT NPError API_CALL NP_GetEntryPoints(NPPluginFuncs* pFuncs) {
  return NPAPIClient::PluginClient::GetEntryPoints(pFuncs);
}

EXPORT NPError API_CALL NP_Initialize(NPNetscapeFuncs* pFuncs) {
  return NPAPIClient::PluginClient::Initialize(pFuncs);
}

EXPORT NPError API_CALL NP_Shutdown() {
  return NPAPIClient::PluginClient::Shutdown();
}
} // extern "C"
