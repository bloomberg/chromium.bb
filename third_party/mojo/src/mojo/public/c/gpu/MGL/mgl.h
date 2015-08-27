// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: This header should be compilable as C.

#ifndef MOJO_PUBLIC_C_GPU_MGL_MGL_H_
#define MOJO_PUBLIC_C_GPU_MGL_MGL_H_

#include <stdint.h>

#include "mojo/public/c/gpu/MGL/mgl_types.h"
#include "mojo/public/c/system/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t MGLOpenGLAPIVersion;

// OpenGL ES 2.0
#define MGL_API_VERSION_GLES2 ((MGLOpenGLAPIVersion)1)
// OpenGL ES 3.0
#define MGL_API_VERSION_GLES3 ((MGLOpenGLAPIVersion)2)
// OpenGL ES 3.1
#define MGL_API_VERSION_GLES31 ((MGLOpenGLAPIVersion)3)

#define MGL_NO_CONTEXT ((MGLContext)0)

struct MojoAsyncWaiter;

// Creates a context at the given API version or returns MGL_NO_CONTEXT.
// |command_buffer_handle| must be a command buffer message pipe handle from
// the Gpu service or another source. The callee takes ownership of this
// handle.
// |share_group| specifies the share group to create this context in.
// If this is MGL_NO_CONTEXT a new share group will be created for this context.
// |lost_callback|, if not null, will be invoked when the context is lost.
// |async_waiter| must be a pointer to a MojoAsyncWaiter implementation that is
// usable from any thread the returned MGLContext will be used from
// for as long as the context exists.
MGLContext MGLCreateContext(MGLOpenGLAPIVersion version,
                            MojoHandle command_buffer_handle,
                            MGLContext share_group,
                            MGLContextLostCallback lost_callback,
                            void* lost_callback_closure,
                            const struct MojoAsyncWaiter* async_waiter);
void MGLDestroyContext(MGLContext context);

// Makes |context| the current MGLContext for the calling thread. Calling with
// MGL_NO_CONTEXT clears the current context.
void MGLMakeCurrent(MGLContext context);

// Returns the currently bound context for the calling thread or MGL_NO_CONTEXT
// if there is none.
MGLContext MGLGetCurrentContext(void);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MOJO_PUBLIC_C_GPU_MGL_MGL_H_
