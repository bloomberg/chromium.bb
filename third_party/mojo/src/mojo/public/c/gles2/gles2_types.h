// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_MOJO_SRC_MOJO_PUBLIC_C_GLES2_GLES2_TYPES_H_
#define THIRD_PARTY_MOJO_SRC_MOJO_PUBLIC_C_GLES2_GLES2_TYPES_H_

// Note: This header should be compilable as C.

#include <stdint.h>

#include "third_party/mojo/src/mojo/public/c/gles2/gles2_export.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MojoGLES2ContextPrivate* MojoGLES2Context;
typedef void (*MojoGLES2ContextLost)(void* closure);
typedef void (*MojoGLES2SignalSyncPointCallback)(void* closure);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // THIRD_PARTY_MOJO_SRC_MOJO_PUBLIC_C_GLES2_GLES2_TYPES_H_
