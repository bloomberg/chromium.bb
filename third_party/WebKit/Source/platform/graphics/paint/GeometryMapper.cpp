// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/GeometryMapper.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/geometry/LayoutRect.h"

namespace blink {

void GeometryMapper::sourceToDestinationVisualRect(
    const PropertyTreeState& sourceState,
    const PropertyTreeState& destinationState,
    FloatRect& rect) {
  bool success = false;
  sourceToDestinationVisualRectInternal(sourceState, destinationState, rect,
                                        success);
  DCHECK(success);
}

void GeometryMapper::sourceToDestinationVisualRectInternal(
    const PropertyTreeState& sourceState,
    const PropertyTreeState& destinationState,
    FloatRect& mappingRect,
    bool& success) {
  localToAncestorVisualRectInternal(sourceState, destinationState, mappingRect,
                                    success);
  // Success if destinationState is an ancestor state.
  if (success)
    return;

  // Otherwise first map to the lowest common ancestor, then map to destination.
  const TransformPaintPropertyNode* lcaTransform = lowestCommonAncestor(
      sourceState.transform(), destinationState.transform());
  DCHECK(lcaTransform);

  // Assume that the clip of destinationState is an ancestor of the clip of
  // sourceState and is under the space of lcaTransform. Otherwise
  // localToAncestorVisualRect() will fail.
  PropertyTreeState lcaState = destinationState;
  lcaState.setTransform(lcaTransform);

  localToAncestorVisualRectInternal(sourceState, lcaState, mappingRect,
                                    success);
  if (!success)
    return;

  ancestorToLocalRect(lcaTransform, destinationState.transform(), mappingRect);
}

void GeometryMapper::sourceToDestinationRect(
    const TransformPaintPropertyNode* sourceTransformNode,
    const TransformPaintPropertyNode* destinationTransformNode,
    FloatRect& mappingRect) {
  bool success = false;
  localToAncestorRectInternal(sourceTransformNode, destinationTransformNode,
                              mappingRect, success);
  // Success if destinationTransformNode is an ancestor of sourceTransformNode.
  if (success)
    return;

  // Otherwise first map to the least common ancestor, then map to destination.
  const TransformPaintPropertyNode* lcaTransform =
      lowestCommonAncestor(sourceTransformNode, destinationTransformNode);
  DCHECK(lcaTransform);

  localToAncestorRect(sourceTransformNode, lcaTransform, mappingRect);
  ancestorToLocalRect(lcaTransform, destinationTransformNode, mappingRect);
}

void GeometryMapper::localToAncestorVisualRect(
    const PropertyTreeState& localState,
    const PropertyTreeState& ancestorState,
    FloatRect& mappingRect) {
  bool success = false;
  localToAncestorVisualRectInternal(localState, ancestorState, mappingRect,
                                    success);
  DCHECK(success);
}

void GeometryMapper::localToAncestorVisualRectInternal(
    const PropertyTreeState& localState,
    const PropertyTreeState& ancestorState,
    FloatRect& rectToMap,
    bool& success) {
  if (localState == ancestorState) {
    success = true;
    return;
  }

  if (localState.effect() != ancestorState.effect()) {
    slowLocalToAncestorVisualRectWithEffects(localState, ancestorState,
                                             rectToMap, success);
    return;
  }

  const auto& transformMatrix = localToAncestorMatrixInternal(
      localState.transform(), ancestorState.transform(), success);
  if (!success) {
    return;
  }

  FloatRect mappedRect = transformMatrix.mapRect(rectToMap);

  const FloatClipRect& clipRect =
      localToAncestorClipRectInternal(localState.clip(), ancestorState.clip(),
                                      ancestorState.transform(), success);

  if (success) {
    rectToMap = clipRect.rect();
    rectToMap.intersect(mappedRect);
  } else if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    // On SPv1 we may fail when the paint invalidation container creates an
    // overflow clip (in ancestorState) which is not in localState of an
    // out-of-flow positioned descendant. See crbug.com/513108 and layout test
    // compositing/overflow/handle-non-ancestor-clip-parent.html (run with
    // --enable-prefer-compositing-to-lcd-text) for details.
    // Ignore it for SPv1 for now.
    success = true;
  }
}

