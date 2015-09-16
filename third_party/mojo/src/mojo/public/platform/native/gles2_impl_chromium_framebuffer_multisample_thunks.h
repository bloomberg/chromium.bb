// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_PLATFORM_NATIVE_GLES2_IMPL_CHROMIUM_FRAMEBUFFER_MULTISAMPLE_THUNKS_H_
#define MOJO_PUBLIC_PLATFORM_NATIVE_GLES2_IMPL_CHROMIUM_FRAMEBUFFER_MULTISAMPLE_THUNKS_H_

#include <stddef.h>

#include "mojo/public/c/gles2/chromium_framebuffer_multisample.h"

// Specifies the API for the GLES2 CHROMIUM_framebuffer_multisample
// extension.
#pragma pack(push, 8)
struct MojoGLES2ImplChromiumFramebufferMultisampleThunks {
  size_t size;  // Should be set to sizeof(*this).

#define VISIT_GL_CALL(Function, ReturnType, PARAMETERS, ARGUMENTS) \
  ReturnType(GL_APIENTRY* Function) PARAMETERS;
#include "mojo/public/c/gles2/gles2_call_visitor_chromium_framebuffer_multisample_autogen.h"
#undef VISIT_GL_CALL
};
#pragma pack(pop)

// Intended to be called from the embedder to get the embedder's implementation
// of GLES2.
inline MojoGLES2ImplChromiumFramebufferMultisampleThunks
MojoMakeGLES2ImplChromiumFramebufferMultisampleThunks() {
  MojoGLES2ImplChromiumFramebufferMultisampleThunks
      gles2_impl_chromium_framebuffer_multisample_thunks = {
          sizeof(MojoGLES2ImplChromiumFramebufferMultisampleThunks),
#define VISIT_GL_CALL(Function, ReturnType, PARAMETERS, ARGUMENTS) gl##Function,
#include "mojo/public/c/gles2/gles2_call_visitor_chromium_framebuffer_multisample_autogen.h"
#undef VISIT_GL_CALL
      };

  return gles2_impl_chromium_framebuffer_multisample_thunks;
}

// Use this type for the function found by dynamically discovering it in
// a DSO linked with mojo_system.
// The contents of |gles2_impl_chromium_framebuffer_multisample_thunks| are
// copied.
typedef size_t (*MojoSetGLES2ImplChromiumFramebufferMultisampleThunksFn)(
    const MojoGLES2ImplChromiumFramebufferMultisampleThunks* thunks);

#endif  // MOJO_PUBLIC_PLATFORM_NATIVE_GLES2_IMPL_CHROMIUM_FRAMEBUFFER_MULTISAMPLE_THUNKS_H_
