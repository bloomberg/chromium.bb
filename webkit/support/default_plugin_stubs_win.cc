// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These are stubs for default_plugin which isn't current build in
// a webkit only checkout.

#include "base/logging.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/nphostapi.h"

namespace default_plugin {

NPError API_CALL NP_GetEntryPoints(NPPluginFuncs* funcs) {
  NOTIMPLEMENTED();
  return NPERR_GENERIC_ERROR;
}

NPError API_CALL NP_Initialize(NPNetscapeFuncs* funcs) {
  NOTIMPLEMENTED();
  return NPERR_GENERIC_ERROR;
}

NPError API_CALL NP_Shutdown(void) {
  NOTIMPLEMENTED();
  return NPERR_GENERIC_ERROR;
}

}  // default_plugin
