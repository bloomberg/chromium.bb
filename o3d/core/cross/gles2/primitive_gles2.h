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


// This file contains the declaration of the PrimitiveGLES2 class.

#ifndef O3D_CORE_CROSS_GLES2_PRIMITIVE_GLES2_H_
#define O3D_CORE_CROSS_GLES2_PRIMITIVE_GLES2_H_

#include <map>
#include "core/cross/primitive.h"
#include "core/cross/gles2/param_cache_gles2.h"

namespace o3d {

// PrimitiveGLES2 is the OpenGLES2 implementation of the Primitive.  It provides
// the necessary interfaces for setting the geometry streams on the Primitive.
class PrimitiveGLES2 : public Primitive {
 public:
  explicit PrimitiveGLES2(ServiceLocator* service_locator);
  virtual ~PrimitiveGLES2();

 protected:
  // Overridden from Primitive.
  virtual void PlatformSpecificRender(Renderer* renderer,
                                      DrawElement* draw_element,
                                      Material* material,
                                      ParamObject* override,
                                      ParamCache* param_cache);

 private:
};
}  // o3d

#endif  // O3D_CORE_CROSS_GLES2_PRIMITIVE_GLES2_H_

