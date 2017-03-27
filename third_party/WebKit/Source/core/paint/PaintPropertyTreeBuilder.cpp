// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintPropertyTreeBuilder.h"

#include <memory>
#include "core/dom/DOMNodeIds.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/CompositingReasonFinder.h"
#include "core/layout/svg/LayoutSVGResourceMasker.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/paint/FindPropertiesNeedingUpdate.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/SVGRootPainter.h"
#include "platform/transforms/TransformationMatrix.h"
#include "wtf/PtrUtil.h"

namespace blink {

PaintPropertyTreeBuilderContext::PaintPropertyTreeBuilderContext()
    : containerForAbsolutePosition(nullptr),
      currentEffect(EffectPaintPropertyNode::root()),
      inputClipOfCurrentEffect(ClipPaintPropertyNode::root()),
      forceSubtreeUpdate(false) {
  current.clip = absolutePosition.clip = fixedPosition.clip =
      ClipPaintPropertyNode::root();
  current.transform = absolutePosition.transform = fixedPosition.transform =
      TransformPaintPropertyNode::root();
  current.scroll = absolutePosition.scroll = fixedPosition.scroll =
      ScrollPaintPropertyNode::root();
}

// True if a new property was created, false if an existing one was updated.
static bool updatePreTranslation(
    FrameView& frameView,
    PassRefPtr<const TransformPaintPropertyNode> parent,
    const TransformationMatrix& matrix,
    const FloatPoint3D& origin) {
  DCHECK(!RuntimeEnabledFeatures::rootLayerScrollingEnabled());
  if (auto* existingPreTranslation = frameView.preTranslation()) {
    existingPreTranslation->update(std::move(parent), matrix, origin);
    return false;
  }
  frameView.setPreTranslation(
      TransformPaintPropertyNode::create(std::move(parent), matrix, origin));
  return true;
}

// True if a new property was created, false if an existing one was updated.
static bool updateContentClip(
    FrameView& frameView,
    PassRefPtr<const ClipPaintPropertyNode> parent,
    PassRefPtr<const TransformPaintPropertyNode> localTransformSpace,
    const FloatRoundedRect& clipRect) {
  DCHECK(!RuntimeEnabledFeatures::rootLayerScrollingEnabled());
  if (auto* existingContentClip = frameView.contentClip()) {
    existingContentClip->update(std::move(parent),
                                std::move(localTransformSpace), clipRect);
    return false;
  }
  frameView.setContentClip(ClipPaintPropertyNode::create(
      std::move(parent), std::move(localTransformSpace), clipRect));
  return true;
}

static CompositorElementId createDomNodeBasedCompositorElementId(
    const LayoutObject& object) {
  return createCompositorElementId(DOMNodeIds::idForNode(object.node()),
                                   CompositorSubElementId::Primary);
}

// True if a new property was created or a main thread scrolling reason changed
// (which can affect descendants), false if an existing one was updated.
static bool updateScrollTranslation(
    FrameView& frameView,
    PassRefPtr<const TransformPaintPropertyNode> parent,
    const TransformationMatrix& matrix,
    const FloatPoint3D& origin,
    PassRefPtr<const ScrollPaintPropertyNode> scrollParent,
    const IntSize& clip,
    const IntSize& bounds,
    bool userScrollableHorizontal,
    bool userScrollableVertical,
    MainThreadScrollingReasons mainThreadScrollingReasons,
    WebLayerScrollClient* scrollClient) {
  DCHECK(!RuntimeEnabledFeatures::rootLayerScrollingEnabled());
  CompositorElementId compositorElementId =
      createDomNodeBasedCompositorElementId(*frameView.layoutView());
  if (auto* existingScrollTranslation = frameView.scrollTranslation()) {
    auto existingReasons =
        existingScrollTranslation->scrollNode()->mainThreadScrollingReasons();
    existingScrollTranslation->updateScrollTranslation(
        std::move(parent), matrix, origin, false, 0, CompositingReasonNone,
        compositorElementId, std::move(scrollParent), clip, bounds,
        userScrollableHorizontal, userScrollableVertical,
        mainThreadScrollingReasons, scrollClient);
    return existingReasons != mainThreadScrollingReasons;
  }
  frameView.setScrollTranslation(
      TransformPaintPropertyNode::createScrollTranslation(
          std::move(parent), matrix, origin, false, 0, CompositingReasonNone,
          compositorElementId, std::move(scrollParent), clip, bounds,
          userScrollableHorizontal, userScrollableVertical,
          mainThreadScrollingReasons, scrollClient));
  return true;
}

static MainThreadScrollingReasons mainThreadScrollingReasons(
    const FrameView& frameView,
    MainThreadScrollingReasons ancestorReasons) {
  auto reasons = ancestorReasons;
  if (!frameView.frame().settings()->getThreadedScrollingEnabled())
    reasons |= MainThreadScrollingReason::kThreadedScrollingDisabled;
  if (frameView.hasBackgroundAttachmentFixedObjects())
    reasons |= MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects;
  return reasons;
}

void PaintPropertyTreeBuilder::updateProperties(
    FrameView& frameView,
    PaintPropertyTreeBuilderContext& context) {
  if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
    // With root layer scrolling, the LayoutView (a LayoutObject) properties are
    // updated like other objects (see updatePropertiesAndContextForSelf and
    // updatePropertiesAndContextForChildren) instead of needing LayoutView-
    // specific property updates here.
    context.current.paintOffset.moveBy(frameView.location());
    context.current.renderingContextId = 0;
    context.current.shouldFlattenInheritedTransform = true;
    context.absolutePosition = context.current;
    context.containerForAbsolutePosition = nullptr;
    context.fixedPosition = context.current;
    return;
  }

#if DCHECK_IS_ON()
  FindFrameViewPropertiesNeedingUpdateScope checkScope(&frameView, context);
#endif

