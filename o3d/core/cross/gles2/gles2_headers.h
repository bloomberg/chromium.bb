/*
 * Copyright 2009, Google Inc.
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

#ifndef O3D_CORE_CROSS_GLES2_GL_HEADERS_H_
#define O3D_CORE_CROSS_GLES2_GL_HEADERS_H_

#if defined(GLES2_BACKEND_DESKTOP_GL)

#include <GL/glew.h>
#if defined(OS_WIN)
#include <GL/wglew.h>
#elif defined(OS_LINUX)
#include <GL/glx.h>
#endif

// Use ARB_shader_object / ARB_vertex_shader functions to work when GL2 isn't
// present but the extensions are.
// Note that several GL2 functions are defined into the same ARB function (e.g.
// glDeleteProgram and glDeleteShader into glDeleteObjectARB). That's expected.
#undef glAttachShader
#define glAttachShader glAttachObjectARB
#undef glCompileShader
#define glCompileShader glCompileShaderARB
#undef glCreateProgram
#define glCreateProgram glCreateProgramObjectARB
#undef glCreateShader
#define glCreateShader glCreateShaderObjectARB
#undef glDeleteProgram
#define glDeleteProgram glDeleteObjectARB
#undef glDeleteShader
#define glDeleteShader glDeleteObjectARB
#undef glDisableVertexAttribArray
#define glDisableVertexAttribArray glDisableVertexAttribArrayARB
#undef glEnableVertexAttribArray
#define glEnableVertexAttribArray glEnableVertexAttribArrayARB
#undef glGetActiveAttrib
#define glGetActiveAttrib glGetActiveAttribARB
#undef glGetActiveUniform
#define glGetActiveUniform glGetActiveUniformARB
#undef glGetAttribLocation
#define glGetAttribLocation glGetAttribLocationARB
#undef glGetProgramInfoLog
#define glGetProgramInfoLog glGetInfoLogARB
#undef glGetProgramiv
#define glGetProgramiv glGetObjectParameterivARB
#undef glGetShaderInfoLog
#define glGetShaderInfoLog glGetInfoLogARB
#undef glGetShaderiv
#define glGetShaderiv glGetObjectParameterivARB
#undef glGetUniformLocation
#define glGetUniformLocation glGetUniformLocationARB
#undef glLinkProgram
#define glLinkProgram glLinkProgramARB
#undef glShaderSource
#define glShaderSource glShaderSourceARB
#undef glUniform1f
#define glUniform1f glUniform1fARB
#undef glUniform1fv
#define glUniform1fv glUniform1fvARB
#undef glUniform1i
#define glUniform1i glUniform1iARB
#undef glUniform1iv
#define glUniform1iv glUniform1ivARB
#undef glUniform2fv
#define glUniform2fv glUniform2fvARB
#undef glUniform3fv
#define glUniform3fv glUniform3fvARB
#undef glUniform4fv
#define glUniform4fv glUniform4fvARB
#undef glUniformMatrix4fv
#define glUniformMatrix4fv glUniformMatrix4fvARB
#undef glUseProgram
#define glUseProgram glUseProgramObjectARB
#undef glVertexAttribPointer
#define glVertexAttribPointer glVertexAttribPointerARB

#elif defined(GLES2_BACKEND_NATIVE_GLES2)

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#define glClearDepth glClearDepthf
#define glDepthRange glDepthRangef

// Buffer Objects
#define glBindBufferARB glBindBuffer
#define glBufferDataARB glBufferData
#define glBufferSubDataARB glBufferSubData
#define glDeleteBuffersARB glDeleteBuffers
#define glGenBuffersARB glGenBuffers

// Framebuffer Objects
#define glBindFramebufferEXT glBindFramebuffer
#define glBindRenderbufferEXT glBindRenderbuffer
#define glCheckFramebufferStatusEXT glCheckFramebufferStatus
#define glDeleteRenderbuffersEXT glDeleteRenderbuffers
#define glDeleteFramebuffersEXT glDeleteFramebuffers
#define glFramebufferRenderbufferEXT glFramebufferRenderbuffer
#define glFramebufferTexture2DEXT glFramebufferTexture2D
#define glGenFramebuffersEXT glGenFramebuffers
#define glGenRenderbuffersEXT glGenRenderbuffers
#define glRenderbufferStorageEXT glRenderbufferStorage

#define GLEW_VERSION_2_0 true
#define GLEW_VERSION_1_4 true

// TODO(piman): handle gracefully the case where GL_EXT_bgra isn't present.
#define GL_BGRA 0x80E1

// TODO(piman): handle gracefully the case where GL_OES_half_float isn't
// present.
#define GL_HALF_FLOAT_ARB GL_HALF_FLOAT_OES

#elif defined(GLES2_BACKEND_GLES2_COMMAND_BUFFERS)

#include <GLES2/gl2.h>

#else  // GLES2_BACKEND_xxx not defined

#error "GLES2_BACKEND_xxx not defined"

#endif  // GLES2_BACKEND_xxx

#endif  // O3D_CORE_CROSS_GLES2_GL_HEADERS_H_
