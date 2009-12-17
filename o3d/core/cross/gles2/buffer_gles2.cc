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


// This file contains the implementatinos of VertexBufferGLES2 and
// IndexBufferGLES2, used to implement O3D using OpenGLES2.
//
// To force the vertex and index buffers to be created by Cg Runtime
// control, define the compile flag "USE_CG_BUFFERS". This option is off by
// default and buffers are created, locked and managed using the OpenGLES2
// "ARB_vertex_buffer_object" extension.

#include "core/cross/error.h"
#include "core/cross/gles2/buffer_gles2.h"
#include "core/cross/gles2/renderer_gles2.h"
#include "core/cross/gles2/utils_gles2.h"
#include "core/cross/gles2/utils_gles2-inl.h"

namespace o3d {

namespace {

GLenum BufferAccessModeToGLenum(Buffer::AccessMode access_mode) {
  switch (access_mode) {
    case Buffer::READ_ONLY:
      return GL_READ_ONLY_ARB;
    case Buffer::WRITE_ONLY:
      return GL_WRITE_ONLY_ARB;
    case Buffer::READ_WRITE:
      return GL_READ_WRITE_ARB;
    case Buffer::NONE:
      break;
  }
  DCHECK(false);
  return GL_READ_WRITE_ARB;
}

}  // anonymous namespace

// Vertex Buffers --------------------------------------------------------------

// Initializes the O3D VertexBuffer object but does not allocate an
// OpenGLES2 vertex buffer object yet.
VertexBufferGLES2::VertexBufferGLES2(ServiceLocator* service_locator)
    : VertexBuffer(service_locator),
      renderer_(static_cast<RendererGLES2*>(
          service_locator->GetService<Renderer>())),
      gl_buffer_(0) {
  DLOG(INFO) << "VertexBufferGLES2 Construct";
}

// Destructor releases the OpenGLES2 VBO.
VertexBufferGLES2::~VertexBufferGLES2() {
  DLOG(INFO) << "VertexBufferGLES2 Destruct \"" << name() << "\"";
  ConcreteFree();
}

// Creates a OpenGLES2 vertex buffer of the requested size.
bool VertexBufferGLES2::ConcreteAllocate(size_t size_in_bytes) {
  DLOG(INFO) << "VertexBufferGLES2 Allocate  \"" << name() << "\"";
  renderer_->MakeCurrentLazy();
  ConcreteFree();
  // Create a new VBO.
  glGenBuffersARB(1, &gl_buffer_);

  if (!gl_buffer_) return false;

  // Give the VBO a size, but no data, and set the hint to "STATIC_DRAW"
  // to mark the buffer as set up once then used often.
  glBindBufferARB(GL_ARRAY_BUFFER_ARB, gl_buffer_);
  glBufferDataARB(GL_ARRAY_BUFFER_ARB,
                  size_in_bytes,
                  NULL,
                  GL_STATIC_DRAW_ARB);
  CHECK_GL_ERROR();
  return true;
}

void VertexBufferGLES2::ConcreteFree() {
  if (gl_buffer_) {
    renderer_->MakeCurrentLazy();
    glDeleteBuffersARB(1, &gl_buffer_);
    gl_buffer_ = 0;
    CHECK_GL_ERROR();
  }
}

// Calls Lock on the OpenGLES2 buffer to get the address in memory of where the
// buffer data is currently stored.
bool VertexBufferGLES2::ConcreteLock(Buffer::AccessMode access_mode,
                                     void **buffer_data) {
  DLOG(INFO) << "VertexBufferGLES2 Lock  \"" << name() << "\"";
  renderer_->MakeCurrentLazy();
  glBindBufferARB(GL_ARRAY_BUFFER_ARB, gl_buffer_);
  *buffer_data = glMapBufferARB(GL_ARRAY_BUFFER_ARB,
                                BufferAccessModeToGLenum(access_mode));
  if (*buffer_data == NULL) {
    GLenum error = glGetError();
    if (error == GL_OUT_OF_MEMORY) {
      O3D_ERROR(service_locator()) << "Out of memory for buffer lock.";
    } else {
      O3D_ERROR(service_locator()) << "Unable to lock a GLES2 Array Buffer";
    }
    return false;
  }
  CHECK_GL_ERROR();
  return true;
}

// Calls Unlock on the OpenGLES2 buffer to notify that the contents of the
// buffer are now ready for use.
bool VertexBufferGLES2::ConcreteUnlock() {
  DLOG(INFO) << "VertexBufferGLES2 Unlock  \"" << name() << "\"";
  renderer_->MakeCurrentLazy();
  glBindBufferARB(GL_ARRAY_BUFFER_ARB, gl_buffer_);
  if (!glUnmapBufferARB(GL_ARRAY_BUFFER)) {
    GLenum error = glGetError();
    if (error == GL_INVALID_OPERATION) {
      O3D_ERROR(service_locator()) <<
          "Buffer was unlocked without first being locked.";
    } else {
      O3D_ERROR(
          service_locator()) << "Unable to unlock a GLES2 Element Array Buffer";
    }
    return false;
  }
  CHECK_GL_ERROR();
  return true;
}


// Index Buffers ---------------------------------------------------------------

// Initializes the O3D IndexBuffer object but does not create a OpenGLES2
// buffer yet.

IndexBufferGLES2::IndexBufferGLES2(ServiceLocator* service_locator)
    : IndexBuffer(service_locator),
      renderer_(static_cast<RendererGLES2*>(
          service_locator->GetService<Renderer>())),
      gl_buffer_(0) {
  DLOG(INFO) << "IndexBufferGLES2 Construct";
}

// Destructor releases the OpenGLES2 index buffer.
IndexBufferGLES2::~IndexBufferGLES2() {
  DLOG(INFO) << "IndexBufferGLES2 Destruct  \"" << name() << "\"";
  ConcreteFree();
}

// Creates a OpenGLES2 index buffer of the requested size.
bool IndexBufferGLES2::ConcreteAllocate(size_t size_in_bytes) {
  DLOG(INFO) << "IndexBufferGLES2 Allocate  \"" << name() << "\"";
  renderer_->MakeCurrentLazy();
  ConcreteFree();
  // Create a new VBO.
  glGenBuffersARB(1, &gl_buffer_);
  if (!gl_buffer_) return false;
  // Give the VBO a size, but no data, and set the hint to "STATIC_DRAW"
  // to mark the buffer as set up once then used often.
  glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, gl_buffer_);
  glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB,
                  size_in_bytes,
                  NULL,
                  GL_STATIC_DRAW_ARB);
  CHECK_GL_ERROR();
  return true;
}

