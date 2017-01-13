// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/GeometryMapper.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/geometry/LayoutRect.h"

namespace blink {

FloatRect GeometryMapper::sourceToDestinationVisualRect(
    const FloatRect& rect,
    const PropertyTreeState& sourceState,
    const PropertyTreeState& destinationState) {
  bool success = false;
  FloatRect result = sourceToDestinationVisualRectInternal(
      rect, sourceState, destinationState, success);
  DCHECK(success);
  return result;
}

FloatRect GeometryMapper::sourceToDestinationVisualRectInternal(
    const FloatRect& rect,
    const PropertyTreeState& sourceState,
    const PropertyTreeState& destinationState,
    bool& success) {
  FloatRect result = localToAncestorVisualRectInternal(
      rect, sourceState, destinationState, success);
  // Success if destinationState is an ancestor state.
  if (success)
    return result;

  // Otherwise first map to the least common ancestor, then map to destination.
  const TransformPaintPropertyNode* lcaTransform = leastCommonAncestor(
      sourceState.transform(), destinationState.transform());
  DCHECK(lcaTransform);

  // Assume that the clip of destinationState is an ancestor of the clip of
  // sourceState and is under the space of lcaTransform. Otherwise
  // localToAncestorVisualRect() will fail.
  PropertyTreeState lcaState = destinationState;
  lcaState.setTransform(lcaTransform);

  FloatRect lcaVisualRect =
      localToAncestorVisualRectInternal(rect, sourceState, lcaState, success);
  if (!success)
    return lcaVisualRect;
  return ancestorToLocalRect(lcaVisualRect, lcaTransform,
                             destinationState.transform());
}

FloatRect GeometryMapper::sourceToDestinationRect(
    const FloatRect& rect,
    const TransformPaintPropertyNode* sourceTransformNode,
    const TransformPaintPropertyNode* destinationTransformNode) {
  bool success = false;
  FloatRect result = localToAncestorRectInternal(
      rect, sourceTransformNode, destinationTransformNode, success);
  // Success if destinationTransformNode is an ancestor of sourceTransformNode.
  if (success)
    return result;

  // Otherwise first map to the least common ancestor, then map to destination.
  const TransformPaintPropertyNode* lcaTransform =
      leastCommonAncestor(sourceTransformNode, destinationTransformNode);
  DCHECK(lcaTransform);

  FloatRect lcaRect =
      localToAncestorRect(rect, sourceTransformNode, lcaTransform);
  return ancestorToLocalRect(lcaRect, lcaTransform, destinationTransformNode);
}

FloatRect GeometryMapper::localToAncestorVisualRect(
    const FloatRect& rect,
    const PropertyTreeState& localState,
    const PropertyTreeState& ancestorState) {
  bool success = false;
  FloatRect result = localToAncestorVisualRectInternal(rect, localState,
                                                       ancestorState, success);
  DCHECK(success);
  return result;
}

FloatRect GeometryMapper::localToAncestorVisualRectInternal(
    const FloatRect& rect,
    const PropertyTreeState& localState,
    const PropertyTreeState& ancestorState,
    bool& success) {
  if (localState == ancestorState) {
    success = true;
    return rect;
  }

  const auto& transformMatrix = localToAncestorMatrixInternal(
      localState.transform(), ancestorState.transform(), success);
  if (!success)
    return rect;

  FloatRect mappedRect = transformMatrix.mapRect(rect);

  const auto clipRect =
      localToAncestorClipRectInternal(localState, ancestorState, success);

  if (success) {
    mappedRect.intersect(clipRect);
  } else if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    // On SPv1 we may fail when the paint invalidation container creates an
    // overflow clip (in ancestorState) which is not in localState of an
    // out-of-flow positioned descendant. See crbug.com/513108 and layout test
    // compositing/overflow/handle-non-ancestor-clip-parent.html (run with
    // --enable-prefer-compositing-to-lcd-text) for details.
    // Ignore it for SPv1 for now.
    success = true;
  }

  return mappedRect;
}

FloatRect GeometryMapper::localToAncestorRect(
    const FloatRect& rect,
    const TransformPaintPropertyNode* localTransformNode,
    const TransformPaintPropertyNode* ancestorTransformNode) {
  bool success = false;
  FloatRect result = localToAncestorRectInternal(
      rect, localTransformNode, ancestorTransformNode, success);
  DCHECK(success);
  return result;
}

FloatRect GeometryMapper::localToAncestorRectInternal(
    const FloatRect& rect,
    const TransformPaintPropertyNode* localTransformNode,
    const TransformPaintPropertyNode* ancestorTransformNode,
    bool& success) {
  if (localTransformNode == ancestorTransformNode) {
    success = true;
    return rect;
  }

  const auto& transformMatrix = localToAncestorMatrixInternal(
      localTransformNode, ancestorTransformNode, success);
  if (!success)
    return rect;
  return transformMatrix.mapRect(rect);
}

FloatRect GeometryMapper::ancestorToLocalRect(
    const FloatRect& rect,
    const TransformPaintPropertyNode* ancestorTransformNode,
    const TransformPaintPropertyNode* localTransformNode) {
  if (localTransformNode == ancestorTransformNode)
    return rect;

  const auto& transformMatrix =
      localToAncestorMatrix(localTransformNode, ancestorTransformNode);
  DCHECK(transformMatrix.isInvertible());

  // TODO(chrishtr): Cache the inverse?
  return transformMatrix.inverse().mapRect(rect);
}

