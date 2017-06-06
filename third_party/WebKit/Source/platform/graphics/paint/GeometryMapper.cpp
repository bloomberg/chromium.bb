// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/GeometryMapper.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/geometry/LayoutRect.h"

namespace blink {

const TransformationMatrix& GeometryMapper::IdentityMatrix() {
  DEFINE_STATIC_LOCAL(TransformationMatrix, identity, (TransformationMatrix()));
  return identity;
}

const FloatClipRect& GeometryMapper::InfiniteClip() {
  DEFINE_STATIC_LOCAL(FloatClipRect, infinite, (FloatClipRect()));
  return infinite;
}

FloatClipRect& GeometryMapper::TempRect() {
  DEFINE_STATIC_LOCAL(FloatClipRect, temp, (FloatClipRect()));
  return temp;
}

void GeometryMapper::SourceToDestinationVisualRect(
    const PropertyTreeState& source_state,
    const PropertyTreeState& destination_state,
    FloatClipRect& rect) {
  bool success = false;
  SourceToDestinationVisualRectInternal(source_state, destination_state, rect,
                                        success);
  DCHECK(success);
}

void GeometryMapper::SourceToDestinationVisualRectInternal(
    const PropertyTreeState& source_state,
    const PropertyTreeState& destination_state,
    FloatClipRect& mapping_rect,
    bool& success) {
  LocalToAncestorVisualRectInternal(source_state, destination_state,
                                    mapping_rect, success);
  // Success if destinationState is an ancestor state.
  if (success)
    return;

  // Otherwise first map to the lowest common ancestor, then map to destination.
  const TransformPaintPropertyNode* lca_transform = LowestCommonAncestor(
      source_state.Transform(), destination_state.Transform());
  DCHECK(lca_transform);

  // Assume that the clip of destinationState is an ancestor of the clip of
  // sourceState and is under the space of lcaTransform. Otherwise
  // localToAncestorVisualRect() will fail.
  PropertyTreeState lca_state = destination_state;
  lca_state.SetTransform(lca_transform);

  LocalToAncestorVisualRectInternal(source_state, lca_state, mapping_rect,
                                    success);
  if (!success)
    return;

  AncestorToLocalRect(lca_transform, destination_state.Transform(),
                      mapping_rect.Rect());
}

void GeometryMapper::SourceToDestinationRect(
    const TransformPaintPropertyNode* source_transform_node,
    const TransformPaintPropertyNode* destination_transform_node,
    FloatRect& mapping_rect) {
  bool success = false;
  LocalToAncestorRectInternal(source_transform_node, destination_transform_node,
                              mapping_rect, success);
  // Success if destinationTransformNode is an ancestor of sourceTransformNode.
  if (success)
    return;

  // Otherwise first map to the least common ancestor, then map to destination.
  const TransformPaintPropertyNode* lca_transform =
      LowestCommonAncestor(source_transform_node, destination_transform_node);
  DCHECK(lca_transform);

  LocalToAncestorRect(source_transform_node, lca_transform, mapping_rect);
  AncestorToLocalRect(lca_transform, destination_transform_node, mapping_rect);
}

void GeometryMapper::LocalToAncestorVisualRect(
    const PropertyTreeState& local_state,
    const PropertyTreeState& ancestor_state,
    FloatClipRect& mapping_rect) {
  bool success = false;
  LocalToAncestorVisualRectInternal(local_state, ancestor_state, mapping_rect,
                                    success);
  DCHECK(success);
}

void GeometryMapper::LocalToAncestorVisualRectInternal(
    const PropertyTreeState& local_state,
    const PropertyTreeState& ancestor_state,
    FloatClipRect& rect_to_map,
    bool& success) {
  if (local_state == ancestor_state) {
    success = true;
    return;
  }

  if (local_state.Effect() != ancestor_state.Effect()) {
    SlowLocalToAncestorVisualRectWithEffects(local_state, ancestor_state,
                                             rect_to_map, success);
    return;
  }

  const auto& transform_matrix = LocalToAncestorMatrixInternal(
      local_state.Transform(), ancestor_state.Transform(), success);
  if (!success) {
    return;
  }

  FloatRect mapped_rect = transform_matrix.MapRect(rect_to_map.Rect());

  const FloatClipRect& clip_rect =
      LocalToAncestorClipRectInternal(local_state.Clip(), ancestor_state.Clip(),
                                      ancestor_state.Transform(), success);

  if (success) {
    // This is where we propagate the rounded-ness of |clipRect| to
    // |rectToMap|.
    rect_to_map = clip_rect;
    rect_to_map.Intersect(mapped_rect);
  } else if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    // On SPv1 we may fail when the paint invalidation container creates an
    // overflow clip (in ancestorState) which is not in localState of an
    // out-of-flow positioned descendant. See crbug.com/513108 and layout test
    // compositing/overflow/handle-non-ancestor-clip-parent.html (run with
    // --enable-prefer-compositing-to-lcd-text) for details.
    // Ignore it for SPv1 for now.
    success = true;
    rect_to_map.SetRect(mapped_rect);
  }
}

