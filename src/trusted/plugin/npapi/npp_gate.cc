/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include "native_client/src/include/portability.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"

void NPP_Print(NPP instance, NPPrint* print_info) {
  UNREFERENCED_PARAMETER(instance);
  UNREFERENCED_PARAMETER(print_info);
}

NPError NPP_SetValue(NPP instance, NPNVariable variable, void* value) {
  UNREFERENCED_PARAMETER(instance);
  UNREFERENCED_PARAMETER(variable);
  UNREFERENCED_PARAMETER(value);
  return NPERR_GENERIC_ERROR;
}

#ifdef OJI
// Open JVM Integration support.
// NOTE: This feature is unsupported, and the best I can tell, dead anyway.
jref NPP_GetJavaClass(void) {
  return NULL;
}

#endif  // OJI
