// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Satisfies linker dependencies for targets requiring an OpenMAX Core library.
// Not intended in any way to be functional!!

#include <OMX_Core.h>

#define NOTIMPLEMENTED_POLICY 3  // Fail at runtime via DCHECK.
#include "base/logging.h"

extern "C" {

OMX_API OMX_ERRORTYPE OMX_Init() {
  NOTIMPLEMENTED();
  return OMX_ErrorNotImplemented;
}

OMX_API OMX_ERRORTYPE OMX_Deinit() {
  NOTIMPLEMENTED();
  return OMX_ErrorNotImplemented;
}

OMX_API OMX_ERRORTYPE OMX_GetHandle(OMX_HANDLETYPE*, OMX_STRING, OMX_PTR,
                                    OMX_CALLBACKTYPE*) {
  NOTIMPLEMENTED();
  return OMX_ErrorNotImplemented;
}

OMX_API OMX_ERRORTYPE OMX_FreeHandle(OMX_HANDLETYPE) {
  NOTIMPLEMENTED();
  return OMX_ErrorNotImplemented;
}

OMX_API OMX_ERRORTYPE OMX_GetComponentsOfRole(OMX_STRING, OMX_U32*,
                                              OMX_U8**) {
  NOTIMPLEMENTED();
  return OMX_ErrorNotImplemented;
}

}  // extern "C"
