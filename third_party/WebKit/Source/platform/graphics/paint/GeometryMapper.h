// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GeometryMapper_h
#define GeometryMapper_h

#include "platform/graphics/paint/FloatClipRect.h"
#include "platform/graphics/paint/PropertyTreeState.h"
#include "platform/transforms/TransformationMatrix.h"
#include "platform/wtf/HashMap.h"

namespace blink {

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
  // The runtime of m calls among localToAncestorVisualRect, localToAncestorRect
  // or ancestorToLocalRect with the same |ancestorState| parameter is
  // guaranteed to be O(n + m), where n is the number of transform and clip
  // nodes in their respective property trees.

  // If the clips and transforms of |sourceState| are equal to or descendants of
  // those of |destinationState|, returns the same value as
  // localToAncestorVisualRect. Otherwise, maps the input rect to the
  // transform state which is the lowest common ancestor of
  // |sourceState.transform| and |destinationState.transform|, then multiplies
  // it by the the inverse transform mapping from the lowest common ancestor to
  // |destinationState.transform|.
  //
  // DCHECK fails if the clip of |destinationState| is not an ancestor of the
  // clip of |sourceState|, or the inverse transform is not invertible.
  //
  // |mappingRect| is both input and output.
  //
  // The output FloatClipRect may contain false positives for rounded-ness
  // if a rounded clip is clipped out, and overly conservative results
  // in the presences of transforms.
  //
  // TODO(chrishtr): we should provide a variant of these methods that
  // guarantees a tight result, or else signals an error. crbug.com/708741
  static void SourceToDestinationVisualRect(
      const PropertyTreeState& source_state,
      const PropertyTreeState& destination_state,
      FloatClipRect& mapping_rect);

  // Same as sourceToDestinationVisualRect() except that only transforms are
  // applied.
  //
  // |mappingRect| is both input and output.
  static void SourceToDestinationRect(
      const TransformPaintPropertyNode* source_transform_node,
      const TransformPaintPropertyNode* destination_transform_node,
      FloatRect& mapping_rect);

  // Maps from a rect in |localTransformSpace| to its visual rect in
  // |ancestorState|. This is computed by multiplying the rect by its combined
  // transform between |localTransformSpace| and |ancestorSpace|, then
  // flattening into 2D space, then intersecting by the "clip visual rect" for
  // |localTransformState|'s clips. See above for the definition of "clip visual
  // rect".
  //
  // Note that the clip of |ancestorState| is *not* applied.
  //
  // DCHECK fails if any of the paint property tree nodes in
  // |localTransformState| are not equal to or a descendant of that in
  // |ancestorState|.
  //
  // |mappingRect| is both input and output.
  //
  // The output FloatClipRect may contain false positives for rounded-ness
  // if a rounded clip is clipped out, and overly conservative results
  // in the presences of transforms.
  static void LocalToAncestorVisualRect(
      const PropertyTreeState& local_transform_state,
      const PropertyTreeState& ancestor_state,
      FloatClipRect& mapping_rect);

  // Maps from a rect in |localTransformNode| space to its transformed rect in
  // |ancestorTransformNode| space. This is computed by multiplying the rect by
  // the combined transform between |localTransformNode| and
  // |ancestorTransformNode|, then flattening into 2D space.
  //
  // DCHECK fails if |localTransformNode| is not equal to or a descendant of
  // |ancestorTransformNode|.
  //
  //|mappingRect| is both input and output.
  static void LocalToAncestorRect(
      const TransformPaintPropertyNode* local_transform_node,
      const TransformPaintPropertyNode* ancestor_transform_node,
      FloatRect& mapping_rect);

  // Maps from a rect in |ancestorTransformNode| space to its transformed rect
  // in |localTransformNode| space. This is computed by multiplying the rect by
  // the inverse combined transform between |localTransformNode| and
  // |ancestorTransformNode|, if the transform is invertible.
  //
  // DCHECK fails if the combined transform is not invertible, or
  // |localTransformNode| is not equal to or a descendant of
  // |ancestorTransformNode|.
  //
  // |mappingRect| is both input and output.
  static void AncestorToLocalRect(
      const TransformPaintPropertyNode* ancestor_transform_node,
      const TransformPaintPropertyNode* local_transform_node,
      FloatRect& mapping_rect);

