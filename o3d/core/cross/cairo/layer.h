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
  struct Region {
    Region(bool everywhere, double x, double y, double width, double height)
        : everywhere(everywhere),
          x(x),
          y(y),
          width(width),
          height(height) {
    }

    bool everywhere;
    double x;
    double y;
    double width;
    double height;
  };

  typedef SmartPointer<Layer> Ref;

  enum PaintOperator {
    BLEND,
    BLEND_WITH_TRANSPARENCY,
    COPY,
    COPY_WITH_FADING,
  };

  virtual ~Layer();

  // Methods exposed to JS.

  Pattern* pattern() const {
    return pattern_;
  }

  void set_pattern(Pattern* pattern) {
    pattern_ = Pattern::Ref(pattern);
    set_content_dirty(true);
  }

  bool visible() const {
    return visible_;
  }

  void set_visible(bool visible) {
    visible_ = visible;
  }

  bool everywhere() const {
    return region_.everywhere;
  }

  void set_everywhere(bool everywhere) {
    region_.everywhere = everywhere;
    set_region_dirty(true);
  }

  double alpha() const {
    return alpha_;
  }

  void set_alpha(double alpha) {
    alpha_ = alpha;
    set_content_dirty(true);
  }

  double x() const {
    return region_.x;
  }

  void set_x(double x) {
    region_.x = x;
    set_region_dirty(true);
  }

  double y() const {
    return region_.y;
  }

  void set_y(double y) {
    region_.y = y;
    set_region_dirty(true);
  }

  double z() const {
    return z_;
  }

  void set_z(double z) {
    z_ = z;
    set_z_dirty(true);
    set_content_dirty(true);
  }

  double width() const {
    return region_.width;
  }

  void set_width(double width) {
    region_.width = width;
    set_region_dirty(true);
  }

  double height() const {
    return region_.height;
  }

  void set_height(double height) {
    region_.height = height;
    set_region_dirty(true);
  }

  double scale_x() const {
    return scale_x_;
  }

  void set_scale_x(double scale_x) {
    scale_x_ = scale_x;
    set_content_dirty(true);
  }

  double scale_y() const {
    return scale_y_;
  }

  void set_scale_y(double scale_y) {
    scale_y_ = scale_y;
    set_content_dirty(true);
  }

  PaintOperator paint_operator() const {
    return paint_operator_;
  }

  void set_paint_operator(PaintOperator paint_operator) {
    paint_operator_ = paint_operator;
    set_content_dirty(true);
  }

  // Methods not exposed to JS.

  bool z_dirty() const {
    return z_dirty_;
  }

  void set_z_dirty(bool z_dirty) {
    z_dirty_ = z_dirty;
  }

  bool region_dirty() const {
    return region_dirty_;
  }

  void set_region_dirty(bool region_dirty) {
    region_dirty_ = region_dirty;
  }

  bool content_dirty() const {
    return content_dirty_;
  }

  void set_content_dirty(bool content_dirty) {
    content_dirty_ = content_dirty;
  }

  const Region& region() const {
    return region_;
  }

  const Region& inner_clip_region() const {
    return inner_clip_region_;
  }

  const Region& outer_clip_region() const {
    return outer_clip_region_;
  }

  Region& inner_clip_region() {
    return inner_clip_region_;
  }

  Region& outer_clip_region() {
    return outer_clip_region_;
  }

  // Whether we should currently paint this layer.
  bool ShouldPaint() const {
    return visible() && pattern() != NULL;
  }

  // Whether this layer should currently clip content behind it (i.e.,
  // prevent it from being drawn in the first place).
  bool ShouldClip() const {
    // When alpha blending is used we cannot clip the background because our
    // content will be blended with it.
    return ShouldPaint() &&
        (paint_operator() == COPY || paint_operator() == COPY_WITH_FADING);
  }

  void SaveShouldPaint() {
    saved_should_paint_ = ShouldPaint();
  }

  bool GetSavedShouldPaint() const {
    return saved_should_paint_;
  }

  void SaveOuterClipRegion() {
    saved_outer_clip_region_ = outer_clip_region();
  }

  const Region& GetSavedOuterClipRegion() const {
    return saved_outer_clip_region_;
  }

 private:
  explicit Layer(ServiceLocator* service_locator);

  friend class o3d::IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  // The pattern used to paint this layer.
  Pattern::Ref pattern_;

  // Whether this layer should be visible or not.
  bool visible_;

  // The transparency for the BLEND_WITH_TRANSPARENCY operator or the fading for
  // the COPY_WITH_FADING operator.
  double alpha_;

  // The region of screen space occupied by this layer.
  Region region_;

  // A version of region_ that excludes any fractional pixels.
  Region inner_clip_region_;

  // A version of region_ that fully includes any fractional pixels.
  Region outer_clip_region_;

  // The z coordinate of the layer (used only to determine stacking order).
  double z_;

  // A scaling factor to apply to the pattern's x-axis.
  double scale_x_;

  // A scaling factor to apply to the pattern's y-axis.
  double scale_y_;

  // The paint operator to use for painting this layer.
  PaintOperator paint_operator_;

  // Whether or not this layer's z property has changed since the last frame was
  // rendered.
  bool z_dirty_;

  // Whether or not this layer's region properties have changed since the layer
  // was last updated on-screen.
  bool region_dirty_;

  // Whether or not this layer's content properties (pattern, alpha, z, scale_x,
  // scale_y, paint_operator) have changed since the layer was last updated
  // on-screen.
  bool content_dirty_;

  // The value of ShouldPaint() when the last frame was rendered.
  bool saved_should_paint_;

  // The value of outer_clip_region() when the layer was last updated on-screen.
  Region saved_outer_clip_region_;

  O3D_DECL_CLASS(Layer, ObjectBase)
};  // Layer

}  // namespace o2d

}  // namespace o3d

#endif  // O3D_CORE_CROSS_CAIRO_LAYER_H_