  if (frameView.needsPaintPropertyUpdate() || context.forceSubtreeUpdate) {
    TransformationMatrix frameTranslate;
    frameTranslate.translate(frameView.x() + context.current.paintOffset.x(),
                             frameView.y() + context.current.paintOffset.y());
    context.forceSubtreeUpdate |= updatePreTranslation(
        frameView, context.current.transform, frameTranslate, FloatPoint3D());

    FloatRoundedRect contentClip(
        IntRect(IntPoint(), frameView.visibleContentSize()));
    context.forceSubtreeUpdate |=
        updateContentClip(frameView, context.current.clip,
                          frameView.preTranslation(), contentClip);

    ScrollOffset scrollOffset = frameView.getScrollOffset();
    if (frameView.isScrollable() || !scrollOffset.isZero()) {
      TransformationMatrix frameScroll;
      frameScroll.translate(-scrollOffset.width(), -scrollOffset.height());

      IntSize scrollClip = frameView.visibleContentSize();
      IntSize scrollBounds = frameView.contentsSize();
      bool userScrollableHorizontal =
          frameView.userInputScrollable(HorizontalScrollbar);
      bool userScrollableVertical =
          frameView.userInputScrollable(VerticalScrollbar);

      auto ancestorReasons =
          context.current.scroll->mainThreadScrollingReasons();
      auto reasons = mainThreadScrollingReasons(frameView, ancestorReasons);

      context.forceSubtreeUpdate |= updateScrollTranslation(
          frameView, frameView.preTranslation(), frameScroll, FloatPoint3D(),
          context.current.scroll, scrollClip, scrollBounds,
          userScrollableHorizontal, userScrollableVertical, reasons,
          frameView.getScrollableArea());
    } else {
      if (frameView.scrollTranslation()) {
        // Ensure pre-existing properties are cleared if there is no scrolling.
        frameView.setScrollTranslation(nullptr);
        // Rebuild all descendant properties because a property was removed.
        context.forceSubtreeUpdate = true;
      }
    }
  }

  // Initialize the context for current, absolute and fixed position cases.
  // They are the same, except that scroll translation does not apply to
  // fixed position descendants.
  const auto* fixedTransformNode = frameView.preTranslation()
                                       ? frameView.preTranslation()
                                       : context.current.transform;
  auto* fixedScrollNode = context.current.scroll;
  DCHECK(frameView.preTranslation());
  context.current.transform = frameView.preTranslation();
  DCHECK(frameView.contentClip());
  context.current.clip = frameView.contentClip();
  if (const auto* scrollTranslation = frameView.scrollTranslation()) {
    context.current.transform = scrollTranslation;
    context.current.scroll = scrollTranslation->scrollNode();
  }
  context.current.paintOffset = LayoutPoint();
  context.current.renderingContextId = 0;
  context.current.shouldFlattenInheritedTransform = true;
  context.absolutePosition = context.current;
  context.containerForAbsolutePosition = nullptr;
  context.fixedPosition = context.current;
  context.fixedPosition.transform = fixedTransformNode;
  context.fixedPosition.scroll = fixedScrollNode;

  std::unique_ptr<PropertyTreeState> contentsState(new PropertyTreeState(
      context.current.transform, context.current.clip, context.currentEffect));
  frameView.setTotalPropertyTreeStateForContents(std::move(contentsState));
}

void PaintPropertyTreeBuilder::updatePaintOffsetTranslation(
    const LayoutBoxModelObject& object,
    PaintPropertyTreeBuilderContext& context) {
  bool usesPaintOffsetTranslation = false;
  if (RuntimeEnabledFeatures::rootLayerScrollingEnabled() &&
      object.isLayoutView()) {
    // Root layer scrolling always creates a translation node for LayoutView to
    // ensure fixed and absolute contexts use the correct transform space.
    usesPaintOffsetTranslation = true;
  } else if (object.hasLayer() &&
             context.current.paintOffset != LayoutPoint() &&
             object.layer()->paintsWithTransform(
                 GlobalPaintFlattenCompositingLayers)) {
    usesPaintOffsetTranslation = true;
  }

  if (!usesPaintOffsetTranslation) {
    if (auto* properties = object.getMutableForPainting().paintProperties())
      context.forceSubtreeUpdate |= properties->clearPaintOffsetTranslation();
    return;
  }

  // We should use the same subpixel paint offset values for snapping
  // regardless of whether a transform is present. If there is a transform
  // we round the paint offset but keep around the residual fractional
  // component for the transformed content to paint with.  In spv1 this was
  // called "subpixel accumulation". For more information, see
  // PaintLayer::subpixelAccumulation() and
  // PaintLayerPainter::paintFragmentByApplyingTransform.
  IntPoint roundedPaintOffset = roundedIntPoint(context.current.paintOffset);
  LayoutPoint fractionalPaintOffset =
      LayoutPoint(context.current.paintOffset - roundedPaintOffset);

  auto& properties = object.getMutableForPainting().ensurePaintProperties();
  context.forceSubtreeUpdate |= properties.updatePaintOffsetTranslation(
      context.current.transform,
      TransformationMatrix().translate(roundedPaintOffset.x(),
                                       roundedPaintOffset.y()),
      FloatPoint3D(), context.current.shouldFlattenInheritedTransform,
      context.current.renderingContextId);

  context.current.transform = properties.paintOffsetTranslation();
  context.current.paintOffset = fractionalPaintOffset;
  if (RuntimeEnabledFeatures::rootLayerScrollingEnabled() &&
      object.isLayoutView()) {
    context.absolutePosition.transform = properties.paintOffsetTranslation();
    context.fixedPosition.transform = properties.paintOffsetTranslation();
    context.absolutePosition.paintOffset = LayoutPoint();
    context.fixedPosition.paintOffset = LayoutPoint();
  }
}

