// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_MOJO_SRC_MOJO_PUBLIC_C_GLES2_CHROMIUM_EXTENSION_H_
#define THIRD_PARTY_MOJO_SRC_MOJO_PUBLIC_C_GLES2_CHROMIUM_EXTENSION_H_

// Note: This header should be compilable as C.

#include <GLES2/gl2.h>
#include <stdint.h>

#include "third_party/mojo/src/mojo/public/c/gles2/gles2_export.h"
#include "third_party/mojo/src/mojo/public/c/gles2/gles2_types.h"
#include "third_party/mojo/src/mojo/public/c/system/types.h"

extern "C" typedef struct _ClientBuffer* ClientBuffer;

#ifdef __cplusplus
extern "C" {
#endif

#define VISIT_GL_CALL(Function, ReturnType, PARAMETERS, ARGUMENTS) \
  MOJO_GLES2_EXPORT ReturnType GL_APIENTRY gl##Function PARAMETERS;
#include "third_party/mojo/src/mojo/public/c/gles2/gles2_call_visitor_chromium_extension_autogen.h"
#undef VISIT_GL_CALL

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // THIRD_PARTY_MOJO_SRC_MOJO_PUBLIC_C_GLES2_CHROMIUM_EXTENSION_H_
