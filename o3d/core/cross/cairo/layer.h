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

// An alternative class of Transform for 2d Image Rendering Mode.

#ifndef O3D_CORE_CROSS_CAIRO_LAYER_H_
#define O3D_CORE_CROSS_CAIRO_LAYER_H_

#include <vector>
#include "core/cross/bounding_box.h"
#include "core/cross/param.h"
#include "core/cross/param_cache.h"
#include "core/cross/param_object.h"
#include "core/cross/types.h"
#include "core/cross/cairo/texture_cairo.h"

namespace o3d {

namespace o2d {

class Layer : public ParamObject {
  friend class Client;
 public:
  typedef WeakPointer<Layer> WeakPointerType;

  // Set the corresponding texture for this Layer instance.
  void SetTexture(Texture* texture) {
    texture_ = down_cast<TextureCairo*>(texture);
  }

  TextureCairo* GetTexture() {
    return texture_;
  }

  float GetAlpha() {
    return alpha_;
  }

  float GetScaleX() {
    return scale_x_;
  }

  float GetScaleY() {
    return scale_y_;
  }

  int GetTranslateX() {
    return translate_x_;
  }

  int GetTranslateY() {
    return translate_y_;
  }

  // Set the transparency of the Layer.
  void SetAlpha(float alpha) { alpha_ = alpha; }

  // Translate the given x,y from its origin.
  void Translate(float x, float y) {
    translate_x_ = x;
    translate_y_ = y;
  }

  // Scale the image to the given x,y.
  void Scale(float x, float y) {
    scale_x_ = x;
    scale_y_ = y;
  }

  // Gets a weak pointer to us.
  WeakPointerType GetWeakPointer() const {
    return weak_pointer_manager_.GetWeakPointer();
  }

 private:
  explicit Layer(ServiceLocator* service_locator);

  friend class o3d::IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  // Texture Container.
  TextureCairo* texture_;

  // Manager for weak pointers to us.
  WeakPointerType::WeakPointerManager weak_pointer_manager_;

  // Transparancy of the scene.
  float alpha_;

  // The end-x-size of which the current size needs to scale.
  float scale_x_;

  // The end-y-size of which the current size needs to scale.
  float scale_y_;

  // Size of x to translate.
  float translate_x_;

  // Size of y to translate.
  float translate_y_;

  O3D_DECL_CLASS(Layer, ParamObject)
};  // Layer

}  // namespace o2d

}  // namespace o3d

#endif  // O3D_CORE_CROSS_CAIRO_LAYER_H_
