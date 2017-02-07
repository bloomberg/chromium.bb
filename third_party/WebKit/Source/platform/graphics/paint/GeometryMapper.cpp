// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/GeometryMapper.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/geometry/LayoutRect.h"

namespace blink {

FloatClipRect GeometryMapper::sourceToDestinationVisualRect(
    const FloatRect& rect,
    const PropertyTreeState& sourceState,
    const PropertyTreeState& destinationState) {
  bool success = false;
  FloatClipRect result = sourceToDestinationVisualRectInternal(
      rect, sourceState, destinationState, success);
  DCHECK(success);
  return result;
}

FloatClipRect GeometryMapper::sourceToDestinationVisualRectInternal(
    const FloatRect& rect,
    const PropertyTreeState& sourceState,
    const PropertyTreeState& destinationState,
    bool& success) {
  FloatClipRect result = localToAncestorVisualRectInternal(
      rect, sourceState, destinationState, success);
  // Success if destinationState is an ancestor state.
  if (success)
    return result;

  // Otherwise first map to the lowest common ancestor, then map to destination.
  const TransformPaintPropertyNode* lcaTransform = lowestCommonAncestor(
      sourceState.transform(), destinationState.transform());
  DCHECK(lcaTransform);

  // Assume that the clip of destinationState is an ancestor of the clip of
  // sourceState and is under the space of lcaTransform. Otherwise
  // localToAncestorVisualRect() will fail.
  PropertyTreeState lcaState = destinationState;
  lcaState.setTransform(lcaTransform);

  result =
      localToAncestorVisualRectInternal(rect, sourceState, lcaState, success);
  if (!success)
    return result;
  FloatRect final = ancestorToLocalRect(result.rect(), lcaTransform,
                                        destinationState.transform());
  result.setRect(final);
  return result;
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
      lowestCommonAncestor(sourceTransformNode, destinationTransformNode);
  DCHECK(lcaTransform);

  FloatRect lcaRect =
      localToAncestorRect(rect, sourceTransformNode, lcaTransform);
  return ancestorToLocalRect(lcaRect, lcaTransform, destinationTransformNode);
}

FloatClipRect GeometryMapper::localToAncestorVisualRect(
    const FloatRect& rect,
    const PropertyTreeState& localState,
    const PropertyTreeState& ancestorState) {
  bool success = false;
  FloatClipRect result = localToAncestorVisualRectInternal(
      rect, localState, ancestorState, success);
  DCHECK(success);
  return result;
}

FloatClipRect GeometryMapper::localToAncestorVisualRectInternal(
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

  FloatClipRect clipRect =
      localToAncestorClipRectInternal(localState, ancestorState, success);

  if (success) {
    clipRect.intersect(mappedRect);
  } else if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    // On SPv1 we may fail when the paint invalidation container creates an
    // overflow clip (in ancestorState) which is not in localState of an
    // out-of-flow positioned descendant. See crbug.com/513108 and layout test
    // compositing/overflow/handle-non-ancestor-clip-parent.html (run with
    // --enable-prefer-compositing-to-lcd-text) for details.
    // Ignore it for SPv1 for now.
    success = true;
  }

  return clipRect;
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
    const TransformPaintPropertyNode* transform) {
  auto addResult = m_data.insert(transform, nullptr);
  if (addResult.isNewEntry)
    addResult.storedValue->value = PrecomputedDataForAncestor::create();
  return *addResult.storedValue->value;
}

TransformCache& GeometryMapper::getTransformCache(
    const TransformPaintPropertyNode* node) {
  return getPrecomputedDataForAncestor(node).toAncestorTransforms;
}

ClipCache& GeometryMapper::getClipCache(
    const TransformPaintPropertyNode* transform,
    const ClipPaintPropertyNode* clip) {
  PrecomputedDataForAncestor& precomputedData =
      getPrecomputedDataForAncestor(transform);

  auto addResult = precomputedData.precomputedClips.insert(clip, nullptr);
  if (addResult.isNewEntry)
    addResult.storedValue->value = WTF::makeUnique<ClipCache>();
  return *addResult.storedValue->value;
}

FloatClipRect GeometryMapper::localToAncestorClipRect(
    const PropertyTreeState& localState,
    const PropertyTreeState& ancestorState) {
  bool success = false;
  FloatClipRect result =
      localToAncestorClipRectInternal(localState, ancestorState, success);
  DCHECK(success);
  return result;
}

FloatClipRect GeometryMapper::localToAncestorClipRectInternal(
    const PropertyTreeState& localState,
    const PropertyTreeState& ancestorState,
    bool& success) {
  FloatClipRect clip(LayoutRect::infiniteIntRect());
  if (localState.clip() == ancestorState.clip()) {
    success = true;
    return clip;
  }

  ClipCache& clipCache =
      getClipCache(ancestorState.transform(), ancestorState.clip());
  const ClipPaintPropertyNode* clipNode = localState.clip();
  Vector<const ClipPaintPropertyNode*> intermediateNodes;

  // Iterate over the path from localState.clip to ancestorState.clip. Stop if
  // we've found a memoized (precomputed) clip for any particular node.
  while (clipNode && clipNode != ancestorState.clip()) {
    auto it = clipCache.find(clipNode);
    if (it != clipCache.end()) {
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
    if ((*it)->clipRect().isRounded())
      clip.setHasRadius(true);
    clipCache.set(*it, clip);
  }

  success = true;
  return clipCache.find(localState.clip())->value;
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

  TransformCache& transformCache = getTransformCache(ancestorTransformNode);

  const TransformPaintPropertyNode* transformNode = localTransformNode;
  Vector<const TransformPaintPropertyNode*> intermediateNodes;
  TransformationMatrix transformMatrix;

  // Iterate over the path from localTransformNode to ancestorState.transform.
  // Stop if we've found a memoized (precomputed) transform for any particular
  // node.
  while (transformNode && transformNode != ancestorTransformNode) {
    auto it = transformCache.find(transformNode);
    if (it != transformCache.end()) {
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
    transformCache.set(*it, transformMatrix);
  }
  success = true;
  return transformCache.find(localTransformNode)->value;
}

void GeometryMapper::clearCache() {
  m_data.clear();
}

namespace {

template <typename NodeType>
unsigned nodeDepth(const NodeType* node) {
  unsigned depth = 0;
  while (node) {
    depth++;
    node = node->parent();
  }
  return depth;
}

}  // namespace

template <typename NodeType>
const NodeType* GeometryMapper::lowestCommonAncestor(const NodeType* a,
                                                     const NodeType* b) {
  // Measure both depths.
  unsigned depthA = nodeDepth(a);
  unsigned depthB = nodeDepth(b);

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

// Explicitly instantiate the template for all supported types. This allows
// placing the template implementation in this .cpp file. See
// http://stackoverflow.com/a/488989 for more.
template const EffectPaintPropertyNode* GeometryMapper::lowestCommonAncestor(
    const EffectPaintPropertyNode*,
    const EffectPaintPropertyNode*);
template const TransformPaintPropertyNode* GeometryMapper::lowestCommonAncestor(
    const TransformPaintPropertyNode*,
    const TransformPaintPropertyNode*);
template const ClipPaintPropertyNode* GeometryMapper::lowestCommonAncestor(
    const ClipPaintPropertyNode*,
    const ClipPaintPropertyNode*);
template const ScrollPaintPropertyNode* GeometryMapper::lowestCommonAncestor(
    const ScrollPaintPropertyNode*,
    const ScrollPaintPropertyNode*);

}  // namespace blink
