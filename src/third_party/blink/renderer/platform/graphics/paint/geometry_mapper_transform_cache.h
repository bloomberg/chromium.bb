// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_GEOMETRY_MAPPER_TRANSFORM_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_GEOMETRY_MAPPER_TRANSFORM_CACHE_H_

#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

class TransformPaintPropertyNode;

// A GeometryMapperTransformCache hangs off a TransformPaintPropertyNode.
// It stores useful intermediate results such as screen matrix for geometry
// queries.
class PLATFORM_EXPORT GeometryMapperTransformCache {
  USING_FAST_MALLOC(GeometryMapperTransformCache);
 public:
  GeometryMapperTransformCache() = default;
  GeometryMapperTransformCache(const GeometryMapperTransformCache&) = delete;
  GeometryMapperTransformCache& operator=(const GeometryMapperTransformCache&) =
      delete;

  static void ClearCache();
  bool IsValid() const;

  void UpdateIfNeeded(const TransformPaintPropertyNode& node) {
    if (cache_generation_ != s_global_generation)
      Update(node);
    DCHECK_EQ(cache_generation_, s_global_generation);
  }

  const gfx::Vector2dF& to_2d_translation_root() const {
    return to_2d_translation_root_;
  }
  const TransformPaintPropertyNode* root_of_2d_translation() const {
    return root_of_2d_translation_;
  }

  // As screen transform data are used rarely, they are updated only when
  // needed. This method must be called before calling any screen transform
  // related getters.
  void UpdateScreenTransform(const TransformPaintPropertyNode&);

  // These getters must be called after UpdateScreenTransform() when screen
  // transform data is really needed.
  const TransformationMatrix& to_screen() const {
    DCHECK(screen_transform_);
    return screen_transform_->to_screen;
  }
  const TransformationMatrix& projection_from_screen() const {
    DCHECK(screen_transform_);
    return screen_transform_->projection_from_screen;
  }
  bool projection_from_screen_is_valid() const {
#if DCHECK_IS_ON()
    CheckScreenTransformUpdated();
#endif
    return LIKELY(!screen_transform_) ||
           screen_transform_->projection_from_screen_is_valid;
  }
  void ApplyToScreen(TransformationMatrix& m) const {
    if (UNLIKELY(screen_transform_))
      m.Multiply(to_screen());
    else
      ApplyToPlaneRoot(m);
  }
  void ApplyProjectionFromScreen(TransformationMatrix& m) const {
    if (UNLIKELY(screen_transform_))
      m.Multiply(projection_from_screen());
    else
      ApplyFromPlaneRoot(m);
  }
  bool has_animation_to_screen() const {
#if DCHECK_IS_ON()
    CheckScreenTransformUpdated();
#endif
    return UNLIKELY(screen_transform_) ? screen_transform_->has_animation
                                       : has_animation_to_plane_root();
  }

  const TransformationMatrix& to_plane_root() const {
    DCHECK(plane_root_transform_);
    return plane_root_transform_->to_plane_root;
  }
  const TransformationMatrix& from_plane_root() const {
    DCHECK(plane_root_transform_);
    return plane_root_transform_->from_plane_root;
  }
  void ApplyToPlaneRoot(TransformationMatrix& m) const {
    if (UNLIKELY(plane_root_transform_)) {
      m.Multiply(to_plane_root());
    } else {
      m.Translate(to_2d_translation_root_.x(), to_2d_translation_root_.y());
    }
  }
  void ApplyFromPlaneRoot(TransformationMatrix& m) const {
    if (UNLIKELY(plane_root_transform_)) {
      m.Multiply(from_plane_root());
    } else {
      m.Translate(-to_2d_translation_root_.x(), -to_2d_translation_root_.y());
    }
  }
  const TransformPaintPropertyNode* plane_root() const {
    return UNLIKELY(plane_root_transform_) ? plane_root_transform_->plane_root
                                           : root_of_2d_translation();
  }
  bool has_animation_to_plane_root() const {
    return UNLIKELY(plane_root_transform_) &&
           plane_root_transform_->has_animation;
  }

  bool has_fixed() const { return has_fixed_; }

 private:
  friend class GeometryMapperTransformCacheTest;

#if DCHECK_IS_ON()
  void CheckScreenTransformUpdated() const;
#endif