void GeometryMapper::slowLocalToAncestorVisualRectWithEffects(
    const PropertyTreeState& localState,
    const PropertyTreeState& ancestorState,
    FloatRect& mappingRect,
    bool& success) {
  PropertyTreeState lastTransformAndClipState(localState.transform(),
                                              localState.clip(), nullptr);

  for (const auto* effect = localState.effect();
       effect && effect != ancestorState.effect(); effect = effect->parent()) {
    if (!effect->hasFilterThatMovesPixels())
      continue;

    PropertyTreeState transformAndClipState(effect->localTransformSpace(),
                                            effect->outputClip(), nullptr);
    sourceToDestinationVisualRectInternal(
        lastTransformAndClipState, transformAndClipState, mappingRect, success);
    if (!success)
      return;

    mappingRect = effect->mapRect(mappingRect);
    lastTransformAndClipState = transformAndClipState;
  }

  PropertyTreeState finalTransformAndClipState(ancestorState.transform(),
                                               ancestorState.clip(), nullptr);
  sourceToDestinationVisualRectInternal(lastTransformAndClipState,
                                        finalTransformAndClipState, mappingRect,
                                        success);
}

void GeometryMapper::localToAncestorRect(
    const TransformPaintPropertyNode* localTransformNode,
    const TransformPaintPropertyNode* ancestorTransformNode,
    FloatRect& mappingRect) {
  bool success = false;
  localToAncestorRectInternal(localTransformNode, ancestorTransformNode,
                              mappingRect, success);
  DCHECK(success);
}

void GeometryMapper::localToAncestorRectInternal(
    const TransformPaintPropertyNode* localTransformNode,
    const TransformPaintPropertyNode* ancestorTransformNode,
    FloatRect& mappingRect,
    bool& success) {
  if (localTransformNode == ancestorTransformNode) {
    success = true;
    return;
  }

  const auto& transformMatrix = localToAncestorMatrixInternal(
      localTransformNode, ancestorTransformNode, success);
  if (!success)
    return;
  mappingRect = transformMatrix.mapRect(mappingRect);
}

void GeometryMapper::ancestorToLocalRect(
    const TransformPaintPropertyNode* ancestorTransformNode,
    const TransformPaintPropertyNode* localTransformNode,
    FloatRect& rect) {
  if (localTransformNode == ancestorTransformNode)
    return;

  const auto& transformMatrix =
      localToAncestorMatrix(localTransformNode, ancestorTransformNode);
  DCHECK(transformMatrix.isInvertible());

  // TODO(chrishtr): Cache the inverse?
  rect = transformMatrix.inverse().mapRect(rect);
}

FloatClipRect GeometryMapper::localToAncestorClipRect(
    const PropertyTreeState& localState,
    const PropertyTreeState& ancestorState) {
  bool success = false;
  FloatClipRect result =
      localToAncestorClipRectInternal(localState.clip(), ancestorState.clip(),
                                      ancestorState.transform(), success);

  DCHECK(success);

  return result;
}

const FloatClipRect& GeometryMapper::sourceToDestinationClipRect(
    const PropertyTreeState& sourceState,
    const PropertyTreeState& destinationState) {
  bool success = false;
  const FloatClipRect& result = sourceToDestinationClipRectInternal(
      sourceState, destinationState, success);
  DCHECK(success);

  return result;
}