void GeometryMapper::SlowLocalToAncestorVisualRectWithEffects(
    const PropertyTreeState& local_state,
    const PropertyTreeState& ancestor_state,
    FloatClipRect& mapping_rect,
    bool& success) {
  PropertyTreeState last_transform_and_clip_state(local_state.Transform(),
                                                  local_state.Clip(), nullptr);

  for (const auto* effect = local_state.Effect();
       effect && effect != ancestor_state.Effect(); effect = effect->Parent()) {
    if (!effect->HasFilterThatMovesPixels())
      continue;

    PropertyTreeState transform_and_clip_state(effect->LocalTransformSpace(),
                                               effect->OutputClip(), nullptr);
    SourceToDestinationVisualRectInternal(last_transform_and_clip_state,
                                          transform_and_clip_state,
                                          mapping_rect, success);
    if (!success)
      return;

    mapping_rect.SetRect(effect->MapRect(mapping_rect.Rect()));
    last_transform_and_clip_state = transform_and_clip_state;
  }

  PropertyTreeState final_transform_and_clip_state(
      ancestor_state.Transform(), ancestor_state.Clip(), nullptr);
  SourceToDestinationVisualRectInternal(last_transform_and_clip_state,
                                        final_transform_and_clip_state,
                                        mapping_rect, success);
}

void GeometryMapper::LocalToAncestorRect(
    const TransformPaintPropertyNode* local_transform_node,
    const TransformPaintPropertyNode* ancestor_transform_node,
    FloatRect& mapping_rect) {
  bool success = false;
  LocalToAncestorRectInternal(local_transform_node, ancestor_transform_node,
                              mapping_rect, success);
  DCHECK(success);
}

void GeometryMapper::LocalToAncestorRectInternal(
    const TransformPaintPropertyNode* local_transform_node,
    const TransformPaintPropertyNode* ancestor_transform_node,
    FloatRect& mapping_rect,
    bool& success) {
  if (local_transform_node == ancestor_transform_node) {
    success = true;
    return;
  }

  const auto& transform_matrix = LocalToAncestorMatrixInternal(
      local_transform_node, ancestor_transform_node, success);
  if (!success)
    return;
  mapping_rect = transform_matrix.MapRect(mapping_rect);
}

void GeometryMapper::AncestorToLocalRect(
    const TransformPaintPropertyNode* ancestor_transform_node,
    const TransformPaintPropertyNode* local_transform_node,
    FloatRect& rect) {
  if (local_transform_node == ancestor_transform_node)
    return;

  const auto& transform_matrix =
      LocalToAncestorMatrix(local_transform_node, ancestor_transform_node);
  DCHECK(transform_matrix.IsInvertible());

  // TODO(chrishtr): Cache the inverse?
  rect = transform_matrix.Inverse().MapRect(rect);
}

const FloatClipRect& GeometryMapper::LocalToAncestorClipRect(
    const PropertyTreeState& local_state,
    const PropertyTreeState& ancestor_state) {
  bool success = false;
  const FloatClipRect& result =
      LocalToAncestorClipRectInternal(local_state.Clip(), ancestor_state.Clip(),
                                      ancestor_state.Transform(), success);

  DCHECK(success);

  return result;
}

const FloatClipRect& GeometryMapper::SourceToDestinationClipRect(
    const PropertyTreeState& source_state,
    const PropertyTreeState& destination_state) {
  bool success = false;
  const FloatClipRect& result = SourceToDestinationClipRectInternal(
      source_state, destination_state, success);
  DCHECK(success);

  return result;
}