// SVG does not use the general transform update of |updateTransform|, instead
// creating a transform node for SVG-specific transforms without 3D.
void PaintPropertyTreeBuilder::updateTransformForNonRootSVG(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  DCHECK(object.isSVGChild());
  // SVG does not use paint offset internally, except for SVGForeignObject which
  // has different SVG and HTML coordinate spaces.
  DCHECK(object.isSVGForeignObject() ||
         context.current.paintOffset == LayoutPoint());

  if (object.needsPaintPropertyUpdate() || context.forceSubtreeUpdate) {
    // TODO(pdr): Refactor this so all non-root SVG objects use the same
    // transform function.
    const AffineTransform& transform = object.isSVGForeignObject()
                                           ? object.localSVGTransform()
                                           : object.localToSVGParentTransform();
    // TODO(pdr): Check for the presence of a transform instead of the value.
    // Checking for an identity matrix will cause the property tree structure
    // to change during animations if the animation passes through the
    // identity matrix.
    if (!transform.isIdentity()) {
      // The origin is included in the local transform, so leave origin empty.
      auto& properties = object.getMutableForPainting().ensurePaintProperties();
      context.forceSubtreeUpdate |= properties.updateTransform(
          context.current.transform, TransformationMatrix(transform),
          FloatPoint3D());
    } else {
      if (auto* properties = object.getMutableForPainting().paintProperties())
        context.forceSubtreeUpdate |= properties->clearTransform();
    }
  }

  if (object.paintProperties() && object.paintProperties()->transform()) {
    context.current.transform = object.paintProperties()->transform();
    context.current.shouldFlattenInheritedTransform = false;
    context.current.renderingContextId = 0;
  }
}

static CompositingReasons compositingReasonsForTransform(const LayoutBox& box) {
  const ComputedStyle& style = box.styleRef();
  CompositingReasons compositingReasons = CompositingReasonNone;
  if (CompositingReasonFinder::requiresCompositingForTransform(box))
    compositingReasons |= CompositingReason3DTransform;

  if (CompositingReasonFinder::requiresCompositingForTransformAnimation(style))
    compositingReasons |= CompositingReasonActiveAnimation;

  if (style.hasWillChangeCompositingHint() &&
      !style.subtreeWillChangeContents())
    compositingReasons |= CompositingReasonWillChangeCompositingHint;

  if (box.hasLayer() && box.layer()->has3DTransformedDescendant()) {
    if (style.hasPerspective())
      compositingReasons |= CompositingReasonPerspectiveWith3DDescendants;
    if (style.usedTransformStyle3D() == TransformStyle3DPreserve3D)
      compositingReasons |= CompositingReasonPreserve3DWith3DDescendants;
  }

  return compositingReasons;
}

static FloatPoint3D transformOrigin(const LayoutBox& box) {
  const ComputedStyle& style = box.styleRef();
  // Transform origin has no effect without a transform or motion path.
  if (!style.hasTransform())
    return FloatPoint3D();
  FloatSize borderBoxSize(box.size());
  return FloatPoint3D(
      floatValueForLength(style.transformOriginX(), borderBoxSize.width()),
      floatValueForLength(style.transformOriginY(), borderBoxSize.height()),
      style.transformOriginZ());
}

void PaintPropertyTreeBuilder::updateTransform(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (object.isSVGChild()) {
    updateTransformForNonRootSVG(object, context);
    return;
  }

  if (object.needsPaintPropertyUpdate() || context.forceSubtreeUpdate) {
    const ComputedStyle& style = object.styleRef();

    // A transform node is allocated for transforms, preserves-3d and any
    // direct compositing reason. The latter is required because this is the
    // only way to represent compositing both an element and its stacking
    // descendants.
    bool hasTransform = false;
    if (object.isBox()) {
      auto& box = toLayoutBox(object);

      CompositingReasons compositingReasons =
          compositingReasonsForTransform(box);

      if (style.hasTransform() || style.preserves3D() ||
          compositingReasons != CompositingReasonNone) {
        TransformationMatrix matrix;
        style.applyTransform(
            matrix, box.size(), ComputedStyle::ExcludeTransformOrigin,
            ComputedStyle::IncludeMotionPath,
            ComputedStyle::IncludeIndependentTransformProperties);

        // TODO(trchen): transform-style should only be respected if a
        // PaintLayer is created. If a node with transform-style: preserve-3d
        // does not exist in an existing rendering context, it establishes a new
        // one.
        unsigned renderingContextId = context.current.renderingContextId;
        if (style.preserves3D() && !renderingContextId)
          renderingContextId = PtrHash<const LayoutObject>::hash(&object);

        CompositorElementId compositorElementId =
            style.hasCurrentTransformAnimation()
                ? createDomNodeBasedCompositorElementId(object)
                : CompositorElementId();

        auto& properties =
            object.getMutableForPainting().ensurePaintProperties();
        context.forceSubtreeUpdate |= properties.updateTransform(
            context.current.transform, matrix, transformOrigin(box),
            context.current.shouldFlattenInheritedTransform, renderingContextId,
            compositingReasons, compositorElementId);
        hasTransform = true;
      }
    }
    if (!hasTransform) {
      if (auto* properties = object.getMutableForPainting().paintProperties())
        context.forceSubtreeUpdate |= properties->clearTransform();
    }
  }

  const auto* properties = object.paintProperties();
  if (properties && properties->transform()) {
    context.current.transform = properties->transform();
    if (object.styleRef().preserves3D()) {
      context.current.renderingContextId =
          properties->transform()->renderingContextId();
      context.current.shouldFlattenInheritedTransform = false;
    } else {
      context.current.renderingContextId = 0;
      context.current.shouldFlattenInheritedTransform = true;
    }
  }
}

