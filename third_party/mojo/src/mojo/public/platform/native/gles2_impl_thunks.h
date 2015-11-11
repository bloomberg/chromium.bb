// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_MOJO_SRC_MOJO_PUBLIC_PLATFORM_NATIVE_GLES2_IMPL_THUNKS_H_
#define THIRD_PARTY_MOJO_SRC_MOJO_PUBLIC_PLATFORM_NATIVE_GLES2_IMPL_THUNKS_H_

#include <stddef.h>

#include "third_party/mojo/src/mojo/public/c/gles2/gles2.h"

// Like MojoGLES2ControlThunks, but specifies the frozen GLES2 API. Separated
// out as MojoGLES2ControlThunks may be modified and added to, but this
// interface is frozen.
#pragma pack(push, 8)
struct MojoGLES2ImplThunks {
  size_t size;  // Should be set to sizeof(MojoGLES2ImplThunks).

#define VISIT_GL_CALL(Function, ReturnType, PARAMETERS, ARGUMENTS) \
  ReturnType(GL_APIENTRY *Function) PARAMETERS;
#include "third_party/mojo/src/mojo/public/c/gles2/gles2_call_visitor_autogen.h"
#undef VISIT_GL_CALL
};
#pragma pack(pop)

// Intended to be called from the embedder to get the embedder's implementation
// of GLES2.
inline MojoGLES2ImplThunks MojoMakeGLES2ImplThunks() {
  MojoGLES2ImplThunks gles2_impl_thunks = {
      sizeof(MojoGLES2ImplThunks),
#define VISIT_GL_CALL(Function, ReturnType, PARAMETERS, ARGUMENTS) gl##Function,
#include "third_party/mojo/src/mojo/public/c/gles2/gles2_call_visitor_autogen.h"
#undef VISIT_GL_CALL
  };

  return gles2_impl_thunks;
}

// Use this type for the function found by dynamically discovering it in
// a DSO linked with mojo_system. For example:
// MojoSetGLES2ImplThunksFn mojo_set_gles2_impl_thunks_fn =
//     reinterpret_cast<MojoSetGLES2ImplThunksFn>(
//         app_library.GetFunctionPointer("MojoSetGLES2ImplThunks"));
// The expected size of |gles2_impl_thunks| is returned.
// The contents of |gles2_impl_thunks| are copied.
typedef size_t (*MojoSetGLES2ImplThunksFn)(
    const MojoGLES2ImplThunks* gles2_impl_thunks);

#endif  // THIRD_PARTY_MOJO_SRC_MOJO_PUBLIC_PLATFORM_NATIVE_GLES2_IMPL_THUNKS_H_