void IndexBufferGLES2::ConcreteFree() {
  if (gl_buffer_) {
    renderer_->MakeCurrentLazy();
    glDeleteBuffersARB(1, &gl_buffer_);
    gl_buffer_ = 0;
    CHECK_GL_ERROR();
  }
}

// Maps the OpenGLES2 buffer to get the address in memory of the buffer data.
bool IndexBufferGLES2::ConcreteLock(Buffer::AccessMode access_mode,
                                    void **buffer_data) {
  DLOG(INFO) << "IndexBufferGLES2 Lock  \"" << name() << "\"";
  renderer_->MakeCurrentLazy();
  glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, gl_buffer_);
  if (!num_elements())
    return true;
  *buffer_data = glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,
                                BufferAccessModeToGLenum(access_mode));
  if (*buffer_data == NULL) {
    GLenum error = glGetError();
    if (error == GL_OUT_OF_MEMORY) {
      O3D_ERROR(service_locator()) << "Out of memory for buffer lock.";
    } else {
      O3D_ERROR(
          service_locator()) << "Unable to lock a GLES2 Element Array Buffer";
    }
    return false;
  }
  CHECK_GL_ERROR();
  return true;
}

// Calls Unlock on the OpenGLES2 buffer to notify that the contents of the
// buffer are now ready for use.
bool IndexBufferGLES2::ConcreteUnlock() {
  DLOG(INFO) << "IndexBufferGLES2 Unlock  \"" << name() << "\"";
  renderer_->MakeCurrentLazy();
  if (!num_elements())
    return true;
  glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, gl_buffer_);
  if (!glUnmapBufferARB(GL_ELEMENT_ARRAY_BUFFER)) {
    GLenum error = glGetError();
    if (error == GL_INVALID_OPERATION) {
      O3D_ERROR(service_locator()) <<
          "Buffer was unlocked without first being locked.";
    } else {
      O3D_ERROR(
          service_locator()) << "Unable to unlock a GLES2 Element Array Buffer";
    }
    return false;
  }
  CHECK_GL_ERROR();
  return true;
}
}  // namespace o3d