PrecomputedDataForAncestor& GeometryMapper::getPrecomputedDataForAncestor(
    const TransformPaintPropertyNode* ancestorTransformNode) {
  auto addResult = m_data.add(ancestorTransformNode, nullptr);
  if (addResult.isNewEntry)
    addResult.storedValue->value = PrecomputedDataForAncestor::create();
  return *addResult.storedValue->value;
}

FloatRect GeometryMapper::localToAncestorClipRect(
    const PropertyTreeState& localState,
    const PropertyTreeState& ancestorState) {
  bool success = false;
  FloatRect result =
      localToAncestorClipRectInternal(localState, ancestorState, success);
  DCHECK(success);
  return result;
}

FloatRect GeometryMapper::localToAncestorClipRectInternal(
    const PropertyTreeState& localState,
    const PropertyTreeState& ancestorState,
    bool& success) {
  FloatRect clip(LayoutRect::infiniteIntRect());
  if (localState.clip() == ancestorState.clip()) {
    success = true;
    return clip;
  }

  PrecomputedDataForAncestor& precomputedData =
      getPrecomputedDataForAncestor(ancestorState.transform());
  const ClipPaintPropertyNode* clipNode = localState.clip();
  Vector<const ClipPaintPropertyNode*> intermediateNodes;

  // Iterate over the path from localState.clip to ancestorState.clip. Stop if
  // we've found a memoized (precomputed) clip for any particular node.
  while (clipNode && clipNode != ancestorState.clip()) {
    auto it = precomputedData.toAncestorClipRects.find(clipNode);
    if (it != precomputedData.toAncestorClipRects.end()) {
      clip = it->value;
      break;
    }
    intermediateNodes.push_back(clipNode);
    clipNode = clipNode->parent();
  }
  if (!clipNode) {
    success = false;
    return clip;
  }

  // Iterate down from the top intermediate node found in the previous loop,
  // computing and memoizing clip rects as we go.
  for (auto it = intermediateNodes.rbegin(); it != intermediateNodes.rend();
       ++it) {
    success = false;
    const TransformationMatrix& transformMatrix = localToAncestorMatrixInternal(
        (*it)->localTransformSpace(), ancestorState.transform(), success);
    if (!success)
      return clip;
    FloatRect mappedRect = transformMatrix.mapRect((*it)->clipRect().rect());
    clip.intersect(mappedRect);
    precomputedData.toAncestorClipRects.set(*it, clip);
  }

  success = true;
  return precomputedData.toAncestorClipRects.find(localState.clip())->value;
}

const TransformationMatrix& GeometryMapper::localToAncestorMatrix(
    const TransformPaintPropertyNode* localTransformNode,
    const TransformPaintPropertyNode* ancestorTransformNode) {
  bool success = false;
  const auto& result = localToAncestorMatrixInternal(
      localTransformNode, ancestorTransformNode, success);
  DCHECK(success);
  return result;
}

const TransformationMatrix& GeometryMapper::localToAncestorMatrixInternal(
    const TransformPaintPropertyNode* localTransformNode,
    const TransformPaintPropertyNode* ancestorTransformNode,
    bool& success) {
  if (localTransformNode == ancestorTransformNode) {
    success = true;
    return m_identity;
  }

  PrecomputedDataForAncestor& precomputedData =
      getPrecomputedDataForAncestor(ancestorTransformNode);

  const TransformPaintPropertyNode* transformNode = localTransformNode;
  Vector<const TransformPaintPropertyNode*> intermediateNodes;
  TransformationMatrix transformMatrix;

  // Iterate over the path from localTransformNode to ancestorState.transform.
  // Stop if we've found a memoized (precomputed) transform for any particular
  // node.
  while (transformNode && transformNode != ancestorTransformNode) {
    auto it = precomputedData.toAncestorTransforms.find(transformNode);
    if (it != precomputedData.toAncestorTransforms.end()) {
      transformMatrix = it->value;
      break;
    }
    intermediateNodes.push_back(transformNode);
    transformNode = transformNode->parent();
  }
  if (!transformNode) {
    success = false;
    return m_identity;
  }

  // Iterate down from the top intermediate node found in the previous loop,
  // computing and memoizing transforms as we go.
  for (auto it = intermediateNodes.rbegin(); it != intermediateNodes.rend();
       it++) {
    TransformationMatrix localTransformMatrix = (*it)->matrix();
    localTransformMatrix.applyTransformOrigin((*it)->origin());
    transformMatrix = transformMatrix * localTransformMatrix;
    precomputedData.toAncestorTransforms.set(*it, transformMatrix);
  }
  success = true;
  return precomputedData.toAncestorTransforms.find(localTransformNode)->value;
}

namespace {

unsigned transformPropertyTreeNodeDepth(
    const TransformPaintPropertyNode* node) {
  unsigned depth = 0;
  while (node) {
    depth++;
    node = node->parent();
  }
  return depth;
}
}

const TransformPaintPropertyNode* GeometryMapper::leastCommonAncestor(
    const TransformPaintPropertyNode* a,
    const TransformPaintPropertyNode* b) {
  // Measure both depths.
  unsigned depthA = transformPropertyTreeNodeDepth(a);
  unsigned depthB = transformPropertyTreeNodeDepth(b);

  // Make it so depthA >= depthB.
  if (depthA < depthB) {
    std::swap(a, b);
    std::swap(depthA, depthB);
  }

  // Make it so depthA == depthB.
  while (depthA > depthB) {
    a = a->parent();
    depthA--;
  }

  // Walk up until we find the ancestor.
  while (a != b) {
    a = a->parent();
    b = b->parent();
  }
  return a;
}

}  // namespace blink
