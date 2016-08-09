// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/compositing/PaintArtifactCompositor.h"

#include "cc/layers/content_layer_client.h"
#include "cc/layers/layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/playback/display_item_list.h"
#include "cc/playback/display_item_list_settings.h"
#include "cc/playback/drawing_display_item.h"
#include "cc/playback/transform_display_item.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/transform_node.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/ForeignLayerDisplayItem.h"
#include "platform/graphics/paint/PaintArtifact.h"
#include "platform/graphics/paint/PropertyTreeState.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebLayer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/skia_util.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/PtrUtil.h"
#include <algorithm>
#include <memory>
#include <utility>

namespace blink {

class PaintArtifactCompositor::ContentLayerClientImpl : public cc::ContentLayerClient {
    WTF_MAKE_NONCOPYABLE(ContentLayerClientImpl);
    USING_FAST_MALLOC(ContentLayerClientImpl);
public:
    ContentLayerClientImpl(scoped_refptr<cc::DisplayItemList> list, const gfx::Rect& paintableRegion)
        : m_ccDisplayItemList(std::move(list)), m_paintableRegion(paintableRegion) { }

    // cc::ContentLayerClient
    gfx::Rect PaintableRegion() override { return m_paintableRegion; }
    scoped_refptr<cc::DisplayItemList> PaintContentsToDisplayList(PaintingControlSetting) override
    {
        return m_ccDisplayItemList;
    }
    bool FillsBoundsCompletely() const override { return false; }
    size_t GetApproximateUnsharedMemoryUsage() const override
    {
        // TODO(jbroman): Actually calculate memory usage.
        return 0;
    }

private:
    scoped_refptr<cc::DisplayItemList> m_ccDisplayItemList;
    gfx::Rect m_paintableRegion;
};

PaintArtifactCompositor::PaintArtifactCompositor()
{
    if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled())
        return;
    m_rootLayer = cc::Layer::Create();
    m_webLayer = wrapUnique(Platform::current()->compositorSupport()->createLayerFromCCLayer(m_rootLayer.get()));
}

PaintArtifactCompositor::~PaintArtifactCompositor()
{
}

namespace {

static void appendDisplayItemToCcDisplayItemList(const DisplayItem& displayItem, cc::DisplayItemList* list)
{
    if (DisplayItem::isDrawingType(displayItem.getType())) {
        const SkPicture* picture = static_cast<const DrawingDisplayItem&>(displayItem).picture();
        if (!picture)
            return;
        gfx::Rect bounds = gfx::SkIRectToRect(picture->cullRect().roundOut());
        list->CreateAndAppendItem<cc::DrawingDisplayItem>(bounds, sk_ref_sp(picture));
    }
}

static scoped_refptr<cc::DisplayItemList> recordPaintChunk(const PaintArtifact& artifact, const PaintChunk& chunk, const gfx::Rect& combinedBounds)
{
    cc::DisplayItemListSettings settings;
    scoped_refptr<cc::DisplayItemList> list = cc::DisplayItemList::Create(
        gfx::Rect(combinedBounds.size()), settings);

    gfx::Transform translation;
    translation.Translate(-combinedBounds.x(), -combinedBounds.y());
    // TODO(jbroman, wkorman): What visual rectangle is wanted here?
    list->CreateAndAppendItem<cc::TransformDisplayItem>(gfx::Rect(), translation);

    const DisplayItemList& displayItems = artifact.getDisplayItemList();
    for (const auto& displayItem : displayItems.itemsInPaintChunk(chunk))
        appendDisplayItemToCcDisplayItemList(displayItem, list.get());

    list->CreateAndAppendItem<cc::EndTransformDisplayItem>(gfx::Rect());

    list->Finalize();
    return list;
}

static gfx::Transform transformToTransformSpace(const TransformPaintPropertyNode* currentSpace, const TransformPaintPropertyNode* targetSpace)
{
    TransformationMatrix matrix;
    while (currentSpace != targetSpace) {
        TransformationMatrix localMatrix = currentSpace->matrix();
        localMatrix.applyTransformOrigin(currentSpace->origin());
        matrix = localMatrix * matrix;
        currentSpace = currentSpace->parent();
    }
    return gfx::Transform(TransformationMatrix::toSkMatrix44(matrix));
}

const TransformPaintPropertyNode* localTransformSpace(const ClipPaintPropertyNode* clip)
{
    return clip ? clip->localTransformSpace() : nullptr;
}

scoped_refptr<cc::Layer> createClipLayer(const ClipPaintPropertyNode* node)
{
    // TODO(jbroman): Handle rounded-rect clips.
    // TODO(jbroman): Handle clips of non-integer size.
    gfx::RectF clipRect = node->clipRect().rect();

    // TODO(jbroman): This, and the similar logic in
    // PaintArtifactCompositor::update, will need to be updated to account for
    // other kinds of intermediate layers, such as those that apply effects.
    // TODO(jbroman): This assumes that the transform space of this node's
    // parent is an ancestor of this node's transform space. That's not
    // necessarily true, and this should be fixed. crbug.com/597156
    gfx::Transform transform = transformToTransformSpace(localTransformSpace(node), localTransformSpace(node->parent()));
    gfx::Vector2dF offset = clipRect.OffsetFromOrigin();
    transform.Translate(offset.x(), offset.y());
    if (node->parent()) {
        FloatPoint offsetDueToParentClipOffset = node->parent()->clipRect().rect().location();
        gfx::Transform undoClipOffset;
        undoClipOffset.Translate(-offsetDueToParentClipOffset.x(), -offsetDueToParentClipOffset.y());
        transform.ConcatTransform(undoClipOffset);
    }

    scoped_refptr<cc::Layer> layer = cc::Layer::Create();
    layer->SetIsDrawable(false);
    layer->SetMasksToBounds(true);
    layer->SetPosition(gfx::PointF());
    layer->SetBounds(gfx::ToRoundedSize(clipRect.size()));
    layer->SetTransform(transform);
    return layer;
}

class ClipLayerManager {
public:
    ClipLayerManager(cc::Layer* rootLayer)
    {
        m_clipLayers.append(NodeLayerPair(nullptr, rootLayer));
    }

