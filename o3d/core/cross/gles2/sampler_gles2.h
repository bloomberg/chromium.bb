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


// This file contains the class declaration for SamplerGLES2.

#ifndef O3D_CORE_CROSS_GLES2_SAMPLER_GLES2_H_
#define O3D_CORE_CROSS_GLES2_SAMPLER_GLES2_H_

#include "core/cross/sampler.h"

namespace o3d {

class RendererGLES2;

// TODO(gman): Replace.
typedef GLuint GLES2Parameter;

// SamplerGLES2 is an implementation of the Sampler object for GLES2.
class SamplerGLES2 : public Sampler {
 public:
  explicit SamplerGLES2(ServiceLocator* service_locator);
  virtual ~SamplerGLES2();

  // Sets the gl texture and sampler states.
  // Returns the handle to the texture.
  GLint SetTextureAndStates(GLES2Parameter gl_param);

  // Unbinds the GLES2 texture.
  void ResetTexture(GLES2Parameter gl_param);

 private:
  RendererGLES2* renderer_;

  // We compare this to RendererGLES2::GetTextureGroupSetCount to see
  // if we need to update our texture unit
  int texture_unit_group_set_count_;

  GLenum texture_unit_;

  DISALLOW_COPY_AND_ASSIGN(SamplerGLES2);
};
}  // namespace o3d


#endif  // O3D_CORE_CROSS_GLES2_SAMPLER_GLES2_H_

