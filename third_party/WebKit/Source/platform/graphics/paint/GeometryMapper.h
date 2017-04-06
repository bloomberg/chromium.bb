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
  static void sourceToDestinationVisualRect(
      const PropertyTreeState& sourceState,
      const PropertyTreeState& destinationState,
      FloatClipRect& mappingRect);

  // Same as sourceToDestinationVisualRect() except that only transforms are
  // applied.
  //
  // |mappingRect| is both input and output.
  static void sourceToDestinationRect(
      const TransformPaintPropertyNode* sourceTransformNode,
      const TransformPaintPropertyNode* destinationTransformNode,
      FloatRect& mappingRect);

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
  static void localToAncestorVisualRect(
      const PropertyTreeState& localTransformState,
      const PropertyTreeState& ancestorState,
      FloatClipRect& mappingRect);

  // Maps from a rect in |localTransformNode| space to its transformed rect in
  // |ancestorTransformNode| space. This is computed by multiplying the rect by
  // the combined transform between |localTransformNode| and
  // |ancestorTransformNode|, then flattening into 2D space.
  //
  // DCHECK fails if |localTransformNode| is not equal to or a descendant of
  // |ancestorTransformNode|.
  //
  //|mappingRect| is both input and output.
  static void localToAncestorRect(
      const TransformPaintPropertyNode* localTransformNode,
      const TransformPaintPropertyNode* ancestorTransformNode,
      FloatRect& mappingRect);

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
  static void ancestorToLocalRect(
      const TransformPaintPropertyNode* ancestorTransformNode,
      const TransformPaintPropertyNode* localTransformNode,
      FloatRect& mappingRect);

  // Returns the matrix used in |LocalToAncestorRect|. DCHECK fails iff
  // |localTransformNode| is not equal to or a descendant of
  // |ancestorTransformNode|.
  // This matrix may not be flattened. Since GeometryMapper only supports
  // flattened ancestor spaces, the returned matrix must be flattened to have
  // the correct semantics (calling mapRect() on it implicitly applies
  // flattening to the input; flattenTo2d() does it explicitly to tme matrix).
  static const TransformationMatrix& localToAncestorMatrix(
      const TransformPaintPropertyNode* localTransformNode,
      const TransformPaintPropertyNode* ancestorTransformNode);

  // Returns the "clip visual rect" between |localTransformState| and
  // |ancestorState|. See above for the definition of "clip visual rect".
  //
  // The output FloatClipRect may contain false positives for rounded-ness
  // if a rounded clip is clipped out, and overly conservative results
  // in the presences of transforms.
  static FloatClipRect localToAncestorClipRect(
      const PropertyTreeState& localTransformState,
      const PropertyTreeState& ancestorState);

  // Like localToAncestorClipRect, except it can handle destination transform
  // spaces which are not direct ancestors of the source transform space.
  //
  // The output FloatClipRect may contain false positives for rounded-ness
  // if a rounded clip is clipped out, and overly conservative results
  // in the presences of transforms.
  static const FloatClipRect& sourceToDestinationClipRect(
      const PropertyTreeState& sourceState,
      const PropertyTreeState& destinationState);

  // Returns the lowest common ancestor in the paint property tree.
  template <typename NodeType>
  static PLATFORM_EXPORT const NodeType* lowestCommonAncestor(const NodeType*,
                                                              const NodeType*);

  static void clearCache();

 private:
  // The internal methods do the same things as their public counterparts, but
  // take an extra |success| parameter which indicates if the function is
  // successful on return. See comments of the public functions for failure
  // conditions.

  static void sourceToDestinationVisualRectInternal(
      const PropertyTreeState& sourceState,
      const PropertyTreeState& destinationState,
      FloatClipRect& mappingRect,
      bool& success);

  static void localToAncestorVisualRectInternal(
      const PropertyTreeState& localTransformState,
      const PropertyTreeState& ancestorState,
      FloatClipRect& mappingRect,
      bool& success);

  static void localToAncestorRectInternal(
      const TransformPaintPropertyNode* localTransformNode,
      const TransformPaintPropertyNode* ancestorTransformNode,
      FloatRect&,
      bool& success);

  static const TransformationMatrix& localToAncestorMatrixInternal(
      const TransformPaintPropertyNode* localTransformNode,
      const TransformPaintPropertyNode* ancestorTransformNode,
      bool& success);

  static const FloatClipRect& localToAncestorClipRectInternal(
      const ClipPaintPropertyNode* descendant,
      const ClipPaintPropertyNode* ancestorClip,
      const TransformPaintPropertyNode* ancestorTransform,
      bool& success);

  static const FloatClipRect& sourceToDestinationClipRectInternal(
      const PropertyTreeState& sourceState,
      const PropertyTreeState& destinationState,
      bool& success);

  static void slowLocalToAncestorVisualRectWithEffects(
      const PropertyTreeState& localState,
      const PropertyTreeState& ancestorState,
      FloatClipRect& mappingRect,
      bool& success);

  static const TransformationMatrix& identityMatrix();
  static const FloatClipRect& infiniteClip();
  static FloatClipRect& tempRect();

  friend class GeometryMapperTest;
  friend class PaintLayerClipperTest;
};

}  // namespace blink

#endif  // GeometryMapper_h
