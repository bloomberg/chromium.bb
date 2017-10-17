// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/GeometryMapper.h"

#include "platform/geometry/LayoutRect.h"
#include "platform/runtime_enabled_features.h"

namespace blink {

const TransformationMatrix& GeometryMapper::SourceToDestinationProjection(
    const TransformPaintPropertyNode* source,
    const TransformPaintPropertyNode* destination) {
  DCHECK(source && destination);
  bool success = false;
  const auto& result =
      SourceToDestinationProjectionInternal(source, destination, success);
  DCHECK(success);
  return result;
}

// Returns flatten(destination_to_screen)^-1 * flatten(source_to_screen)
//
// In case that source and destination are coplanar in tree hierarchy [1],
// computes destination_to_plane_root ^ -1 * source_to_plane_root.
// It can be proved that [2] the result will be the same (except numerical
// errors) when the plane root has invertible screen projection, and this
// offers fallback definition when plane root is singular. For example:
// <div style="transform:rotateY(90deg); overflow:scroll;">
//   <div id="A" style="opacity:0.5;">
//     <div id="B" style="position:absolute;"></div>
//   </div>
// </div>
// Both A and B have non-invertible screen projection, nevertheless it is
// useful to define projection between A and B. Say, the transform may be
// animated in compositor thus become visible.
// As SPv1 treats 3D transforms as compositing trigger, that implies mappings
// within the same compositing layer can only contain 2D transforms, thus
// intra-composited-layer queries are guaranteed to be handled correctly.
//
// [1] As defined by that all local transforms between source and some common
//     ancestor 'plane root' and all local transforms between the destination
//     and the plane root being flat.
// [2] destination_to_screen = plane_root_to_screen * destination_to_plane_root
//     source_to_screen = plane_root_to_screen * source_to_plane_root
//     output = flatten(destination_to_screen)^-1 * flatten(source_to_screen)
//     = flatten(plane_root_to_screen * destination_to_plane_root)^-1 *
//       flatten(plane_root_to_screen * source_to_plane_root)
//     Because both destination_to_plane_root and source_to_plane_root are
//     already flat,
//     = flatten(plane_root_to_screen * flatten(destination_to_plane_root))^-1 *
//       flatten(plane_root_to_screen * flatten(source_to_plane_root))
//     By flatten lemma [3] flatten(A * flatten(B)) = flatten(A) * flatten(B),
//     = flatten(destination_to_plane_root)^-1 *
//       flatten(plane_root_to_screen)^-1 *
//       flatten(plane_root_to_screen) * flatten(source_to_plane_root)
//     If flatten(plane_root_to_screen) is invertible, they cancel out:
//     = flatten(destination_to_plane_root)^-1 * flatten(source_to_plane_root)
//     = destination_to_plane_root^-1 * source_to_plane_root
// [3] Flatten lemma: https://goo.gl/DNKyOc
const TransformationMatrix&
GeometryMapper::SourceToDestinationProjectionInternal(
    const TransformPaintPropertyNode* source,
    const TransformPaintPropertyNode* destination,
    bool& success) {
  DCHECK(source && destination);
  DEFINE_STATIC_LOCAL(TransformationMatrix, identity, (TransformationMatrix()));
  DEFINE_STATIC_LOCAL(TransformationMatrix, temp, (TransformationMatrix()));

  if (source == destination) {
    success = true;
    return identity;
  }

  const GeometryMapperTransformCache& source_cache =
      source->GetTransformCache();
  const GeometryMapperTransformCache& destination_cache =
      destination->GetTransformCache();

  // Case 1: Check if source and destination are known to be coplanar.
  // Even if destination may have invertible screen projection,
  // this formula is likely to be numerically more stable.
  if (source_cache.plane_root() == destination_cache.plane_root()) {
    success = true;
    if (source == destination_cache.plane_root())
      return destination_cache.from_plane_root();
    if (destination == source_cache.plane_root())
      return source_cache.to_plane_root();
    temp = destination_cache.from_plane_root();
    temp.Multiply(source_cache.to_plane_root());
    return temp;
  }

  // Case 2: Check if we can fallback to the canonical definition of
  // flatten(destination_to_screen)^-1 * flatten(source_to_screen)
  // If flatten(destination_to_screen)^-1 is invalid, we are out of luck.
  if (!destination_cache.projection_from_screen_is_valid()) {
    success = false;
    return identity;
  }

  // Case 3: Compute:
  // flatten(destination_to_screen)^-1 * flatten(source_to_screen)
  const auto* root = TransformPaintPropertyNode::Root();
  success = true;
  if (source == root)
    return destination_cache.projection_from_screen();
  if (destination == root) {
    temp = source_cache.to_screen();
  } else {
    temp = destination_cache.projection_from_screen();
    temp.Multiply(source_cache.to_screen());
  }
  temp.FlattenTo2d();
  return temp;
}

void GeometryMapper::SourceToDestinationRect(
    const TransformPaintPropertyNode* source_transform_node,
    const TransformPaintPropertyNode* destination_transform_node,
    FloatRect& mapping_rect) {
  bool success = false;
  const TransformationMatrix& source_to_destination =
      SourceToDestinationProjectionInternal(
          source_transform_node, destination_transform_node, success);
  mapping_rect =
      success ? source_to_destination.MapRect(mapping_rect) : FloatRect();
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

  const auto& transform_matrix = SourceToDestinationProjectionInternal(
      local_state.Transform(), ancestor_state.Transform(), success);
  if (!success) {
    // A failure implies either source-to-plane or destination-to-plane being
    // singular. A notable example of singular source-to-plane from valid CSS:
    // <div id="plane" style="transform:rotateY(180deg)">
    //   <div style="overflow:overflow">
    //     <div id="ancestor" style="opacity:0.5;">
    //       <div id="local" style="position:absolute; transform:scaleX(0);">
    //       </div>
    //     </div>
    //   </div>
    // </div>
    // Either way, the element won't be renderable thus returning empty rect.
    success = true;
    rect_to_map = FloatClipRect(FloatRect());
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
    // On SPv1* we may fail when the paint invalidation container creates an
    // overflow clip (in ancestorState) which is not in localState of an
    // out-of-flow positioned descendant. See crbug.com/513108 and layout test
    // compositing/overflow/handle-non-ancestor-clip-parent.html (run with
    // --enable-prefer-compositing-to-lcd-text) for details.
    // Ignore it for SPv1* for now.
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

    DCHECK(effect->OutputClip());
    PropertyTreeState transform_and_clip_state(effect->LocalTransformSpace(),
                                               effect->OutputClip(), nullptr);
    LocalToAncestorVisualRectInternal(last_transform_and_clip_state,
                                      transform_and_clip_state, mapping_rect,
                                      success);
    if (!success) {
      success = true;
      mapping_rect = FloatClipRect(FloatRect());
      return;
    }

    mapping_rect.SetRect(effect->MapRect(mapping_rect.Rect()));
    last_transform_and_clip_state = transform_and_clip_state;
  }

  PropertyTreeState final_transform_and_clip_state(
      ancestor_state.Transform(), ancestor_state.Clip(), nullptr);
  LocalToAncestorVisualRectInternal(last_transform_and_clip_state,
                                    final_transform_and_clip_state,
                                    mapping_rect, success);
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

const FloatClipRect& GeometryMapper::LocalToAncestorClipRectInternal(
    const ClipPaintPropertyNode* descendant,
    const ClipPaintPropertyNode* ancestor_clip,
    const TransformPaintPropertyNode* ancestor_transform,
    bool& success) {
  DEFINE_STATIC_LOCAL(FloatClipRect, infinite, (FloatClipRect()));
  DEFINE_STATIC_LOCAL(FloatClipRect, empty, (FloatRect()));

  FloatClipRect clip;
  if (descendant == ancestor_clip) {
    success = true;
    return infinite;
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
    if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
      // On SPv1 we may fail when the paint invalidation container creates an
      // overflow clip (in ancestorState) which is not in localState of an
      // out-of-flow positioned descendant. See crbug.com/513108 and layout
      // test compositing/overflow/handle-non-ancestor-clip-parent.html (run
      // with --enable-prefer-compositing-to-lcd-text) for details.
      // Ignore it for SPv1 for now.
      success = true;
    }
    return infinite;
  }

  // Iterate down from the top intermediate node found in the previous loop,
  // computing and memoizing clip rects as we go.
  for (auto it = intermediate_nodes.rbegin(); it != intermediate_nodes.rend();
       ++it) {
    const TransformationMatrix& transform_matrix =
        SourceToDestinationProjectionInternal((*it)->LocalTransformSpace(),
                                              ancestor_transform, success);
    if (!success) {
      success = true;
      return empty;
    }
    FloatRect mapped_rect = transform_matrix.MapRect((*it)->ClipRect().Rect());
    clip.Intersect(mapped_rect);
    if ((*it)->ClipRect().IsRounded())
      clip.SetHasRadius();

    (*it)->GetClipCache().SetCachedClip(clip_and_transform, clip);
  }

  const FloatClipRect* cached_clip =
      descendant->GetClipCache().GetCachedClip(clip_and_transform);
  DCHECK(cached_clip);
  CHECK(clip.HasRadius() == cached_clip->HasRadius());

  success = true;
  return *cached_clip;
}

void GeometryMapper::ClearCache() {
  GeometryMapperTransformCache::ClearCache();
  GeometryMapperClipCache::ClearCache();
}

}  // namespace blink