static bool computeMaskParameters(IntRect& maskClip,
                                  ColorFilter& maskColorFilter,
                                  const LayoutObject& object,
                                  const LayoutPoint& paintOffset) {
  DCHECK(object.isBoxModelObject() || object.isSVGChild());
  const ComputedStyle& style = object.styleRef();

  if (object.isSVGChild()) {
    SVGResources* resources =
        SVGResourcesCache::cachedResourcesForLayoutObject(&object);
    LayoutSVGResourceMasker* masker = resources ? resources->masker() : nullptr;
    if (!masker)
      return false;
    maskClip = enclosingIntRect(object.objectBoundingBox());
    maskColorFilter = masker->style()->svgStyle().maskType() == MT_LUMINANCE
                          ? ColorFilterLuminanceToAlpha
                          : ColorFilterNone;
    return true;
  }
  if (!style.hasMask())
    return false;

  LayoutRect maximumMaskRegion;
  // For HTML/CSS objects, the extent of the mask is known as "mask
  // painting area", which is determined by CSS mask-clip property.
  // We don't implement mask-clip:margin-box or no-clip currently,
  // so the maximum we can get is border-box.
  if (object.isBox()) {
    maximumMaskRegion = toLayoutBox(object).borderBoxRect();
  } else {
    // For inline elements, depends on the value of box-decoration-break
    // there could be one box in multiple fragments or multiple boxes.
    // Either way here we are only interested in the bounding box of them.
    DCHECK(object.isLayoutInline());
    maximumMaskRegion = toLayoutInline(object).linesBoundingBox();
  }
  maximumMaskRegion.moveBy(paintOffset);
  maskClip = enclosingIntRect(maximumMaskRegion);
  maskColorFilter = ColorFilterNone;
  return true;
}

void PaintPropertyTreeBuilder::updateEffect(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  const ComputedStyle& style = object.styleRef();

  const bool isCSSIsolatedGroup =
      object.isBoxModelObject() && style.isStackingContext();
  if (!isCSSIsolatedGroup && !object.isSVGChild()) {
    if (object.needsPaintPropertyUpdate() || context.forceSubtreeUpdate) {
      if (auto* properties = object.getMutableForPainting().paintProperties()) {
        context.forceSubtreeUpdate |= properties->clearEffect();
        context.forceSubtreeUpdate |= properties->clearMask();
        context.forceSubtreeUpdate |= properties->clearMaskClip();
      }
    }
    return;
  }

  // TODO(trchen): Can't omit effect node if we have 3D children.
  if (object.needsPaintPropertyUpdate() || context.forceSubtreeUpdate) {
    const ClipPaintPropertyNode* outputClip = context.inputClipOfCurrentEffect;

    bool effectNodeNeeded = false;

    // Can't omit effect node if we have paint children with exotic blending.
    if (object.isSVG()) {
      // Yes, including LayoutSVGRoot, because SVG layout objects don't create
      // PaintLayer so PaintLayer::hasNonIsolatedDescendantWithBlendMode()
      // doesn't catch SVG descendants.
      if (SVGLayoutSupport::isIsolationRequired(&object))
        effectNodeNeeded = true;
    } else if (PaintLayer* layer = toLayoutBoxModelObject(object).layer()) {
      if (layer->hasNonIsolatedDescendantWithBlendMode())
        effectNodeNeeded = true;
    }

    SkBlendMode blendMode = object.isBlendingAllowed()
                                ? WebCoreCompositeToSkiaComposite(
                                      CompositeSourceOver, style.blendMode())
                                : SkBlendMode::kSrcOver;
    if (blendMode != SkBlendMode::kSrcOver)
      effectNodeNeeded = true;

    float opacity = style.opacity();
    if (opacity != 1.0f)
      effectNodeNeeded = true;

    // We may begin to composite our subtree prior to an animation starts,
    // but a compositor element ID is only needed when an animation is current.
    CompositorElementId compositorElementId =
        style.hasCurrentOpacityAnimation()
            ? createDomNodeBasedCompositorElementId(object)
            : CompositorElementId();
    CompositingReasons compositingReasons = CompositingReasonNone;
    if (CompositingReasonFinder::requiresCompositingForOpacityAnimation(
            style)) {
      compositingReasons = CompositingReasonActiveAnimation;
      effectNodeNeeded = true;
    }
    DCHECK(!style.hasCurrentOpacityAnimation() ||
           compositingReasons != CompositingReasonNone);

    IntRect maskClip;
    ColorFilter maskColorFilter;
    bool hasMask = computeMaskParameters(maskClip, maskColorFilter, object,
                                         context.current.paintOffset);
    if (hasMask) {
      effectNodeNeeded = true;

      auto& properties = object.getMutableForPainting().ensurePaintProperties();
      context.forceSubtreeUpdate |= properties.updateMaskClip(
          context.current.clip, context.current.transform,
          FloatRoundedRect(maskClip));
      outputClip = properties.maskClip();

      // TODO(crbug.com/683425): PaintArtifactCompositor does not handle
      // grouping (i.e. descendant-dependent compositing reason) properly yet.
      // This forces masked subtree always create a layer for now.
      compositingReasons |= CompositingReasonIsolateCompositedDescendants;
    } else {
      if (auto* properties = object.getMutableForPainting().paintProperties())
        context.forceSubtreeUpdate |= properties->clearMaskClip();
    }

    if (effectNodeNeeded) {
      auto& properties = object.getMutableForPainting().ensurePaintProperties();
      context.forceSubtreeUpdate |= properties.updateEffect(
          context.currentEffect, context.current.transform, outputClip,
          ColorFilterNone, CompositorFilterOperations(), opacity, blendMode,
          compositingReasons, compositorElementId);
      if (hasMask) {
        // TODO(crbug.com/683425): PaintArtifactCompositor does not handle
        // grouping (i.e. descendant-dependent compositing reason) properly yet.
        // Adding CompositingReasonSquashingDisallowed forces mask not getting
        // squashed into a child effect. Have no compositing reason otherwise.
        context.forceSubtreeUpdate |= properties.updateMask(
            properties.effect(), context.current.transform, outputClip,
            maskColorFilter, CompositorFilterOperations(), 1.f,
            SkBlendMode::kDstIn, CompositingReasonSquashingDisallowed,
            CompositorElementId());
      } else {
        context.forceSubtreeUpdate |= properties.clearMask();
      }
    } else {
      if (auto* properties = object.getMutableForPainting().paintProperties()) {
        context.forceSubtreeUpdate |= properties->clearEffect();
        context.forceSubtreeUpdate |= properties->clearMask();
      }
    }
  }

  const auto* properties = object.paintProperties();
  if (properties && properties->effect()) {
    context.currentEffect = properties->effect();
    if (properties->maskClip()) {
      context.inputClipOfCurrentEffect = context.current.clip =
          context.absolutePosition.clip = context.fixedPosition.clip =
              properties->maskClip();
    }
  }
}