    cc::Layer* switchToNewClipLayer(const ClipPaintPropertyNode* clip)
    {
        // Walk up to the nearest common ancestor.
        const auto* ancestor = propertyTreeNearestCommonAncestor<ClipPaintPropertyNode>(clip, m_clipLayers.last().first);
        while (m_clipLayers.last().first != ancestor)
            m_clipLayers.removeLast();

        // If the new one was an ancestor, we're done.
        cc::Layer* ancestorClipLayer = m_clipLayers.last().second;
        if (ancestor == clip)
            return ancestorClipLayer;

        // Otherwise, we need to build new clip layers.
        // We do this from the bottom up.
        size_t numExistingClipLayers = m_clipLayers.size();
        scoped_refptr<cc::Layer> childLayer;
        do {
            scoped_refptr<cc::Layer> clipLayer = createClipLayer(clip);
            m_clipLayers.append(NodeLayerPair(clip, clipLayer.get()));
            if (childLayer)
                clipLayer->AddChild(std::move(childLayer));
            childLayer = std::move(clipLayer);
            clip = clip->parent();
        } while (ancestor != clip);
        ancestorClipLayer->AddChild(std::move(childLayer));

        // Rearrange the new clip layers to be in top-down order, like they
        // should be.
        std::reverse(m_clipLayers.begin() + numExistingClipLayers, m_clipLayers.end());

        // Return the last (bottom-most) clip layer.
        return m_clipLayers.last().second;
    }

private:
    using NodeLayerPair = std::pair<const ClipPaintPropertyNode*, cc::Layer*>;
    Vector<NodeLayerPair, 16> m_clipLayers;
};

scoped_refptr<cc::Layer> foreignLayerForPaintChunk(const PaintArtifact& paintArtifact, const PaintChunk& paintChunk, gfx::Vector2dF& layerOffset)
{
    if (paintChunk.size() != 1)
        return nullptr;

    const auto& displayItem = paintArtifact.getDisplayItemList()[paintChunk.beginIndex];
    if (!displayItem.isForeignLayer())
        return nullptr;

    const auto& foreignLayerDisplayItem = static_cast<const ForeignLayerDisplayItem&>(displayItem);
    layerOffset = gfx::Vector2dF(foreignLayerDisplayItem.location().x(), foreignLayerDisplayItem.location().y());
    scoped_refptr<cc::Layer> layer = foreignLayerDisplayItem.layer();
    layer->SetBounds(foreignLayerDisplayItem.bounds());
    layer->SetIsDrawable(true);
    return layer;
}


constexpr int kRealRootNodeId = 0;
constexpr int kSecondaryRootNodeId = 1;
constexpr int kPropertyTreeSequenceNumber = 1;

// Creates a minimal set of property trees for the compositor.
void setMinimalPropertyTrees(cc::PropertyTrees* propertyTrees, int ownerId)
{
    // cc is hardcoded to use transform node index 1 for device scale and transform.
    cc::TransformTree& transformTree = propertyTrees->transform_tree;
    transformTree.clear();
    cc::TransformNode& transformNode = *transformTree.Node(transformTree.Insert(cc::TransformNode(), kRealRootNodeId));
    DCHECK_EQ(transformNode.id, kSecondaryRootNodeId);
    transformNode.source_node_id = transformNode.parent_id;
    transformTree.SetTargetId(transformNode.id, kRealRootNodeId);
    transformTree.SetContentTargetId(transformNode.id, kRealRootNodeId);

    // cc is hardcoded to use clip node index 1 for viewport clip.
    cc::ClipTree& clipTree = propertyTrees->clip_tree;
    clipTree.clear();
    cc::ClipNode& clipNode = *clipTree.Node(clipTree.Insert(cc::ClipNode(), kRealRootNodeId));
    DCHECK_EQ(clipNode.id, kSecondaryRootNodeId);
    clipNode.owner_id = ownerId;

    // cc is hardcoded to use effect node index 1 for root render surface.
    cc::EffectTree& effectTree = propertyTrees->effect_tree;
    effectTree.clear();
    cc::EffectNode& effectNode = *effectTree.Node(effectTree.Insert(cc::EffectNode(), kRealRootNodeId));
    DCHECK_EQ(effectNode.id, kSecondaryRootNodeId);
    effectNode.owner_id = ownerId;
    effectNode.clip_id = clipNode.id;
    effectNode.has_render_surface = true;

    cc::ScrollTree& scrollTree = propertyTrees->scroll_tree;
    scrollTree.clear();
}

} // namespace