const FloatClipRect& GeometryMapper::SourceToDestinationClipRectInternal(
    const PropertyTreeState& source_state,
    const PropertyTreeState& destination_state,
    bool& success) {
  const FloatClipRect& result = LocalToAncestorClipRectInternal(
      source_state.Clip(), destination_state.Clip(),
      destination_state.Transform(), success);
  // Success if destinationState is an ancestor state.
  if (success)
    return result;

  // Otherwise first map to the lowest common ancestor, then map to
  // destination.
  const TransformPaintPropertyNode* lca_transform = LowestCommonAncestor(
      source_state.Transform(), destination_state.Transform());
  DCHECK(lca_transform);

  // Assume that the clip of destinationState is an ancestor of the clip of
  // sourceState and is under the space of lcaTransform. Otherwise
  // localToAncestorClipRectInternal() will fail.
  PropertyTreeState lca_state = destination_state;
  lca_state.SetTransform(lca_transform);

  const FloatClipRect& result2 = LocalToAncestorClipRectInternal(
      source_state.Clip(), lca_state.Clip(), lca_state.Transform(), success);
  if (!success) {
    if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
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
  if (!result2.IsInfinite()) {
    FloatRect rect = result2.Rect();
    AncestorToLocalRect(lca_transform, destination_state.Transform(), rect);
    FloatClipRect& temp = TempRect();
    temp.SetRect(rect);
    if (result2.HasRadius())
      temp.SetHasRadius();
    return temp;
  }
  return result2;
}

const FloatClipRect& GeometryMapper::LocalToAncestorClipRectInternal(
    const ClipPaintPropertyNode* descendant,
    const ClipPaintPropertyNode* ancestor_clip,
    const TransformPaintPropertyNode* ancestor_transform,
    bool& success) {
  FloatClipRect clip;
  if (descendant == ancestor_clip) {
    success = true;
    return InfiniteClip();
  }

  const ClipPaintPropertyNode* clip_node = descendant;
  Vector<const ClipPaintPropertyNode*> intermediate_nodes;

  GeometryMapperClipCache::ClipAndTransform clip_and_transform(
      ancestor_clip, ancestor_transform);
  // Iterate over the path from localState.clip to ancestorState.clip. Stop if
  // we've found a memoized (precomputed) clip for any particular node.
  while (clip_node && clip_node != ancestor_clip) {
    if (const FloatClipRect* cached_clip =
            clip_node->GetClipCache().GetCachedClip(clip_and_transform)) {
      clip = *cached_clip;
      break;
    }

    intermediate_nodes.push_back(clip_node);
    clip_node = clip_node->Parent();
  }
  if (!clip_node) {
    success = false;
    return InfiniteClip();
  }

  // Iterate down from the top intermediate node found in the previous loop,
  // computing and memoizing clip rects as we go.
  for (auto it = intermediate_nodes.rbegin(); it != intermediate_nodes.rend();
       ++it) {
    success = false;
    const TransformationMatrix& transform_matrix =
        LocalToAncestorMatrixInternal((*it)->LocalTransformSpace(),
                                      ancestor_transform, success);
    if (!success)
      return InfiniteClip();
    FloatRect mapped_rect = transform_matrix.MapRect((*it)->ClipRect().Rect());
    clip.Intersect(mapped_rect);
    if ((*it)->ClipRect().IsRounded())
      clip.SetHasRadius();

    (*it)->GetClipCache().SetCachedClip(clip_and_transform, clip);
  }

  success = true;

  const FloatClipRect* cached_clip =
      descendant->GetClipCache().GetCachedClip(clip_and_transform);
  DCHECK(cached_clip);
  CHECK(clip.HasRadius() == cached_clip->HasRadius());
  return *cached_clip;
}

const TransformationMatrix& GeometryMapper::LocalToAncestorMatrix(
    const TransformPaintPropertyNode* local_transform_node,
    const TransformPaintPropertyNode* ancestor_transform_node) {
  bool success = false;
  const auto& result = LocalToAncestorMatrixInternal(
      local_transform_node, ancestor_transform_node, success);
  DCHECK(success);
  return result;
}

const TransformationMatrix& GeometryMapper::LocalToAncestorMatrixInternal(
    const TransformPaintPropertyNode* local_transform_node,
    const TransformPaintPropertyNode* ancestor_transform_node,
    bool& success) {
  if (local_transform_node == ancestor_transform_node) {
    success = true;
    return IdentityMatrix();
  }

  const TransformPaintPropertyNode* transform_node = local_transform_node;
  Vector<const TransformPaintPropertyNode*> intermediate_nodes;
  TransformationMatrix transform_matrix;

  // Iterate over the path from localTransformNode to ancestorState.transform.
  // Stop if we've found a memoized (precomputed) transform for any particular
  // node.
  while (transform_node && transform_node != ancestor_transform_node) {
    if (const TransformationMatrix* cached_matrix =
            transform_node->GetTransformCache().GetCachedTransform(
                ancestor_transform_node)) {
      transform_matrix = *cached_matrix;
      break;
    }

    intermediate_nodes.push_back(transform_node);
    transform_node = transform_node->Parent();
  }
  if (!transform_node) {
    success = false;
    return IdentityMatrix();
  }

  // Iterate down from the top intermediate node found in the previous loop,
  // computing and memoizing transforms as we go.
  for (auto it = intermediate_nodes.rbegin(); it != intermediate_nodes.rend();
       it++) {
    TransformationMatrix local_transform_matrix = (*it)->Matrix();
    local_transform_matrix.ApplyTransformOrigin((*it)->Origin());

    // Flattening Lemma: flatten(A * flatten(B)) = flatten(flatten(A) * B).
    // goo.gl/DNKyOc. Thus we can flatten transformMatrix rather than
    // localTransformMatrix, because GeometryMapper only supports transforms
    // into a flattened destination space.
    if ((*it)->FlattensInheritedTransform())
      transform_matrix.FlattenTo2d();

    transform_matrix = transform_matrix * local_transform_matrix;
    (*it)->GetTransformCache().SetCachedTransform(ancestor_transform_node,
                                                  transform_matrix);
  }
  success = true;
  const TransformationMatrix* cached_matrix =
      local_transform_node->GetTransformCache().GetCachedTransform(
          ancestor_transform_node);
  DCHECK(cached_matrix);
  return *cached_matrix;
}

void GeometryMapper::ClearCache() {
  GeometryMapperTransformCache::ClearCache();
  GeometryMapperClipCache::ClearCache();
}

namespace {

template <typename NodeType>
unsigned NodeDepth(const NodeType* node) {
  unsigned depth = 0;
  while (node) {
    depth++;
    node = node->Parent();
  }
  return depth;
}

}  // namespace

template <typename NodeType>
const NodeType* GeometryMapper::LowestCommonAncestor(const NodeType* a,
                                                     const NodeType* b) {
  // Measure both depths.
  unsigned depth_a = NodeDepth(a);
  unsigned depth_b = NodeDepth(b);

  // Make it so depthA >= depthB.
  if (depth_a < depth_b) {
    std::swap(a, b);
    std::swap(depth_a, depth_b);
  }

  // Make it so depthA == depthB.
  while (depth_a > depth_b) {
    a = a->Parent();
    depth_a--;
  }

  // Walk up until we find the ancestor.
  while (a != b) {
    a = a->Parent();
    b = b->Parent();
  }
  return a;
}

// Explicitly instantiate the template for all supported types. This allows
// placing the template implementation in this .cpp file. See
// http://stackoverflow.com/a/488989 for more.
template const EffectPaintPropertyNode* GeometryMapper::LowestCommonAncestor(
    const EffectPaintPropertyNode*,
    const EffectPaintPropertyNode*);
template const TransformPaintPropertyNode* GeometryMapper::LowestCommonAncestor(
    const TransformPaintPropertyNode*,
    const TransformPaintPropertyNode*);
template const ClipPaintPropertyNode* GeometryMapper::LowestCommonAncestor(
    const ClipPaintPropertyNode*,
    const ClipPaintPropertyNode*);
template const ScrollPaintPropertyNode* GeometryMapper::LowestCommonAncestor(
    const ScrollPaintPropertyNode*,
    const ScrollPaintPropertyNode*);

}  // namespace blink
