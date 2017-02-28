// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/compositing/PropertyTreeManager.h"

#include "cc/layers/layer.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/GeometryMapper.h"
#include "platform/graphics/paint/ScrollPaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "public/platform/WebLayerScrollClient.h"
#include "third_party/skia/include/effects/SkColorFilterImageFilter.h"
#include "third_party/skia/include/effects/SkLumaColorFilter.h"

namespace blink {

namespace {

static constexpr int kInvalidNodeId = -1;
// cc's property trees use 0 for the root node (always non-null).
static constexpr int kRealRootNodeId = 0;
// cc allocates special nodes for root effects such as the device scale.
static constexpr int kSecondaryRootNodeId = 1;

}  // namespace

PropertyTreeManager::PropertyTreeManager(cc::PropertyTrees& propertyTrees,
                                         cc::Layer* rootLayer,
                                         int sequenceNumber)
    : m_propertyTrees(propertyTrees),
      m_rootLayer(rootLayer),
      m_sequenceNumber(sequenceNumber) {
  setupRootTransformNode();
  setupRootClipNode();
  setupRootEffectNode();
  setupRootScrollNode();
}

cc::TransformTree& PropertyTreeManager::transformTree() {
  return m_propertyTrees.transform_tree;
}

cc::ClipTree& PropertyTreeManager::clipTree() {
  return m_propertyTrees.clip_tree;
}

cc::EffectTree& PropertyTreeManager::effectTree() {
  return m_propertyTrees.effect_tree;
}

cc::ScrollTree& PropertyTreeManager::scrollTree() {
  return m_propertyTrees.scroll_tree;
}

const EffectPaintPropertyNode* PropertyTreeManager::currentEffectNode() const {
  return m_effectStack.back().effect;
}

void PropertyTreeManager::setupRootTransformNode() {
  // cc is hardcoded to use transform node index 1 for device scale and
  // transform.
  cc::TransformTree& transformTree = m_propertyTrees.transform_tree;
  transformTree.clear();
  m_propertyTrees.element_id_to_transform_node_index.clear();
  cc::TransformNode& transformNode = *transformTree.Node(
      transformTree.Insert(cc::TransformNode(), kRealRootNodeId));
  DCHECK_EQ(transformNode.id, kSecondaryRootNodeId);
  transformNode.source_node_id = transformNode.parent_id;
  transformTree.SetTargetId(transformNode.id, kRealRootNodeId);
  transformTree.SetContentTargetId(transformNode.id, kRealRootNodeId);

  // TODO(jaydasika): We shouldn't set ToScreen and FromScreen of root
  // transform node here. They should be set while updating transform tree in
  // cc.
  float deviceScaleFactor =
      m_rootLayer->layer_tree_host()->device_scale_factor();
  gfx::Transform toScreen;
  toScreen.Scale(deviceScaleFactor, deviceScaleFactor);
  transformTree.SetToScreen(kRealRootNodeId, toScreen);
  gfx::Transform fromScreen;
  bool invertible = toScreen.GetInverse(&fromScreen);
  DCHECK(invertible);
  transformTree.SetFromScreen(kRealRootNodeId, fromScreen);
  transformTree.set_needs_update(true);

  m_transformNodeMap.set(TransformPaintPropertyNode::root(), transformNode.id);
  m_rootLayer->SetTransformTreeIndex(transformNode.id);
}

void PropertyTreeManager::setupRootClipNode() {
  // cc is hardcoded to use clip node index 1 for viewport clip.
  cc::ClipTree& clipTree = m_propertyTrees.clip_tree;
  clipTree.clear();
  m_propertyTrees.layer_id_to_clip_node_index.clear();
  cc::ClipNode& clipNode =
      *clipTree.Node(clipTree.Insert(cc::ClipNode(), kRealRootNodeId));
  DCHECK_EQ(clipNode.id, kSecondaryRootNodeId);

  clipNode.resets_clip = true;
  clipNode.owning_layer_id = m_rootLayer->id();
  clipNode.clip_type = cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP;
  clipNode.clip = gfx::RectF(
      gfx::SizeF(m_rootLayer->layer_tree_host()->device_viewport_size()));
  clipNode.transform_id = kRealRootNodeId;
  clipNode.target_transform_id = kRealRootNodeId;
  clipNode.target_effect_id = kSecondaryRootNodeId;
  m_propertyTrees.layer_id_to_clip_node_index[clipNode.owning_layer_id] =
      clipNode.id;

  m_clipNodeMap.set(ClipPaintPropertyNode::root(), clipNode.id);
  m_rootLayer->SetClipTreeIndex(clipNode.id);
}

void PropertyTreeManager::setupRootEffectNode() {
  // cc is hardcoded to use effect node index 1 for root render surface.
  cc::EffectTree& effectTree = m_propertyTrees.effect_tree;
  effectTree.clear();
  m_propertyTrees.layer_id_to_effect_node_index.clear();
  m_propertyTrees.element_id_to_effect_node_index.clear();
  cc::EffectNode& effectNode =
      *effectTree.Node(effectTree.Insert(cc::EffectNode(), kInvalidNodeId));
  DCHECK_EQ(effectNode.id, kSecondaryRootNodeId);
  effectNode.owning_layer_id = m_rootLayer->id();
  effectNode.transform_id = kRealRootNodeId;
  effectNode.clip_id = kSecondaryRootNodeId;
  effectNode.has_render_surface = true;
  m_propertyTrees.layer_id_to_effect_node_index[effectNode.owning_layer_id] =
      effectNode.id;

  m_effectStack.push_back(
      BlinkEffectAndCcIdPair{EffectPaintPropertyNode::root(), effectNode.id});
  m_rootLayer->SetEffectTreeIndex(effectNode.id);
}

void PropertyTreeManager::setupRootScrollNode() {
  cc::ScrollTree& scrollTree = m_propertyTrees.scroll_tree;
  scrollTree.clear();
  m_propertyTrees.layer_id_to_scroll_node_index.clear();
  m_propertyTrees.element_id_to_scroll_node_index.clear();
  cc::ScrollNode& scrollNode =
      *scrollTree.Node(scrollTree.Insert(cc::ScrollNode(), kRealRootNodeId));
  DCHECK_EQ(scrollNode.id, kSecondaryRootNodeId);
  scrollNode.owning_layer_id = m_rootLayer->id();
  scrollNode.transform_id = kSecondaryRootNodeId;
  m_propertyTrees.layer_id_to_scroll_node_index[scrollNode.owning_layer_id] =
      scrollNode.id;

  m_scrollNodeMap.set(ScrollPaintPropertyNode::root(), scrollNode.id);
  m_rootLayer->SetScrollTreeIndex(scrollNode.id);
}

int PropertyTreeManager::ensureCompositorTransformNode(
    const TransformPaintPropertyNode* transformNode) {
  DCHECK(transformNode);
  // TODO(crbug.com/645615): Remove the failsafe here.
  if (!transformNode)
    return kSecondaryRootNodeId;

  auto it = m_transformNodeMap.find(transformNode);
  if (it != m_transformNodeMap.end())
    return it->value;

  scoped_refptr<cc::Layer> dummyLayer = cc::Layer::Create();
  int parentId = ensureCompositorTransformNode(transformNode->parent());
  int id = transformTree().Insert(cc::TransformNode(), parentId);

  cc::TransformNode& compositorNode = *transformTree().Node(id);
  transformTree().SetTargetId(id, kRealRootNodeId);
  transformTree().SetContentTargetId(id, kRealRootNodeId);
  compositorNode.source_node_id = parentId;

  FloatPoint3D origin = transformNode->origin();
  compositorNode.pre_local.matrix().setTranslate(-origin.x(), -origin.y(),
                                                 -origin.z());
  compositorNode.local.matrix() =
      TransformationMatrix::toSkMatrix44(transformNode->matrix());
  compositorNode.post_local.matrix().setTranslate(origin.x(), origin.y(),
                                                  origin.z());
  compositorNode.needs_local_transform_update = true;
  compositorNode.flattens_inherited_transform =
      transformNode->flattensInheritedTransform();
  compositorNode.sorting_context_id = transformNode->renderingContextId();

  m_rootLayer->AddChild(dummyLayer);
  dummyLayer->SetTransformTreeIndex(id);
  dummyLayer->SetClipTreeIndex(kSecondaryRootNodeId);
  dummyLayer->SetEffectTreeIndex(kSecondaryRootNodeId);
  dummyLayer->SetScrollTreeIndex(kRealRootNodeId);
  dummyLayer->set_property_tree_sequence_number(m_sequenceNumber);
  CompositorElementId compositorElementId =
      transformNode->compositorElementId();
  if (compositorElementId) {
    m_propertyTrees.element_id_to_transform_node_index[compositorElementId] =
        id;
  }

  auto result = m_transformNodeMap.set(transformNode, id);
  DCHECK(result.isNewEntry);
  transformTree().set_needs_update(true);

  if (transformNode->scrollNode())
    updateScrollAndScrollTranslationNodes(transformNode);

  return id;
}

int PropertyTreeManager::ensureCompositorClipNode(
    const ClipPaintPropertyNode* clipNode) {
  DCHECK(clipNode);
  // TODO(crbug.com/645615): Remove the failsafe here.
  if (!clipNode)
    return kSecondaryRootNodeId;

  auto it = m_clipNodeMap.find(clipNode);
  if (it != m_clipNodeMap.end())
    return it->value;

  scoped_refptr<cc::Layer> dummyLayer = cc::Layer::Create();
  int parentId = ensureCompositorClipNode(clipNode->parent());
  int id = clipTree().Insert(cc::ClipNode(), parentId);

  cc::ClipNode& compositorNode = *clipTree().Node(id);
  compositorNode.owning_layer_id = dummyLayer->id();
  m_propertyTrees.layer_id_to_clip_node_index[compositorNode.owning_layer_id] =
      id;

  // TODO(jbroman): Don't discard rounded corners.
  compositorNode.clip = clipNode->clipRect().rect();
  compositorNode.transform_id =
      ensureCompositorTransformNode(clipNode->localTransformSpace());
  compositorNode.target_transform_id = kRealRootNodeId;
  compositorNode.target_effect_id = kSecondaryRootNodeId;
  compositorNode.clip_type = cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP;
  compositorNode.layers_are_clipped = true;
  compositorNode.layers_are_clipped_when_surfaces_disabled = true;

  m_rootLayer->AddChild(dummyLayer);
  dummyLayer->SetTransformTreeIndex(compositorNode.transform_id);
  dummyLayer->SetClipTreeIndex(id);
  dummyLayer->SetEffectTreeIndex(kSecondaryRootNodeId);
  dummyLayer->SetScrollTreeIndex(kRealRootNodeId);
  dummyLayer->set_property_tree_sequence_number(m_sequenceNumber);

  auto result = m_clipNodeMap.set(clipNode, id);
  DCHECK(result.isNewEntry);
  clipTree().set_needs_update(true);
  return id;
}

int PropertyTreeManager::ensureCompositorScrollNode(
    const ScrollPaintPropertyNode* scrollNode) {
  DCHECK(scrollNode);
  // TODO(crbug.com/645615): Remove the failsafe here.
  if (!scrollNode)
    return kSecondaryRootNodeId;

  auto it = m_scrollNodeMap.find(scrollNode);
  if (it != m_scrollNodeMap.end())
    return it->value;

  int parentId = ensureCompositorScrollNode(scrollNode->parent());
  int id = scrollTree().Insert(cc::ScrollNode(), parentId);

  cc::ScrollNode& compositorNode = *scrollTree().Node(id);
  compositorNode.scrollable = true;

  compositorNode.scroll_clip_layer_bounds.SetSize(scrollNode->clip().width(),
                                                  scrollNode->clip().height());
  compositorNode.bounds.SetSize(scrollNode->bounds().width(),
                                scrollNode->bounds().height());
  compositorNode.user_scrollable_horizontal =
      scrollNode->userScrollableHorizontal();
  compositorNode.user_scrollable_vertical =
      scrollNode->userScrollableVertical();
  compositorNode.main_thread_scrolling_reasons =
      scrollNode->mainThreadScrollingReasons();

  auto result = m_scrollNodeMap.set(scrollNode, id);
  DCHECK(result.isNewEntry);
  scrollTree().set_needs_update(true);

  return id;
}

void PropertyTreeManager::updateScrollAndScrollTranslationNodes(
    const TransformPaintPropertyNode* scrollOffsetNode) {
  DCHECK(scrollOffsetNode->scrollNode());
  int scrollNodeId = ensureCompositorScrollNode(scrollOffsetNode->scrollNode());
  auto& compositorScrollNode = *scrollTree().Node(scrollNodeId);
  int transformNodeId = ensureCompositorTransformNode(scrollOffsetNode);
  auto& compositorTransformNode = *transformTree().Node(transformNodeId);

  auto compositorElementId = scrollOffsetNode->compositorElementId();
  if (compositorElementId) {
    compositorScrollNode.element_id = compositorElementId;
    m_propertyTrees.element_id_to_scroll_node_index[compositorElementId] =
        scrollNodeId;
  }

  compositorScrollNode.transform_id = transformNodeId;
  // TODO(pdr): Set the scroll node's non_fast_scrolling_region value.

  // Blink creates a 2d transform node just for scroll offset whereas cc's
  // transform node has a special scroll offset field. To handle this we adjust
  // cc's transform node to remove the 2d scroll translation and instead set the
  // scroll_offset field.
  auto scrollOffsetSize = scrollOffsetNode->matrix().to2DTranslation();
  auto scrollOffset =
      gfx::ScrollOffset(-scrollOffsetSize.width(), -scrollOffsetSize.height());
  DCHECK(compositorTransformNode.local.IsIdentityOr2DTranslation());
  compositorTransformNode.scroll_offset = scrollOffset;
  compositorTransformNode.local.MakeIdentity();
  compositorTransformNode.scrolls = true;
  transformTree().set_needs_update(true);
  // TODO(pdr): Because of a layer dependancy, the scroll tree scroll offset is
  // set in updateLayerScrollMapping but that should occur here.
}

void PropertyTreeManager::updateLayerScrollMapping(
    cc::Layer* layer,
    const TransformPaintPropertyNode* transform) {
  auto* enclosingScrollNode = transform->findEnclosingScrollNode();
  int scrollNodeId = ensureCompositorScrollNode(enclosingScrollNode);
  layer->SetScrollTreeIndex(scrollNodeId);
  int layerId = layer->id();
  m_propertyTrees.layer_id_to_scroll_node_index[layerId] = scrollNodeId;

  if (!transform->isScrollTranslation())
    return;

  auto& compositorScrollNode = *scrollTree().Node(scrollNodeId);

  // TODO(pdr): Remove the scroll node's owning_layer_id. This approach of
  // setting owning_layer_id only when it is not set lets us maintain a 1:1
  // mapping from layer to scroll node.
  if (compositorScrollNode.owning_layer_id == cc::Layer::INVALID_ID) {
    compositorScrollNode.owning_layer_id = layerId;
    auto& compositorTransformNode =
        *transformTree().Node(compositorScrollNode.transform_id);
    // TODO(pdr): Set this in updateScrollAndScrollTranslationNodes once the
    // layer id is no longer needed.
    scrollTree().SetScrollOffset(layerId,
                                 compositorTransformNode.scroll_offset);
    if (auto* scrollClient = enclosingScrollNode->scrollClient()) {
      layer->set_did_scroll_callback(
          base::Bind(&blink::WebLayerScrollClient::didScroll,
                     base::Unretained(scrollClient)));
    }
  }
}

int PropertyTreeManager::switchToEffectNode(
    const EffectPaintPropertyNode& nextEffect) {
  const EffectPaintPropertyNode* ancestor =
      GeometryMapper::lowestCommonAncestor(currentEffectNode(), &nextEffect);
  DCHECK(ancestor) << "Malformed effect tree. All nodes must be descendant of "
                      "EffectPaintPropertyNode::root().";
  while (currentEffectNode() != ancestor)
    m_effectStack.pop_back();

  // Now the current effect is the lowest common ancestor of previous effect
  // and the next effect. That implies it is an existing node that already has
  // at least one paint chunk or child effect, and we are going to either attach
  // another paint chunk or child effect to it. We can no longer omit render
  // surface for it even for opacity-only nodes.
  // See comments in PropertyTreeManager::buildEffectNodesRecursively().
  // TODO(crbug.com/504464): Remove premature optimization here.
  if (currentEffectNode() && currentEffectNode()->opacity() != 1.f) {
    effectTree()
        .Node(getCurrentCompositorEffectNodeIndex())
        ->has_render_surface = true;
  }

  buildEffectNodesRecursively(&nextEffect);

  return getCurrentCompositorEffectNodeIndex();
}

void PropertyTreeManager::buildEffectNodesRecursively(
    const EffectPaintPropertyNode* nextEffect) {
  if (nextEffect == currentEffectNode())
    return;
  DCHECK(nextEffect);

  buildEffectNodesRecursively(nextEffect->parent());
  DCHECK_EQ(nextEffect->parent(), currentEffectNode());

#if DCHECK_IS_ON()
  DCHECK(!m_effectNodesConverted.contains(nextEffect))
      << "Malformed paint artifact. Paint chunks under the same effect should "
         "be contiguous.";
  m_effectNodesConverted.insert(nextEffect);
#endif

  // An effect node can't omit render surface if it has child with exotic
  // blending mode. See comments below for more detail.
  // TODO(crbug.com/504464): Remove premature optimization here.
  if (nextEffect->blendMode() != SkBlendMode::kSrcOver) {
    effectTree()
        .Node(getCurrentCompositorEffectNodeIndex())
        ->has_render_surface = true;
  }

  // We currently create dummy layers to host effect nodes and corresponding
  // render surfaces. This should be removed once cc implements better support
  // for freestanding property trees.
  scoped_refptr<cc::Layer> dummyLayer = nextEffect->ensureDummyLayer();
  m_rootLayer->AddChild(dummyLayer);

  int outputClipId = ensureCompositorClipNode(nextEffect->outputClip());

  cc::EffectNode& effectNode = *effectTree().Node(effectTree().Insert(
      cc::EffectNode(), getCurrentCompositorEffectNodeIndex()));
  effectNode.owning_layer_id = dummyLayer->id();
  effectNode.clip_id = outputClipId;
  // Every effect is supposed to have render surface enabled for grouping,
  // but we can get away without one if the effect is opacity-only and has only
  // one compositing child with kSrcOver blend mode. This is both for
  // optimization and not introducing sub-pixel differences in layout tests.
  // See PropertyTreeManager::switchToEffectNode() and above where we
  // retrospectively enable render surface when more than one compositing child
  // or a child with exotic blend mode is detected.
  // TODO(crbug.com/504464): There is ongoing work in cc to delay render surface
  // decision until later phase of the pipeline. Remove premature optimization
  // here once the work is ready.
  if (!nextEffect->filter().isEmpty() ||
      nextEffect->blendMode() != SkBlendMode::kSrcOver)
    effectNode.has_render_surface = true;
  effectNode.opacity = nextEffect->opacity();
  if (nextEffect->colorFilter() != ColorFilterNone) {
    // Currently color filter is only used by SVG masks.
    // We are cutting corner here by support only specific configuration.
    DCHECK(nextEffect->colorFilter() == ColorFilterLuminanceToAlpha);
    DCHECK(nextEffect->blendMode() == SkBlendMode::kDstIn);
    DCHECK(nextEffect->filter().isEmpty());
    effectNode.filters.Append(cc::FilterOperation::CreateReferenceFilter(
        SkColorFilterImageFilter::Make(SkLumaColorFilter::Make(), nullptr)));
  } else {
    effectNode.filters = nextEffect->filter().asCcFilterOperations();
  }
  effectNode.blend_mode = nextEffect->blendMode();
  m_propertyTrees.layer_id_to_effect_node_index[effectNode.owning_layer_id] =
      effectNode.id;
  CompositorElementId compositorElementId = nextEffect->compositorElementId();
  if (compositorElementId) {
    m_propertyTrees.element_id_to_effect_node_index[compositorElementId] =
        effectNode.id;
  }
  m_effectStack.push_back(BlinkEffectAndCcIdPair{nextEffect, effectNode.id});

  dummyLayer->set_property_tree_sequence_number(m_sequenceNumber);
  dummyLayer->SetTransformTreeIndex(kSecondaryRootNodeId);
  dummyLayer->SetClipTreeIndex(outputClipId);
  dummyLayer->SetEffectTreeIndex(effectNode.id);
  dummyLayer->SetScrollTreeIndex(kRealRootNodeId);
}

}  // namespace blink