void PaintPropertyTreeBuilder::updateFilter(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  const ComputedStyle& style = object.styleRef();

  if (object.needsPaintPropertyUpdate() || context.forceSubtreeUpdate) {
    CompositorFilterOperations filter;
    if (object.isSVGChild()) {
      // TODO(trchen): SVG caches filters in SVGResources. Implement it.
    } else if (PaintLayer* layer = toLayoutBoxModelObject(object).layer()) {
      // TODO(trchen): Eliminate PaintLayer dependency.
      filter = layer->createCompositorFilterOperationsForFilter(style);
    }

    // The CSS filter spec didn't specify how filters interact with overflow
    // clips. The implementation here mimics the old Blink/WebKit behavior for
    // backward compatibility.
    // Basically the output of the filter will be affected by clips that applies
    // to the current element. The descendants that paints into the input of the
    // filter ignores any clips collected so far. For example:
    // <div style="overflow:scroll">
    //   <div style="filter:blur(1px);">
    //     <div>A</div>
    //     <div style="position:absolute;">B</div>
    //   </div>
    // </div>
    // In this example "A" should be clipped if the filter was not present.
    // With the filter, "A" will be rastered without clipping, but instead
    // the blurred result will be clipped.
    // On the other hand, "B" should not be clipped because the overflow clip is
    // not in its containing block chain, but as the filter output will be
    // clipped, so a blurred "B" may still be invisible.
    const ClipPaintPropertyNode* outputClip = context.current.clip;

    // TODO(trchen): A filter may contain spatial operations such that an
    // output pixel may depend on an input pixel outside of the output clip.
    // We should generate a special clip node to represent this expansion.

    // We may begin to composite our subtree prior to an animation starts,
    // but a compositor element ID is only needed when an animation is current.
    CompositorElementId compositorElementId =
        style.hasCurrentFilterAnimation()
            ? createDomNodeBasedCompositorElementId(object)
            : CompositorElementId();
    CompositingReasons compositingReasons =
        CompositingReasonFinder::requiresCompositingForFilterAnimation(style)
            ? CompositingReasonActiveAnimation
            : CompositingReasonNone;
    DCHECK(!style.hasCurrentFilterAnimation() ||
           compositingReasons != CompositingReasonNone);

    if (compositingReasons == CompositingReasonNone && filter.isEmpty()) {
      if (auto* properties = object.getMutableForPainting().paintProperties())
        context.forceSubtreeUpdate |= properties->clearFilter();
    } else {
      auto& properties = object.getMutableForPainting().ensurePaintProperties();
      context.forceSubtreeUpdate |= properties.updateFilter(
          context.currentEffect, context.current.transform, outputClip,
          ColorFilterNone, std::move(filter), 1.f, SkBlendMode::kSrcOver,
          compositingReasons, compositorElementId);
    }
  }

  const auto* properties = object.paintProperties();
  if (properties && properties->filter()) {
    context.currentEffect = properties->filter();
    // TODO(trchen): Change input clip to expansion hint once implemented.
    const ClipPaintPropertyNode* inputClip = properties->filter()->outputClip();
    context.inputClipOfCurrentEffect = context.current.clip =
        context.absolutePosition.clip = context.fixedPosition.clip = inputClip;
  }
}

void PaintPropertyTreeBuilder::updateCssClip(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (object.needsPaintPropertyUpdate() || context.forceSubtreeUpdate) {
    if (object.hasClip()) {
      // Create clip node for descendants that are not fixed position.
      // We don't have to setup context.absolutePosition.clip here because this
      // object must be a container for absolute position descendants, and will
      // copy from in-flow context later at updateOutOfFlowContext() step.
      DCHECK(object.canContainAbsolutePositionObjects());
      LayoutRect clipRect =
          toLayoutBox(object).clipRect(context.current.paintOffset);
      auto& properties = object.getMutableForPainting().ensurePaintProperties();
      context.forceSubtreeUpdate |= properties.updateCssClip(
          context.current.clip, context.current.transform,
          FloatRoundedRect(FloatRect(clipRect)));
    } else {
      if (auto* properties = object.getMutableForPainting().paintProperties())
        context.forceSubtreeUpdate |= properties->clearCssClip();
    }
  }

  const auto* properties = object.paintProperties();
  if (properties && properties->cssClip())
    context.current.clip = properties->cssClip();
}

void PaintPropertyTreeBuilder::updateLocalBorderBoxContext(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (!object.needsPaintPropertyUpdate() && !context.forceSubtreeUpdate)
    return;

  // We need localBorderBoxProperties for layered objects only.
  if (!object.hasLayer()) {
    if (auto* properties = object.getMutableForPainting().paintProperties())
      properties->clearLocalBorderBoxProperties();
  } else {
    auto& properties = object.getMutableForPainting().ensurePaintProperties();
    properties.updateLocalBorderBoxProperties(
        context.current.transform, context.current.clip, context.currentEffect);
  }
}