void PaintArtifactCompositor::update(const PaintArtifact& paintArtifact)
{
    DCHECK(m_rootLayer);

    if (m_extraDataForTestingEnabled)
        m_extraDataForTesting = wrapUnique(new ExtraDataForTesting);

    // If the compositor is configured to expect using flat layer lists plus
    // property trees, then we should provide that format.
    cc::LayerTreeHost* host = m_rootLayer->layer_tree_host();
    const bool useLayerLists = host && host->settings().use_layer_lists;
    if (useLayerLists) {
        updateInLayerListMode(paintArtifact);
        return;
    }

    // TODO(jbroman): This should be incremental.
    m_rootLayer->RemoveAllChildren();
    m_contentLayerClients.clear();

    m_contentLayerClients.reserveCapacity(paintArtifact.paintChunks().size());
    ClipLayerManager clipLayerManager(m_rootLayer.get());
    for (const PaintChunk& paintChunk : paintArtifact.paintChunks()) {
        cc::Layer* parent = clipLayerManager.switchToNewClipLayer(paintChunk.properties.clip.get());

        gfx::Vector2dF layerOffset;
        scoped_refptr<cc::Layer> layer = layerForPaintChunk(paintArtifact, paintChunk, layerOffset);
        // TODO(jbroman): Same as above. This assumes the transform space of the current clip is
        // an ancestor of the chunk. It is not necessarily true. crbug.com/597156
        gfx::Transform transform = transformToTransformSpace(paintChunk.properties.transform.get(), localTransformSpace(paintChunk.properties.clip.get()));
        transform.Translate(layerOffset.x(), layerOffset.y());
        // If a clip was applied, its origin needs to be cancelled out in
        // this transform.
        if (const auto* clip = paintChunk.properties.clip.get()) {
            FloatPoint offsetDueToClipOffset = clip->clipRect().rect().location();
            gfx::Transform undoClipOffset;
            undoClipOffset.Translate(-offsetDueToClipOffset.x(), -offsetDueToClipOffset.y());
            transform.ConcatTransform(undoClipOffset);
        }
        layer->SetTransform(transform);
        layer->SetDoubleSided(!paintChunk.properties.backfaceHidden);
        layer->SetNeedsDisplay();
        parent->AddChild(layer);

        if (m_extraDataForTestingEnabled)
            m_extraDataForTesting->contentLayers.append(layer);
    }
}

