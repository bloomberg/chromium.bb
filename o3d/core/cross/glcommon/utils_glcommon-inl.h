/*
 * Copyright 2010, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef O3D_CORE_CROSS_GLCOMMON_UTILS_GLCOMMON_INL_H_
#define O3D_CORE_CROSS_GLCOMMON_UTILS_GLCOMMON_INL_H_

#include "core/cross/types.h"

namespace o3d {

// Define this to debug GL errors. This has a significant performance hit.
// #define GL_ERROR_DEBUGGING

#ifdef GL_ERROR_DEBUGGING
#define CHECK_GL_ERROR() do {                                         \
  GLenum gl_error = glGetError();                                     \
  LOG_IF(ERROR, gl_error != GL_NO_ERROR) << "GL Error: " << gl_error; \
} while(0)
#define LOG_FLUSHED_GL_ERROR(gl_error) \
  LOG(ERROR) << "Flushing unreaped GL error: " << gl_error;
#else  // GL_ERROR_DEBUGGING
#define CHECK_GL_ERROR() void(0)
#define LOG_FLUSHED_GL_ERROR(gl_error) void(0)
#endif  // GL_ERROR_DEBUGGING

inline static void FlushGlErrors() {
  GLenum gl_error;
  while ((gl_error = glGetError()) != GL_NO_ERROR) {
    LOG_FLUSHED_GL_ERROR(gl_error);
  }
}

}  // namespace o3d

#endif  // O3D_CORE_CROSS_GLCOMMON_UTILS_GLCOMMON_INL_H_