// TODO(trchen): Remove this once we bake the paint offset into frameRect.
void PaintPropertyTreeBuilder::updateScrollbarPaintOffset(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (!object.needsPaintPropertyUpdate() && !context.forceSubtreeUpdate)
    return;

  bool needsScrollbarPaintOffset = false;
  IntPoint roundedPaintOffset = roundedIntPoint(context.current.paintOffset);
  if (roundedPaintOffset != IntPoint() && object.isBoxModelObject()) {
    if (auto* area = toLayoutBoxModelObject(object).getScrollableArea()) {
      if (area->horizontalScrollbar() || area->verticalScrollbar()) {
        auto paintOffset = TransformationMatrix().translate(
            roundedPaintOffset.x(), roundedPaintOffset.y());
        auto& properties =
            object.getMutableForPainting().ensurePaintProperties();
        context.forceSubtreeUpdate |= properties.updateScrollbarPaintOffset(
            context.current.transform, paintOffset, FloatPoint3D());
        needsScrollbarPaintOffset = true;
      }
    }
  }

  auto* properties = object.getMutableForPainting().paintProperties();
  if (!needsScrollbarPaintOffset && properties)
    context.forceSubtreeUpdate |= properties->clearScrollbarPaintOffset();
}

void PaintPropertyTreeBuilder::updateOverflowClip(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (!object.isBox())
    return;

  if (object.needsPaintPropertyUpdate() || context.forceSubtreeUpdate) {
    const LayoutBox& box = toLayoutBox(object);
    // The <input> elements can't have contents thus CSS overflow property
    // doesn't apply.  However for layout purposes we do generate child layout
    // objects for them, e.g. button label.  We should clip the overflow from
    // those children. This is called control clip and we technically treat them
    // like overflow clip.
    LayoutRect clipRect;
    if (box.shouldClipOverflow()) {
      clipRect = LayoutRect(box.overflowClipRect(context.current.paintOffset));
    } else {
      if (auto* properties = object.getMutableForPainting().paintProperties()) {
        context.forceSubtreeUpdate |= properties->clearInnerBorderRadiusClip();
        context.forceSubtreeUpdate |= properties->clearOverflowClip();
      }
      return;
    }

    auto& properties = object.getMutableForPainting().ensurePaintProperties();
    const auto* currentClip = context.current.clip;
    if (box.styleRef().hasBorderRadius()) {
      auto innerBorder = box.styleRef().getRoundedInnerBorderFor(
          LayoutRect(context.current.paintOffset, box.size()));
      context.forceSubtreeUpdate |= properties.updateInnerBorderRadiusClip(
          context.current.clip, context.current.transform, innerBorder);
      currentClip = properties.innerBorderRadiusClip();
    } else {
      context.forceSubtreeUpdate |= properties.clearInnerBorderRadiusClip();
    }

    context.forceSubtreeUpdate |=
        properties.updateOverflowClip(currentClip, context.current.transform,
                                      FloatRoundedRect(FloatRect(clipRect)));
  }

  const auto* properties = object.paintProperties();
  if (properties && properties->overflowClip())
    context.current.clip = properties->overflowClip();
}

static FloatPoint perspectiveOrigin(const LayoutBox& box) {
  const ComputedStyle& style = box.styleRef();
  // Perspective origin has no effect without perspective.
  DCHECK(style.hasPerspective());
  FloatSize borderBoxSize(box.size());
  return FloatPoint(
      floatValueForLength(style.perspectiveOriginX(), borderBoxSize.width()),
      floatValueForLength(style.perspectiveOriginY(), borderBoxSize.height()));
}

void PaintPropertyTreeBuilder::updatePerspective(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (object.needsPaintPropertyUpdate() || context.forceSubtreeUpdate) {
    const ComputedStyle& style = object.styleRef();
    if (object.isBox() && style.hasPerspective()) {
      // The perspective node must not flatten (else nothing will get
      // perspective), but it should still extend the rendering context as
      // most transform nodes do.
      TransformationMatrix matrix =
          TransformationMatrix().applyPerspective(style.perspective());
      FloatPoint3D origin = perspectiveOrigin(toLayoutBox(object)) +
                            toLayoutSize(context.current.paintOffset);
      auto& properties = object.getMutableForPainting().ensurePaintProperties();
      context.forceSubtreeUpdate |= properties.updatePerspective(
          context.current.transform, matrix, origin,
          context.current.shouldFlattenInheritedTransform,
          context.current.renderingContextId);
    } else {
      if (auto* properties = object.getMutableForPainting().paintProperties())
        context.forceSubtreeUpdate |= properties->clearPerspective();
    }
  }

  const auto* properties = object.paintProperties();
  if (properties && properties->perspective()) {
    context.current.transform = properties->perspective();
    context.current.shouldFlattenInheritedTransform = false;
  }
}

void PaintPropertyTreeBuilder::updateSvgLocalToBorderBoxTransform(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (!object.isSVGRoot())
    return;

  if (object.needsPaintPropertyUpdate() || context.forceSubtreeUpdate) {
    AffineTransform transformToBorderBox =
        SVGRootPainter(toLayoutSVGRoot(object))
            .transformToPixelSnappedBorderBox(context.current.paintOffset);
    if (!transformToBorderBox.isIdentity()) {
      auto& properties = object.getMutableForPainting().ensurePaintProperties();
      context.forceSubtreeUpdate |=
          properties.updateSvgLocalToBorderBoxTransform(
              context.current.transform, transformToBorderBox, FloatPoint3D());
    } else {
      if (auto* properties = object.getMutableForPainting().paintProperties()) {
        context.forceSubtreeUpdate |=
            properties->clearSvgLocalToBorderBoxTransform();
      }
    }
  }

  const auto* properties = object.paintProperties();
  if (properties && properties->svgLocalToBorderBoxTransform()) {
    context.current.transform = properties->svgLocalToBorderBoxTransform();
    context.current.shouldFlattenInheritedTransform = false;
    context.current.renderingContextId = 0;
  }
  // The paint offset is included in |transformToBorderBox| so SVG does not need
  // to handle paint offset internally.
  context.current.paintOffset = LayoutPoint();
}