const FloatClipRect& GeometryMapper::sourceToDestinationClipRectInternal(
    const PropertyTreeState& sourceState,
    const PropertyTreeState& destinationState,
    bool& success) {
  const FloatClipRect& result = localToAncestorClipRectInternal(
      sourceState.clip(), destinationState.clip(), destinationState.transform(),
      success);
  // Success if destinationState is an ancestor state.
  if (success)
    return result;

  // Otherwise first map to the lowest common ancestor, then map to
  // destination.
  const TransformPaintPropertyNode* lcaTransform = lowestCommonAncestor(
      sourceState.transform(), destinationState.transform());
  DCHECK(lcaTransform);

  // Assume that the clip of destinationState is an ancestor of the clip of
  // sourceState and is under the space of lcaTransform. Otherwise
  // localToAncestorClipRectInternal() will fail.
  PropertyTreeState lcaState = destinationState;
  lcaState.setTransform(lcaTransform);

  const FloatClipRect& result2 = localToAncestorClipRectInternal(
      sourceState.clip(), lcaState.clip(), lcaState.transform(), success);
  if (!success) {
    if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      // On SPv1 we may fail when the paint invalidation container creates an
      // overflow clip (in ancestorState) which is not in localState of an
      // out-of-flow positioned descendant. See crbug.com/513108 and layout
      // test compositing/overflow/handle-non-ancestor-clip-parent.html (run
      // with --enable-prefer-compositing-to-lcd-text) for details.
      // Ignore it for SPv1 for now.
      success = true;
    }
    return result2;
  }
  if (!result2.isInfinite()) {
    FloatRect rect = result2.rect();
    ancestorToLocalRect(lcaTransform, destinationState.transform(), rect);
    m_tempRect.setRect(rect);
    if (result2.hasRadius())
      m_tempRect.setHasRadius();
    return m_tempRect;
  }
  return result2;
}

const FloatClipRect& GeometryMapper::localToAncestorClipRectInternal(
    const ClipPaintPropertyNode* descendant,
    const ClipPaintPropertyNode* ancestorClip,
    const TransformPaintPropertyNode* ancestorTransform,
    bool& success) {
  FloatClipRect clip;
  if (descendant == ancestorClip) {
    success = true;
    return m_infiniteClip;
  }

  const ClipPaintPropertyNode* clipNode = descendant;
  Vector<const ClipPaintPropertyNode*> intermediateNodes;

  GeometryMapperClipCache::ClipAndTransform clipAndTransform(ancestorClip,
                                                             ancestorTransform);
  // Iterate over the path from localState.clip to ancestorState.clip. Stop if
  // we've found a memoized (precomputed) clip for any particular node.
  while (clipNode && clipNode != ancestorClip) {
    if (const FloatClipRect* cachedClip =
            clipNode->getClipCache().getCachedClip(clipAndTransform)) {
      clip = *cachedClip;
      break;
    }

    intermediateNodes.push_back(clipNode);
    clipNode = clipNode->parent();
  }
  if (!clipNode) {
    success = false;
    return m_infiniteClip;
  }

  // Iterate down from the top intermediate node found in the previous loop,
  // computing and memoizing clip rects as we go.
  for (auto it = intermediateNodes.rbegin(); it != intermediateNodes.rend();
       ++it) {
    success = false;
    const TransformationMatrix& transformMatrix = localToAncestorMatrixInternal(
        (*it)->localTransformSpace(), ancestorTransform, success);
    if (!success)
      return m_infiniteClip;
    FloatRect mappedRect = transformMatrix.mapRect((*it)->clipRect().rect());
    clip.intersect(mappedRect);
    if ((*it)->clipRect().isRounded())
      clip.setHasRadius();

    (*it)->getClipCache().setCachedClip(clipAndTransform, clip);
  }

  success = true;

  const FloatClipRect* cachedClip =
      descendant->getClipCache().getCachedClip(clipAndTransform);
  DCHECK(cachedClip);
  CHECK(clip.hasRadius() == cachedClip->hasRadius());
  return *cachedClip;
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

  const TransformPaintPropertyNode* transformNode = localTransformNode;
  Vector<const TransformPaintPropertyNode*> intermediateNodes;
  TransformationMatrix transformMatrix;

  // Iterate over the path from localTransformNode to ancestorState.transform.
  // Stop if we've found a memoized (precomputed) transform for any particular
  // node.
  while (transformNode && transformNode != ancestorTransformNode) {
    if (const TransformationMatrix* cachedMatrix =
            transformNode->getTransformCache().getCachedTransform(
                ancestorTransformNode)) {
      transformMatrix = *cachedMatrix;
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
    (*it)->getTransformCache().setCachedTransform(ancestorTransformNode,
                                                  transformMatrix);
  }
  success = true;
  const TransformationMatrix* cachedMatrix =
      localTransformNode->getTransformCache().getCachedTransform(
          ancestorTransformNode);
  DCHECK(cachedMatrix);
  return *cachedMatrix;
}

void GeometryMapper::clearCache() {
  GeometryMapperTransformCache::clearCache();
  GeometryMapperClipCache::clearCache();
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