scoped_refptr<cc::Layer> PaintArtifactCompositor::layerForPaintChunk(const PaintArtifact& paintArtifact, const PaintChunk& paintChunk, gfx::Vector2dF& layerOffset)
{
    DCHECK(paintChunk.size());

    // If the paint chunk is a foreign layer, just return that layer.
    if (scoped_refptr<cc::Layer> foreignLayer = foreignLayerForPaintChunk(paintArtifact, paintChunk, layerOffset))
        return foreignLayer;

    // The common case: create a layer for painted content.
    gfx::Rect combinedBounds = enclosingIntRect(paintChunk.bounds);
    scoped_refptr<cc::DisplayItemList> displayList = recordPaintChunk(paintArtifact, paintChunk, combinedBounds);
    std::unique_ptr<ContentLayerClientImpl> contentLayerClient = wrapUnique(
        new ContentLayerClientImpl(std::move(displayList), gfx::Rect(combinedBounds.size())));

    layerOffset = combinedBounds.OffsetFromOrigin();
    scoped_refptr<cc::PictureLayer> layer = cc::PictureLayer::Create(contentLayerClient.get());
    layer->SetBounds(combinedBounds.size());
    layer->SetIsDrawable(true);
    if (paintChunk.knownToBeOpaque)
        layer->SetContentsOpaque(true);
    m_contentLayerClients.append(std::move(contentLayerClient));
    return layer;
}

namespace {

class PropertyTreeManager {
    WTF_MAKE_NONCOPYABLE(PropertyTreeManager);
public:
    PropertyTreeManager(cc::PropertyTrees& propertyTrees, cc::Layer* rootLayer)
        : m_propertyTrees(propertyTrees)
        , m_rootLayer(rootLayer)
#if DCHECK_IS_ON()
        , m_isFirstEffectEver(true)
#endif
    {
        m_effectStack.append(BlinkEffectAndCcIdPair{nullptr, kSecondaryRootNodeId});
    }

    int compositorIdForTransformNode(const TransformPaintPropertyNode*);
    int compositorIdForClipNode(const ClipPaintPropertyNode*);
    int switchToEffectNode(const EffectPaintPropertyNode& nextEffect);
    int compositorIdForCurrentEffectNode() const { return m_effectStack.last().id; }

private:
    void buildEffectNodesRecursively(const EffectPaintPropertyNode* nextEffect);

    cc::TransformTree& transformTree() { return m_propertyTrees.transform_tree; }
    cc::ClipTree& clipTree() { return m_propertyTrees.clip_tree; }
    cc::EffectTree& effectTree() { return m_propertyTrees.effect_tree; }

    const EffectPaintPropertyNode* currentEffectNode() const { return m_effectStack.last().effect; }

    // Property trees which should be updated by the manager.
    cc::PropertyTrees& m_propertyTrees;

    // Layer to which transform "owner" layers should be added. These will not
    // have any actual children, but at present must exist in the tree.
    cc::Layer* m_rootLayer;

    // Maps from Blink-side property tree nodes to cc property node indices.
    HashMap<const TransformPaintPropertyNode*, int> m_transformNodeMap;
    HashMap<const ClipPaintPropertyNode*, int> m_clipNodeMap;