static MainThreadScrollingReasons mainThreadScrollingReasons(
    const LayoutObject& object,
    MainThreadScrollingReasons ancestorReasons) {
  // The current main thread scrolling reasons implementation only changes
  // reasons at frame boundaries, so we can early-out when not at a LayoutView.
  // TODO(pdr): Need to find a solution to the style-related main thread
  // scrolling reasons such as opacity and transform which violate this.
  if (!object.isLayoutView())
    return ancestorReasons;
  return mainThreadScrollingReasons(*object.frameView(), ancestorReasons);
}

void PaintPropertyTreeBuilder::updateScrollAndScrollTranslation(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (object.needsPaintPropertyUpdate() || context.forceSubtreeUpdate) {
    bool needsScrollProperties = false;
    if (object.hasOverflowClip()) {
      auto ancestorReasons =
          context.current.scroll->mainThreadScrollingReasons();
      auto reasons = mainThreadScrollingReasons(object, ancestorReasons);
      bool scrollNodeNeededForMainThreadReasons = ancestorReasons != reasons;

      const LayoutBox& box = toLayoutBox(object);
      auto* scrollableArea = box.getScrollableArea();
      IntSize scrollOffset = box.scrolledContentOffset();
      if (scrollNodeNeededForMainThreadReasons || !scrollOffset.isZero() ||
          scrollableArea->scrollsOverflow()) {
        needsScrollProperties = true;
        auto& properties =
            object.getMutableForPainting().ensurePaintProperties();

        IntSize scrollClip = scrollableArea->visibleContentRect().size();
        IntSize scrollBounds = scrollableArea->contentsSize();
        bool userScrollableHorizontal =
            scrollableArea->userInputScrollable(HorizontalScrollbar);
        bool userScrollableVertical =
            scrollableArea->userInputScrollable(VerticalScrollbar);

        // Main thread scrolling reasons depend on their ancestor's reasons
        // so ensure the entire subtree is updated when reasons change.
        if (auto* existingScrollTranslation = properties.scrollTranslation()) {
          auto* existingScrollNode = existingScrollTranslation->scrollNode();
          if (existingScrollNode->mainThreadScrollingReasons() != reasons)
            context.forceSubtreeUpdate = true;
        }

        CompositorElementId compositorElementId =
            createDomNodeBasedCompositorElementId(object);
        TransformationMatrix matrix = TransformationMatrix().translate(
            -scrollOffset.width(), -scrollOffset.height());
        context.forceSubtreeUpdate |= properties.updateScrollTranslation(
            context.current.transform, matrix, FloatPoint3D(),
            context.current.shouldFlattenInheritedTransform,
            context.current.renderingContextId, CompositingReasonNone,
            compositorElementId, context.current.scroll, scrollClip,
            scrollBounds, userScrollableHorizontal, userScrollableVertical,
            reasons, scrollableArea);
      }
    }

    if (!needsScrollProperties) {
      // Ensure pre-existing properties are cleared.
      if (auto* properties = object.getMutableForPainting().paintProperties())
        context.forceSubtreeUpdate |= properties->clearScrollTranslation();
    }
  }

  if (object.paintProperties() &&
      object.paintProperties()->scrollTranslation()) {
    context.current.transform = object.paintProperties()->scrollTranslation();
    context.current.scroll = context.current.transform->scrollNode();
    context.current.shouldFlattenInheritedTransform = false;
  }
}

void PaintPropertyTreeBuilder::updateOutOfFlowContext(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (object.isLayoutBlock())
    context.paintOffsetForFloat = context.current.paintOffset;

  if (object.canContainAbsolutePositionObjects()) {
    context.absolutePosition = context.current;
    context.containerForAbsolutePosition = &object;
  }

  if (object.isLayoutView()) {
    if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
      const auto* initialFixedTransform = context.fixedPosition.transform;
      const auto* initialFixedScroll = context.fixedPosition.scroll;

      context.fixedPosition = context.current;

      // Fixed position transform and scroll nodes should not be affected.
      context.fixedPosition.transform = initialFixedTransform;
      context.fixedPosition.scroll = initialFixedScroll;
    }
  } else if (object.canContainFixedPositionObjects()) {
    context.fixedPosition = context.current;
  } else if (object.getMutableForPainting().paintProperties() &&
             object.paintProperties()->cssClip()) {
    // CSS clip applies to all descendants, even if this object is not a
    // containing block ancestor of the descendant. It is okay for
    // absolute-position descendants because having CSS clip implies being
    // absolute position container. However for fixed-position descendants we
    // need to insert the clip here if we are not a containing block ancestor of
    // them.
    auto* cssClip = object.getMutableForPainting().paintProperties()->cssClip();

    // Before we actually create anything, check whether in-flow context and
    // fixed-position context has exactly the same clip. Reuse if possible.
    if (context.fixedPosition.clip == cssClip->parent()) {
      context.fixedPosition.clip = cssClip;
    } else {
      if (object.needsPaintPropertyUpdate() || context.forceSubtreeUpdate) {
        auto& properties =
            object.getMutableForPainting().ensurePaintProperties();
        context.forceSubtreeUpdate |= properties.updateCssClipFixedPosition(
            context.fixedPosition.clip, const_cast<TransformPaintPropertyNode*>(
                                            cssClip->localTransformSpace()),
            cssClip->clipRect());
      }
      const auto* properties = object.paintProperties();
      if (properties && properties->cssClipFixedPosition())
        context.fixedPosition.clip = properties->cssClipFixedPosition();
      return;
    }
  }

  if (object.needsPaintPropertyUpdate() || context.forceSubtreeUpdate) {
    if (auto* properties = object.getMutableForPainting().paintProperties())
      context.forceSubtreeUpdate |= properties->clearCssClipFixedPosition();
  }
}

