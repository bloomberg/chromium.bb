// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GeometryMapper_h
#define GeometryMapper_h

#include "platform/graphics/paint/FloatClipRect.h"
#include "platform/graphics/paint/PropertyTreeState.h"
#include "platform/transforms/TransformationMatrix.h"
#include "wtf/HashMap.h"

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
 public:
  static std::unique_ptr<GeometryMapper> create() {
    return WTF::wrapUnique(new GeometryMapper());
  }

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
  void sourceToDestinationVisualRect(const PropertyTreeState& sourceState,
                                     const PropertyTreeState& destinationState,
                                     FloatRect& mappingRect);

  // Same as sourceToDestinationVisualRect() except that only transforms are
  // applied.
  //
  // |mappingRect| is both input and output.
  void sourceToDestinationRect(
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
  void localToAncestorVisualRect(const PropertyTreeState& localTransformState,
                                 const PropertyTreeState& ancestorState,
                                 FloatRect& mappingRect);

  // Maps from a rect in |localTransformNode| space to its transformed rect in
  // |ancestorTransformNode| space. This is computed by multiplying the rect by
  // the combined transform between |localTransformNode| and
  // |ancestorTransformNode|, then flattening into 2D space.
  //
  // DCHECK fails if |localTransformNode| is not equal to or a descendant of
  // |ancestorTransformNode|.
  //
  //|mappingRect| is both input and output.
  void localToAncestorRect(
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
  void ancestorToLocalRect(
      const TransformPaintPropertyNode* ancestorTransformNode,
      const TransformPaintPropertyNode* localTransformNode,
      FloatRect& mappingRect);

  // Returns the matrix used in |LocalToAncestorRect|. DCHECK fails iff
  // |localTransformNode| is not equal to or a descendant of
  // |ancestorTransformNode|.
  const TransformationMatrix& localToAncestorMatrix(
      const TransformPaintPropertyNode* localTransformNode,
      const TransformPaintPropertyNode* ancestorTransformNode);

  // Returns the "clip visual rect" between |localTransformState| and
  // |ancestorState|. See above for the definition of "clip visual rect".
  FloatClipRect localToAncestorClipRect(
      const PropertyTreeState& localTransformState,
      const PropertyTreeState& ancestorState);

  // Like localToAncestorClipRect, except it can handle destination transform
  // spaces which are not direct ancestors of the source transform space.
  const FloatClipRect& sourceToDestinationClipRect(
      const PropertyTreeState& sourceState,
      const PropertyTreeState& destinationState);

  // Returns the lowest common ancestor in the paint property tree.
  template <typename NodeType>
  static PLATFORM_EXPORT const NodeType* lowestCommonAncestor(const NodeType*,
                                                              const NodeType*);

  void clearCache();

 protected:
  GeometryMapper() {}

 private:
  // The internal methods do the same things as their public counterparts, but
  // take an extra |success| parameter which indicates if the function is
  // successful on return. See comments of the public functions for failure
  // conditions.

  void sourceToDestinationVisualRectInternal(
      const PropertyTreeState& sourceState,
      const PropertyTreeState& destinationState,
      FloatRect& mappingRect,
      bool& success);

  void localToAncestorVisualRectInternal(
      const PropertyTreeState& localTransformState,
      const PropertyTreeState& ancestorState,
      FloatRect& mappingRect,
      bool& success);

  void localToAncestorRectInternal(
      const TransformPaintPropertyNode* localTransformNode,
      const TransformPaintPropertyNode* ancestorTransformNode,
      FloatRect&,
      bool& success);

  const TransformationMatrix& localToAncestorMatrixInternal(
      const TransformPaintPropertyNode* localTransformNode,
      const TransformPaintPropertyNode* ancestorTransformNode,
      bool& success);

  const FloatClipRect& localToAncestorClipRectInternal(
      const ClipPaintPropertyNode* descendant,
      const ClipPaintPropertyNode* ancestorClip,
      const TransformPaintPropertyNode* ancestorTransform,
      bool& success);

  const FloatClipRect& sourceToDestinationClipRectInternal(
      const PropertyTreeState& sourceState,
      const PropertyTreeState& destinationState,
      bool& success);

  void slowLocalToAncestorVisualRectWithEffects(
      const PropertyTreeState& localState,
      const PropertyTreeState& ancestorState,
      FloatRect& mappingRect,
      bool& success);

  friend class GeometryMapperTest;
  friend class PaintLayerClipperTest;

  // These are used to represent various return values of the above
  // methods.
  const TransformationMatrix m_identity;
  const FloatClipRect m_infiniteClip;
  FloatClipRect m_tempRect;

  DISALLOW_COPY_AND_ASSIGN(GeometryMapper);
};

}  // namespace blink

#endif  // GeometryMapper_h