    struct BlinkEffectAndCcIdPair {
        const EffectPaintPropertyNode* effect;
        int id;
    };
    Vector<BlinkEffectAndCcIdPair> m_effectStack;

#if DCHECK_IS_ON()
    HashSet<const EffectPaintPropertyNode*> m_effectNodesConverted;
    bool m_isFirstEffectEver;
#endif
};

int PropertyTreeManager::compositorIdForTransformNode(const TransformPaintPropertyNode* transformNode)
{
    if (!transformNode)
        return kSecondaryRootNodeId;

    auto it = m_transformNodeMap.find(transformNode);
    if (it != m_transformNodeMap.end())
        return it->value;

    scoped_refptr<cc::Layer> dummyLayer = cc::Layer::Create();
    int parentId = compositorIdForTransformNode(transformNode->parent());
    int id = transformTree().Insert(cc::TransformNode(), parentId);

    cc::TransformNode& compositorNode = *transformTree().Node(id);
    transformTree().SetTargetId(id, kRealRootNodeId);
    transformTree().SetContentTargetId(id, kRealRootNodeId);
    compositorNode.source_node_id = parentId;

    FloatPoint3D origin = transformNode->origin();
    compositorNode.pre_local.matrix().setTranslate(
        -origin.x(), -origin.y(), -origin.z());
    compositorNode.local.matrix() = TransformationMatrix::toSkMatrix44(transformNode->matrix());
    compositorNode.post_local.matrix().setTranslate(
        origin.x(), origin.y(), origin.z());
    compositorNode.needs_local_transform_update = true;
    compositorNode.flattens_inherited_transform = transformNode->flattensInheritedTransform();
    compositorNode.sorting_context_id = transformNode->renderingContextID();

    m_rootLayer->AddChild(dummyLayer);
    dummyLayer->SetTransformTreeIndex(id);
    dummyLayer->SetClipTreeIndex(kSecondaryRootNodeId);
    dummyLayer->SetEffectTreeIndex(kSecondaryRootNodeId);
    dummyLayer->SetScrollTreeIndex(kRealRootNodeId);
    dummyLayer->set_property_tree_sequence_number(kPropertyTreeSequenceNumber);

    auto result = m_transformNodeMap.set(transformNode, id);
    DCHECK(result.isNewEntry);
    transformTree().set_needs_update(true);
    return id;
}

int PropertyTreeManager::compositorIdForClipNode(const ClipPaintPropertyNode* clipNode)
{
    if (!clipNode)
        return kSecondaryRootNodeId;

    auto it = m_clipNodeMap.find(clipNode);
    if (it != m_clipNodeMap.end())
        return it->value;

    scoped_refptr<cc::Layer> dummyLayer = cc::Layer::Create();
    int parentId = compositorIdForClipNode(clipNode->parent());
    int id = clipTree().Insert(cc::ClipNode(), parentId);

    cc::ClipNode& compositorNode = *clipTree().Node(id);
    compositorNode.owner_id = dummyLayer->id();

    // TODO(jbroman): Don't discard rounded corners.
    compositorNode.clip = clipNode->clipRect().rect();
    compositorNode.transform_id = compositorIdForTransformNode(clipNode->localTransformSpace());
    compositorNode.target_transform_id = kRealRootNodeId;
    compositorNode.applies_local_clip = true;
    compositorNode.layers_are_clipped = true;
    compositorNode.layers_are_clipped_when_surfaces_disabled = true;

    m_rootLayer->AddChild(dummyLayer);
    dummyLayer->SetTransformTreeIndex(compositorNode.transform_id);
    dummyLayer->SetClipTreeIndex(id);
    dummyLayer->SetEffectTreeIndex(kSecondaryRootNodeId);
    dummyLayer->SetScrollTreeIndex(kRealRootNodeId);
    dummyLayer->set_property_tree_sequence_number(kPropertyTreeSequenceNumber);

    auto result = m_clipNodeMap.set(clipNode, id);
    DCHECK(result.isNewEntry);
    clipTree().set_needs_update(true);
    return id;
}

unsigned depth(const EffectPaintPropertyNode* node)
{
    unsigned result = 0;
    for (; node; node = node->parent())
        result++;
    return result;
}

const EffectPaintPropertyNode* lowestCommonAncestor(const EffectPaintPropertyNode* nodeA, const EffectPaintPropertyNode* nodeB)
{
    // Optimized common case.
    if (nodeA == nodeB)
        return nodeA;

    unsigned depthA = depth(nodeA), depthB = depth(nodeB);
    while (depthA > depthB) {
        nodeA = nodeA->parent();
        depthA--;
    }
    while (depthB > depthA) {
        nodeB = nodeB->parent();
        depthB--;
    }
    DCHECK_EQ(depthA, depthB);
    while (nodeA != nodeB) {
        nodeA = nodeA->parent();
        nodeB = nodeB->parent();
    }
    return nodeA;
}

int PropertyTreeManager::switchToEffectNode(const EffectPaintPropertyNode& nextEffect)
{
    const EffectPaintPropertyNode* ancestor = lowestCommonAncestor(currentEffectNode(), &nextEffect);
    while (currentEffectNode() != ancestor)
        m_effectStack.removeLast();

#if DCHECK_IS_ON()
    DCHECK(m_isFirstEffectEver || currentEffectNode()) << "Malformed effect tree. Nodes in the same property tree should have common root.";
    m_isFirstEffectEver = false;
#endif
    buildEffectNodesRecursively(&nextEffect);

    return compositorIdForCurrentEffectNode();
}

void PropertyTreeManager::buildEffectNodesRecursively(const EffectPaintPropertyNode* nextEffect)
{
    if (nextEffect == currentEffectNode())
        return;
    DCHECK(nextEffect);

    buildEffectNodesRecursively(nextEffect->parent());
    DCHECK_EQ(nextEffect->parent(), currentEffectNode());

#if DCHECK_IS_ON()
    DCHECK(!m_effectNodesConverted.contains(nextEffect)) << "Malformed paint artifact. Paint chunks under the same effect should be contiguous.";
    m_effectNodesConverted.add(nextEffect);
#endif

    // We currently create dummy layers to host effect nodes and corresponding render surface.
    // This should be removed once cc implements better support for freestanding property trees.
    scoped_refptr<cc::Layer> dummyLayer = cc::Layer::Create();
    m_rootLayer->AddChild(dummyLayer);

    // Also cc assumes a clip node is always created by a layer that creates render surface.
    cc::ClipNode& dummyClip = *clipTree().Node(clipTree().Insert(cc::ClipNode(), kSecondaryRootNodeId));
    dummyClip.owner_id = dummyLayer->id();
    dummyClip.transform_id = kRealRootNodeId;
    dummyClip.target_transform_id = kRealRootNodeId;

    cc::EffectNode& effectNode = *effectTree().Node(effectTree().Insert(cc::EffectNode(), compositorIdForCurrentEffectNode()));
    effectNode.owner_id = dummyLayer->id();
    effectNode.clip_id = dummyClip.id;
    effectNode.has_render_surface = true;
    effectNode.opacity = nextEffect->opacity();
    m_effectStack.append(BlinkEffectAndCcIdPair{nextEffect, effectNode.id});

    dummyLayer->set_property_tree_sequence_number(kPropertyTreeSequenceNumber);
    dummyLayer->SetTransformTreeIndex(kSecondaryRootNodeId);
    dummyLayer->SetClipTreeIndex(dummyClip.id);
    dummyLayer->SetEffectTreeIndex(effectNode.id);
    dummyLayer->SetScrollTreeIndex(kRealRootNodeId);
}

} // namespace

