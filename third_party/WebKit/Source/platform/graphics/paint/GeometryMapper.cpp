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
    const PropertyTreeState& destinationState,
    bool& success) {
  FloatRect result =
      localToAncestorVisualRect(rect, sourceState, destinationState, success);
  if (success)
    return result;
  return slowSourceToDestinationVisualRect(rect, sourceState, destinationState,
                                           success);
}

FloatRect GeometryMapper::sourceToDestinationRect(
    const FloatRect& rect,
    const TransformPaintPropertyNode* sourceTransformNode,
    const TransformPaintPropertyNode* destinationTransformNode,
    bool& success) {
  FloatRect result = localToAncestorRect(rect, sourceTransformNode,
                                         destinationTransformNode, success);
  if (success)
    return result;
  return slowSourceToDestinationRect(rect, sourceTransformNode,
                                     destinationTransformNode, success);
}

FloatRect GeometryMapper::slowSourceToDestinationVisualRect(
    const FloatRect& rect,
    const PropertyTreeState& sourceState,
    const PropertyTreeState& destinationState,
    bool& success) {
  const TransformPaintPropertyNode* lcaTransform = leastCommonAncestor(
      sourceState.transform(), destinationState.transform());
  DCHECK(lcaTransform);

  // Assume that the clip of destinationState is an ancestor of the clip of
  // sourceState and is under the space of lcaTransform. Otherwise
  // localToAncestorClipRect() will fail.
  PropertyTreeState lcaState = destinationState;
  lcaState.setTransform(lcaTransform);

  FloatRect result =
      localToAncestorVisualRect(rect, sourceState, lcaState, success);
  if (!success)
    return result;

  return ancestorToLocalRect(result, lcaTransform, destinationState.transform(),
                             success);
}

FloatRect GeometryMapper::slowSourceToDestinationRect(
    const FloatRect& rect,
    const TransformPaintPropertyNode* sourceTransformNode,
    const TransformPaintPropertyNode* destinationTransformNode,
    bool& success) {
  const TransformPaintPropertyNode* lcaTransform =
      leastCommonAncestor(sourceTransformNode, destinationTransformNode);
  DCHECK(lcaTransform);

  FloatRect result =
      localToAncestorRect(rect, sourceTransformNode, lcaTransform, success);
  DCHECK(success);

  return ancestorToLocalRect(result, lcaTransform, destinationTransformNode,
                             success);
}

FloatRect GeometryMapper::localToAncestorVisualRect(
    const FloatRect& rect,
    const PropertyTreeState& localState,
    const PropertyTreeState& ancestorState,
    bool& success) {
  if (localState == ancestorState) {
    success = true;
    return rect;
  }

  const auto& transformMatrix = localToAncestorMatrix(
      localState.transform(), ancestorState.transform(), success);
  if (!success)
    return rect;

  FloatRect mappedRect = transformMatrix.mapRect(rect);

  const auto clipRect =
      localToAncestorClipRect(localState, ancestorState, success);

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
    const TransformPaintPropertyNode* ancestorTransformNode,
    bool& success) {
  if (localTransformNode == ancestorTransformNode) {
    success = true;
    return rect;
  }

  const auto& transformMatrix =
      localToAncestorMatrix(localTransformNode, ancestorTransformNode, success);
  if (!success)
    return rect;
  return transformMatrix.mapRect(rect);
}

FloatRect GeometryMapper::ancestorToLocalRect(
    const FloatRect& rect,
    const TransformPaintPropertyNode* ancestorTransformNode,
    const TransformPaintPropertyNode* localTransformNode,
    bool& success) {
  if (localTransformNode == ancestorTransformNode) {
    success = true;
    return rect;
  }

  const auto& transformMatrix =
      localToAncestorMatrix(localTransformNode, ancestorTransformNode, success);
  if (!success)
    return rect;

  if (!transformMatrix.isInvertible()) {
    success = false;
    return rect;
  }
  success = true;

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
    const TransformationMatrix& transformMatrix = localToAncestorMatrix(
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