void PaintPropertyTreeBuilder::updatePaintOffset(
    const LayoutBoxModelObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (object.isFloating())
    context.current.paintOffset = context.paintOffsetForFloat;

  // Multicolumn spanners are painted starting at the multicolumn container (but
  // still inherit properties in layout-tree order) so reset the paint offset.
  if (object.isColumnSpanAll())
    context.current.paintOffset = object.container()->paintOffset();

  switch (object.styleRef().position()) {
    case EPosition::kStatic:
      break;
    case EPosition::kRelative:
      context.current.paintOffset += object.offsetForInFlowPosition();
      break;
    case EPosition::kAbsolute: {
      DCHECK(context.containerForAbsolutePosition == object.container());
      context.current = context.absolutePosition;

      // Absolutely positioned content in an inline should be positioned
      // relative to the inline.
      const LayoutObject* container = context.containerForAbsolutePosition;
      if (container && container->isInFlowPositioned() &&
          container->isLayoutInline()) {
        DCHECK(object.isBox());
        context.current.paintOffset +=
            toLayoutInline(container)->offsetForInFlowPositionedInline(
                toLayoutBox(object));
      }
      break;
    }
    case EPosition::kSticky:
      context.current.paintOffset += object.offsetForInFlowPosition();
      break;
    case EPosition::kFixed:
      context.current = context.fixedPosition;
      break;
    default:
      ASSERT_NOT_REACHED();
  }

  if (object.isBox()) {
    // TODO(pdr): Several calls in this function walk back up the tree to
    // calculate containers (e.g., physicalLocation, offsetForInFlowPosition*).
    // The containing block and other containers can be stored on
    // PaintPropertyTreeBuilderContext instead of recomputing them.
    context.current.paintOffset.moveBy(toLayoutBox(object).physicalLocation());
    // This is a weird quirk that table cells paint as children of table rows,
    // but their location have the row's location baked-in.
    // Similar adjustment is done in LayoutTableCell::offsetFromContainer().
    if (object.isTableCell()) {
      LayoutObject* parentRow = object.parent();
      DCHECK(parentRow && parentRow->isTableRow());
      context.current.paintOffset.moveBy(
          -toLayoutBox(parentRow)->physicalLocation());
    }
  }
}

void PaintPropertyTreeBuilder::updateForObjectLocationAndSize(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (object.isBoxModelObject()) {
    updatePaintOffset(toLayoutBoxModelObject(object), context);
    updatePaintOffsetTranslation(toLayoutBoxModelObject(object), context);
  }

  if (object.paintOffset() != context.current.paintOffset) {
    // Many paint properties depend on paint offset so we force an update of
    // the entire subtree on paint offset changes.
    context.forceSubtreeUpdate = true;

    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      object.getMutableForPainting().setShouldDoFullPaintInvalidation(
          PaintInvalidationLocationChange);
    }
    object.getMutableForPainting().setPaintOffset(context.current.paintOffset);
  }

  if (!object.isBox())
    return;
  const LayoutBox& box = toLayoutBox(object);
  if (box.size() == box.previousSize())
    return;

  // CSS mask and clip-path comes with an implicit clip to the border box.
  // Currently only SPv2 generate and take advantage of those.
  const bool boxGeneratesPropertyNodesForMaskAndClipPath =
      RuntimeEnabledFeatures::slimmingPaintV2Enabled() &&
      (box.hasMask() || box.hasClipPath());
  // The overflow clip paint property depends on the border box rect through
  // overflowClipRect(). The border box rect's size equals the frame rect's
  // size so we trigger a paint property update when the frame rect changes.
  if (box.shouldClipOverflow() ||
      // The used value of CSS clip may depend on size of the box, e.g. for
      // clip: rect(auto auto auto -5px).
      box.hasClip() ||
      // Relative lengths (e.g., percentage values) in transform, perspective,
      // transform-origin, and perspective-origin can depend on the size of the
      // frame rect, so force a property update if it changes. TODO(pdr): We
      // only need to update properties if there are relative lengths.
      box.styleRef().hasTransform() || box.styleRef().hasPerspective() ||
      boxGeneratesPropertyNodesForMaskAndClipPath)
    box.getMutableForPainting().setNeedsPaintPropertyUpdate();
}

void PaintPropertyTreeBuilder::updatePropertiesForSelf(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (object.isSVGHiddenContainer()) {
    // SVG resources are painted within one or more other locations in the
    // SVG during paint, and hence have their own independent paint property
    // trees, paint offset, etc.
    context = PaintPropertyTreeBuilderContext();
  }

  // This is not in FindObjectPropertiesNeedingUpdateScope because paint offset
  // can change without needsPaintPropertyUpdate.
  updateForObjectLocationAndSize(object, context);

#if DCHECK_IS_ON()
  FindObjectPropertiesNeedingUpdateScope checkNeedsUpdateScope(object, context);
#endif

  if (object.isBoxModelObject() || object.isSVG()) {
    updateTransform(object, context);
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      updateEffect(object, context);
      updateFilter(object, context);
    }
    updateCssClip(object, context);
    updateLocalBorderBoxContext(object, context);
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
      updateScrollbarPaintOffset(object, context);
  }
}

void PaintPropertyTreeBuilder::updatePropertiesForChildren(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
#if DCHECK_IS_ON()
  FindObjectPropertiesNeedingUpdateScope checkNeedsUpdateScope(object, context);
#endif

  if (!object.isBoxModelObject() && !object.isSVG())
    return;

  updateOverflowClip(object, context);
  updatePerspective(object, context);
  updateSvgLocalToBorderBoxTransform(object, context);
  updateScrollAndScrollTranslation(object, context);
  updateOutOfFlowContext(object, context);

  context.forceSubtreeUpdate |= object.subtreeNeedsPaintPropertyUpdate();
}

}  // namespace blink