  // Returns the matrix used in |LocalToAncestorRect|. DCHECK fails iff
  // |localTransformNode| is not equal to or a descendant of
  // |ancestorTransformNode|.
  // This matrix may not be flattened. Since GeometryMapper only supports
  // flattened ancestor spaces, the returned matrix must be flattened to have
  // the correct semantics (calling mapRect() on it implicitly applies
  // flattening to the input; flattenTo2d() does it explicitly to tme matrix).
  static const TransformationMatrix& LocalToAncestorMatrix(
      const TransformPaintPropertyNode* local_transform_node,
      const TransformPaintPropertyNode* ancestor_transform_node);

  // Returns the "clip visual rect" between |localTransformState| and
  // |ancestorState|. See above for the definition of "clip visual rect".
  //
  // The output FloatClipRect may contain false positives for rounded-ness
  // if a rounded clip is clipped out, and overly conservative results
  // in the presences of transforms.
  static const FloatClipRect& LocalToAncestorClipRect(
      const PropertyTreeState& local_transform_state,
      const PropertyTreeState& ancestor_state);

  // Like localToAncestorClipRect, except it can handle destination transform
  // spaces which are not direct ancestors of the source transform space.
  //
  // The output FloatClipRect may contain false positives for rounded-ness
  // if a rounded clip is clipped out, and overly conservative results
  // in the presences of transforms.
  static const FloatClipRect& SourceToDestinationClipRect(
      const PropertyTreeState& source_state,
      const PropertyTreeState& destination_state);

  // Returns the lowest common ancestor in the paint property tree.
  template <typename NodeType>
  static PLATFORM_EXPORT const NodeType* LowestCommonAncestor(const NodeType*,
                                                              const NodeType*);

  static void ClearCache();

 private:
  // The internal methods do the same things as their public counterparts, but
  // take an extra |success| parameter which indicates if the function is
  // successful on return. See comments of the public functions for failure
  // conditions.

  static void SourceToDestinationVisualRectInternal(
      const PropertyTreeState& source_state,
      const PropertyTreeState& destination_state,
      FloatClipRect& mapping_rect,
      bool& success);

  static void LocalToAncestorVisualRectInternal(
      const PropertyTreeState& local_transform_state,
      const PropertyTreeState& ancestor_state,
      FloatClipRect& mapping_rect,
      bool& success);

  static void LocalToAncestorRectInternal(
      const TransformPaintPropertyNode* local_transform_node,
      const TransformPaintPropertyNode* ancestor_transform_node,
      FloatRect&,
      bool& success);

  static const TransformationMatrix& LocalToAncestorMatrixInternal(
      const TransformPaintPropertyNode* local_transform_node,
      const TransformPaintPropertyNode* ancestor_transform_node,
      bool& success);

  static const FloatClipRect& LocalToAncestorClipRectInternal(
      const ClipPaintPropertyNode* descendant,
      const ClipPaintPropertyNode* ancestor_clip,
      const TransformPaintPropertyNode* ancestor_transform,
      bool& success);

  static const FloatClipRect& SourceToDestinationClipRectInternal(
      const PropertyTreeState& source_state,
      const PropertyTreeState& destination_state,
      bool& success);

  static void SlowLocalToAncestorVisualRectWithEffects(
      const PropertyTreeState& local_state,
      const PropertyTreeState& ancestor_state,
      FloatClipRect& mapping_rect,
      bool& success);

  static const TransformationMatrix& IdentityMatrix();
  static const FloatClipRect& InfiniteClip();
  static FloatClipRect& TempRect();

  friend class GeometryMapperTest;
  friend class PaintLayerClipperTest;
};

}  // namespace blink

#endif  // GeometryMapper_h
