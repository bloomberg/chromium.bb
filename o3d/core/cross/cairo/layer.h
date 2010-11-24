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

// A Layer is a rectangular region of the O2D canvas to be filled with a
// particular Pattern, with automatic clipping based on stacking order.

#ifndef O3D_CORE_CROSS_CAIRO_LAYER_H_
#define O3D_CORE_CROSS_CAIRO_LAYER_H_

#include <vector>

#include "core/cross/object_base.h"
#include "core/cross/cairo/pattern.h"

namespace o3d {

class IClassManager;

namespace o2d {

class Layer : public ObjectBase {
 public:
  typedef SmartPointer<Layer> Ref;

  Pattern* pattern() const {
    return pattern_;
  }

  void set_pattern(Pattern* pattern) {
    pattern_ = Pattern::Ref(pattern);
  }

  double alpha() const {
    return alpha_;
  }

  void set_alpha(double alpha) {
    alpha_ = alpha;
  }

  double x() const {
    return x_;
  }

  void set_x(double x) {
    x_ = x;
  }

  double y() const {
    return y_;
  }

  void set_y(double y) {
    y_ = y;
  }

  double width() const {
    return width_;
  }

  void set_width(double width) {
    width_ = width;
  }

  double height() const {
    return height_;
  }

  void set_height(double height) {
    height_ = height;
  }

  double scale_x() const {
    return scale_x_;
  }

  void set_scale_x(double scale_x) {
    scale_x_ = scale_x;
  }

  double scale_y() const {
    return scale_y_;
  }

  void set_scale_y(double scale_y) {
    scale_y_ = scale_y;
  }

 private:
  explicit Layer(ServiceLocator* service_locator);

  friend class o3d::IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  // The pattern used to paint this Layer.
  Pattern::Ref pattern_;

  // Transparancy of this layer.
  double alpha_;

  // The x coordinate of the top-left corner of this layer.
  double x_;

  // The y coordinate of the top-left corner of this layer.
  double y_;

  // The width of this layer.
  double width_;

  // The height of this layer.
  double height_;

  // A scaling factor to apply to the pattern's x-axis.
  double scale_x_;

  // A scaling factor to apply to the pattern's y-axis.
  double scale_y_;

  O3D_DECL_CLASS(Layer, ObjectBase)
};  // Layer

}  // namespace o2d

}  // namespace o3d

#endif  // O3D_CORE_CROSS_CAIRO_LAYER_H_
