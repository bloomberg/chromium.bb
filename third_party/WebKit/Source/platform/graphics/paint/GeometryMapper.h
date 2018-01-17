// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GeometryMapper_h
#define GeometryMapper_h

#include "platform/graphics/paint/FloatClipRect.h"
#include "platform/graphics/paint/PropertyTreeState.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Optional.h"

namespace blink {

class TransformationMatrix;

// GeometryMapper is a helper class for fast computations of transformed and
// visual rects in different PropertyTreeStates. The design document has a
// number of details on use cases, algorithmic definitions, and running times.
//
// NOTE: A GeometryMapper object is only valid for property trees that do not
// change. If any mutation occurs, a new GeometryMapper object must be allocated
// corresponding to the new state.
//
// ** WARNING** Callers to the methods below may not assume that any const
// references returned remain const across multiple calls into GeometryMapper.
// If needed, callers must store local copies of the return values.
//
// Design document: http://bit.ly/28P4FDA
//
// TODO(chrishtr): take effect tree into account.
class PLATFORM_EXPORT GeometryMapper {
  STATIC_ONLY(GeometryMapper);

 public:
  // Returns the matrix that is suitable to map geometries on the source plane
  // to some backing in the destination plane.
  // Formal definition:
  //   output = flatten(destination_to_screen)^-1 * flatten(source_to_screen)
  // There are some cases that flatten(destination_to_screen) being
  // singular yet we can still define a reasonable projection, for example:
  // 1. Both nodes inherited a common singular flat ancestor:
  // 2. Both nodes are co-planar to a common singular ancestor:
  // Not every cases outlined above are supported!
  // Read implementation comments for specific restrictions.
  static const TransformationMatrix& SourceToDestinationProjection(
      const TransformPaintPropertyNode* source,
      const TransformPaintPropertyNode* destination);

  // Same as SourceToDestinationVisualRect() except that only transforms are
  // applied.
  //
  // |mapping_rect| is both input and output.
  static void SourceToDestinationRect(
      const TransformPaintPropertyNode* source_transform_node,
      const TransformPaintPropertyNode* destination_transform_node,
      FloatRect& mapping_rect);

  // Returns the "clip visual rect" between |local_state| and |ancestor_state|.
  // See above for the definition of "clip visual rect".
  //
  // The output FloatClipRect may contain false positives for rounded-ness
  // if a rounded clip is clipped out, and overly conservative results
  // in the presences of transforms.
  static FloatClipRect LocalToAncestorClipRect(
      const PropertyTreeState& local_state,
      const PropertyTreeState& ancestor_state,
      OverlayScrollbarClipBehavior = kIgnorePlatformOverlayScrollbarSize);

  // Maps from a rect in |local_state| to its visual rect in |ancestor_state|.
  // This is computed by multiplying the rect by its combined transform between
  // |local_state| and |ancestor_space|, then flattening into 2D space, then
  // intersecting by the "clip visual rect" for |local_state|'s clips. See above
  // for the definition of "clip visual rect".
  //
  // Note that the clip of |ancestor_state| is *not* applied.
  //
  // DCHECK fails if any of the paint property tree nodes in |local_state| are
  // not equal to or a descendant of that in |ancestor_state|.
  //
  // |mapping_rect| is both input and output.
  //
  // The output FloatClipRect may contain false positives for rounded-ness
  // if a rounded clip is clipped out, and overly conservative results
  // in the presences of transforms.
  static void LocalToAncestorVisualRect(
      const PropertyTreeState& local_state,
      const PropertyTreeState& ancestor_state,
      FloatClipRect& mapping_rect,
      OverlayScrollbarClipBehavior = kIgnorePlatformOverlayScrollbarSize);

  static void ClearCache();

 private:
  // The internal methods do the same things as their public counterparts, but
  // take an extra |success| parameter which indicates if the function is
  // successful on return. See comments of the public functions for failure
  // conditions.

  static const TransformationMatrix& SourceToDestinationProjectionInternal(
      const TransformPaintPropertyNode* source,
      const TransformPaintPropertyNode* destination,
      bool& success);

  static FloatClipRect LocalToAncestorClipRectInternal(
      const ClipPaintPropertyNode* descendant,
      const ClipPaintPropertyNode* ancestor_clip,
      const TransformPaintPropertyNode* ancestor_transform,
      OverlayScrollbarClipBehavior,
      bool& success);

  static void LocalToAncestorVisualRectInternal(
      const PropertyTreeState& local_state,
      const PropertyTreeState& ancestor_state,
      FloatClipRect& mapping_rect,
      OverlayScrollbarClipBehavior,
      bool& success);

  static void SlowLocalToAncestorVisualRectWithEffects(
      const PropertyTreeState& local_state,
      const PropertyTreeState& ancestor_state,
      FloatClipRect& mapping_rect,
      OverlayScrollbarClipBehavior,
      bool& success);

  friend class GeometryMapperTest;
  friend class PaintLayerClipperTest;
};

}  // namespace blink

#endif  // GeometryMapper_h