void PaintArtifactCompositor::updateInLayerListMode(const PaintArtifact& paintArtifact)
{
    cc::LayerTreeHost* host = m_rootLayer->layer_tree_host();

    setMinimalPropertyTrees(host->property_trees(), m_rootLayer->id());
    m_rootLayer->RemoveAllChildren();
    m_rootLayer->set_property_tree_sequence_number(kPropertyTreeSequenceNumber);
    m_rootLayer->SetTransformTreeIndex(kSecondaryRootNodeId);
    m_rootLayer->SetClipTreeIndex(kSecondaryRootNodeId);
    m_rootLayer->SetEffectTreeIndex(kSecondaryRootNodeId);
    m_rootLayer->SetScrollTreeIndex(kRealRootNodeId);

    PropertyTreeManager propertyTreeManager(*host->property_trees(), m_rootLayer.get());
    m_contentLayerClients.clear();
    m_contentLayerClients.reserveCapacity(paintArtifact.paintChunks().size());
    for (const PaintChunk& paintChunk : paintArtifact.paintChunks()) {
        gfx::Vector2dF layerOffset;
        scoped_refptr<cc::Layer> layer = layerForPaintChunk(paintArtifact, paintChunk, layerOffset);

        int transformId = propertyTreeManager.compositorIdForTransformNode(paintChunk.properties.transform.get());
        int clipId = propertyTreeManager.compositorIdForClipNode(paintChunk.properties.clip.get());
        int effectId = propertyTreeManager.switchToEffectNode(*paintChunk.properties.effect.get());

        layer->set_offset_to_transform_parent(layerOffset);

        m_rootLayer->AddChild(layer);
        layer->set_property_tree_sequence_number(kPropertyTreeSequenceNumber);
        layer->SetTransformTreeIndex(transformId);
        layer->SetClipTreeIndex(clipId);
        layer->SetEffectTreeIndex(effectId);
        layer->SetScrollTreeIndex(kRealRootNodeId);

        // TODO(jbroman): This probably shouldn't be necessary, but it is still
        // queried by RenderSurfaceImpl.
        layer->Set3dSortingContextId(host->property_trees()->transform_tree.Node(transformId)->sorting_context_id);

        layer->SetShouldCheckBackfaceVisibility(paintChunk.properties.backfaceHidden);

        if (m_extraDataForTestingEnabled)
            m_extraDataForTesting->contentLayers.append(layer);
    }

    // Mark the property trees as having been rebuilt.
    host->property_trees()->sequence_number = kPropertyTreeSequenceNumber;
    host->property_trees()->needs_rebuild = false;
}

} // namespace blink
