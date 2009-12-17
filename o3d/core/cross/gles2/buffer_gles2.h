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


// This file contains the declaration of the platform specific
// VertexBufferGLES2 and IndexBufferGLES2 objects used by O3D

#ifndef O3D_CORE_CROSS_GLES2_BUFFER_GLES2_H_
#define O3D_CORE_CROSS_GLES2_BUFFER_GLES2_H_

#include "core/cross/buffer.h"
#include "core/cross/gles2/gles2_headers.h"

namespace o3d {

class RendererGLES2;

// VertexBufferGLES2 is a wrapper around an OpenGLES2
// Vertex Buffer Object (VBO). The buffer starts out empty.  Calling Allocate()
// will reserve video memory for the buffer. Buffer contents are are updated by
// calling Lock() to get a pointer to the memory allocated for the buffer,
// updating that data in place and calling Unlock() to notify OpenGLES2 that the
// edits are done.
//
// To force the vertex and index buffers to be created by Cg Runtime
// control, define the compile flag "USE_CG_BUFFERS". This option is off by
// default and buffers are created, locked and managed using the OpenGLES2
// "ARB_vertex_buffer_object" extension.

class VertexBufferGLES2 : public VertexBuffer {
 public:
  explicit VertexBufferGLES2(ServiceLocator* service_locator);
  ~VertexBufferGLES2();

  // Returns the OpenGLES2 vertex buffer Object handle.
  GLuint gl_buffer() const { return gl_buffer_; }

 protected:
  // Creates a OpenGLES2 vertex buffer object of the specified size.
  virtual bool ConcreteAllocate(size_t size_in_bytes);

  // Frees the OpenGLES2 vertex buffer object.
  virtual void ConcreteFree();

  // Returns a pointer to the current contents of the buffer.  A matching
  // call to Unlock is necessary to update the contents of the buffer.
  virtual bool ConcreteLock(AccessMode access_mode, void** buffer_data);

  // Notifies OpenGLES2 that the buffer data has been updated.  Unlock is only
  // valid if it follows a Lock operation.
  virtual bool ConcreteUnlock();

 private:
  RendererGLES2* renderer_;
  GLuint gl_buffer_;
};

// IndexBufferGLES2 is a wrapper around an OpenGLES2 Index Buffer Object (VBO).
// The buffer starts out empty.  A call to Allocate() will create an OpenGLES2
// index buffer of the requested size.  Updates the to the contents of the
// buffer are done via the Lock/Unlock calls.
class IndexBufferGLES2 : public IndexBuffer {
 public:
  explicit IndexBufferGLES2(ServiceLocator* service_locator);
  ~IndexBufferGLES2();

  // Returns the OpenGLES2 vertex buffer Object handle.
  GLuint gl_buffer() const { return gl_buffer_; }

 protected:
  // Creates a OpenGLES2 index buffer of the specified size.
  virtual bool ConcreteAllocate(size_t size_in_bytes);

  // Frees the OpenGLES2 vertex buffer object.
  virtual void ConcreteFree();

  // Returns a pointer to the current contents of the buffer.  After calling
  // Lock, the contents of the buffer can be updated in place.
  virtual bool ConcreteLock(AccessMode access_mode, void** buffer_data);

  // Notifies OpenGLES2 that the buffer data has been updated.  Unlock is only
  // valid if it follows a Lock operation.
  virtual bool ConcreteUnlock();

 private:
  RendererGLES2* renderer_;
  GLuint gl_buffer_;
};

}  // namespace o3d


#endif  // O3D_CORE_CROSS_GLES2_BUFFER_GLES2_H_