  void Update(const TransformPaintPropertyNode&);

  static unsigned s_global_generation;

  // The accumulated 2d translation to root_of_2d_translation().
  gfx::Vector2dF to_2d_translation_root_;

  // The parent of the root of consecutive identity or 2d translations from the
  // transform node, or the root of the tree if the whole path from the
  // transform node to the root contains identity or 2d translations only.
  const TransformPaintPropertyNode* root_of_2d_translation_;

  // The cached values here can be categorized in two logical groups:
  //
  // [ Screen Transform ]
  // to_screen : The screen matrix of the node, as defined by:
  //   to_screen = (flattens_inherited_transform ?
  //       flatten(parent.to_screen) : parent.to_screen) * local
  // projection_from_screen : Back projection from screen.
  //   projection_from_screen = flatten(to_screen) ^ -1
  //   Undefined if the inverse projection doesn't exist.
  //   Guaranteed to be flat.
  // projection_from_screen_is_valid : Whether projection_from_screen
  //   is defined.
  //
  // [ Plane Root Transform ]
  // plane_root : The oldest ancestor node such that every intermediate node
  //   in the ancestor chain has a flat and invertible local matrix. In other
  //   words, a node inherits its parent's plane_root if its local matrix is
  //   flat and invertible. Otherwise, it becomes its own plane root.
  //   For example:
  //   <xfrm id="A" matrix="rotateY(10deg)">
  //     <xfrm id="B" flatten_inherited matrix="translateX(10px)"/>
  //     <xfrm id="C" matrix="scaleX(0)">
  //       <xfrm id="D" matrix="scaleX(2)"/>
  //       <xfrm id="E" matrix="rotate(30deg)"/>
  //     </xfrm>
  //     <xfrm id="F" matrix="scaleZ(0)"/>
  //   </xfrm>
  //   A is the plane root of itself because its local matrix is 3D.
  //   B's plane root is A because its local matrix is flat.
  //   C is the plane root of itself because its local matrix is non-invertible.
  //   D and E's plane root is C because their local matrix is flat.
  //   F is the plane root of itself because its local matrix is 3D and
  //     non-invertible.
  // to_plane_root : The accumulated matrix between this node and plane_root.
  //   to_plane_root = (plane_root == this) ? I : parent.to_plane_root * local
  //   Guaranteed to be flat.
  // from_plane_root :
  //   from_plane_root = to_plane_root ^ -1
  //   Guaranteed to exist because each local matrices are invertible.
  //   Guaranteed to be flat.
  // An important invariant is that
  //   flatten(to_screen) = flatten(plane_root.to_screen) * to_plane_root
  //   Proof by induction:
  //   If plane_root == this,
  //     flatten(plane_root.to_screen) * to_plane_root = flatten(to_screen) * I
  //     = flatten(to_screen)
  //   Otherwise,
  //     flatten(to_screen) = flatten((flattens_inherited_transform ?
  //         flatten(parent.to_screen) : parent.to_screen) * local)
  //     Because local is known to be flat,
  //     = flatten((flattens_inherited_transform ?
  //         flatten(parent.to_screen) : parent.to_screen) * flatten(local))
  //     Then by flatten lemma (https://goo.gl/DNKyOc),
  //     = flatten(parent.to_screen) * local
  //     = flatten(parent.plane_root.to_screen) * parent.to_plane_root * local
  //     = flatten(plane_root.to_screen) * to_plane_root
  struct PlaneRootTransform {
    TransformationMatrix to_plane_root;
    TransformationMatrix from_plane_root;
    const TransformPaintPropertyNode* plane_root;
    bool has_animation;
  };
  std::unique_ptr<PlaneRootTransform> plane_root_transform_;

  struct ScreenTransform {
    TransformationMatrix to_screen;
    TransformationMatrix projection_from_screen;
    bool projection_from_screen_is_valid;
    bool has_animation;
  };
  std::unique_ptr<ScreenTransform> screen_transform_;

  // Whether or not there is a fixed position transform to the root.
  bool has_fixed_ = false;

  unsigned cache_generation_ = s_global_generation - 1;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_GEOMETRY_MAPPER_TRANSFORM_CACHE_H_
