// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintPropertyTreeBuilder.h"

#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/SVGRootPainter.h"
#include "platform/transforms/TransformationMatrix.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

void PaintPropertyTreeBuilder::buildTreeRootNodes(FrameView& rootFrame, PaintPropertyTreeBuilderContext& context)
{
    if (RuntimeEnabledFeatures::rootLayerScrollingEnabled())
        return;

    if (!rootFrame.rootTransform() || rootFrame.rootTransform()->parent()) {
        rootFrame.setRootTransform(TransformPaintPropertyNode::create(nullptr, TransformationMatrix(), FloatPoint3D()));
        rootFrame.setRootClip(ClipPaintPropertyNode::create(nullptr, rootFrame.rootTransform(), FloatRoundedRect(LayoutRect::infiniteIntRect())));
        rootFrame.setRootEffect(EffectPaintPropertyNode::create(nullptr, 1.0));
    } else {
        DCHECK(rootFrame.rootClip() && !rootFrame.rootClip()->parent());
        DCHECK(rootFrame.rootEffect() && !rootFrame.rootEffect()->parent());
    }

    context.current.transform = context.absolutePosition.transform = context.fixedPosition.transform = rootFrame.rootTransform();
    context.current.clip = context.absolutePosition.clip = context.fixedPosition.clip = rootFrame.rootClip();
    context.currentEffect = rootFrame.rootEffect();
}

void PaintPropertyTreeBuilder::buildTreeNodes(FrameView& frameView, PaintPropertyTreeBuilderContext& context)
{
    if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
        LayoutView* layoutView = frameView.layoutView();
        if (!layoutView)
            return;

        TransformationMatrix frameTranslate;
        frameTranslate.translate(
            frameView.x() + layoutView->location().x() + context.current.paintOffset.x(),
            frameView.y() + layoutView->location().y() + context.current.paintOffset.y());
        context.current.transform = layoutView->getMutableForPainting().ensureObjectPaintProperties().createOrUpdatePaintOffsetTranslation(
            context.current.transform, frameTranslate, FloatPoint3D());
        context.current.paintOffset = LayoutPoint();
        context.current.renderingContextID = 0;
        context.current.shouldFlattenInheritedTransform = true;
        context.absolutePosition = context.current;
        context.containerForAbsolutePosition = nullptr; // This will get set in updateOutOfFlowContext().
        context.fixedPosition = context.current;
        return;
    }

    TransformationMatrix frameTranslate;
    frameTranslate.translate(frameView.x() + context.current.paintOffset.x(), frameView.y() + context.current.paintOffset.y());
    if (TransformPaintPropertyNode* existingPreTranslation = frameView.preTranslation())
        existingPreTranslation->update(context.current.transform, frameTranslate, FloatPoint3D());
    else
        frameView.setPreTranslation(TransformPaintPropertyNode::create(context.current.transform, frameTranslate, FloatPoint3D()));

    FloatRoundedRect contentClip(IntRect(IntPoint(), frameView.visibleContentSize()));
    if (ClipPaintPropertyNode* existingContentClip = frameView.contentClip())
        existingContentClip->update(context.current.clip, frameView.preTranslation(), contentClip);
    else
        frameView.setContentClip(ClipPaintPropertyNode::create(context.current.clip, frameView.preTranslation(), contentClip));

    DoubleSize scrollOffset = frameView.scrollOffsetDouble();
    TransformationMatrix frameScroll;
    frameScroll.translate(-scrollOffset.width(), -scrollOffset.height());
    if (TransformPaintPropertyNode* existingScrollTranslation = frameView.scrollTranslation())
        existingScrollTranslation->update(frameView.preTranslation(), frameScroll, FloatPoint3D());
    else
        frameView.setScrollTranslation(TransformPaintPropertyNode::create(frameView.preTranslation(), frameScroll, FloatPoint3D()));

    // Initialize the context for current, absolute and fixed position cases.
    // They are the same, except that scroll translation does not apply to
    // fixed position descendants.
    context.current.transform = frameView.scrollTranslation();
    context.current.paintOffset = LayoutPoint();
    context.current.clip = frameView.contentClip();
    context.current.renderingContextID = 0;
    context.current.shouldFlattenInheritedTransform = true;
    context.absolutePosition = context.current;
    context.containerForAbsolutePosition = nullptr;
    context.fixedPosition = context.current;
    context.fixedPosition.transform = frameView.preTranslation();
}

void PaintPropertyTreeBuilder::updatePaintOffsetTranslation(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (object.isBoxModelObject() && context.current.paintOffset != LayoutPoint()) {
        // TODO(trchen): Eliminate PaintLayer dependency.
        PaintLayer* layer = toLayoutBoxModelObject(object).layer();
        if (layer && layer->paintsWithTransform(GlobalPaintNormalPhase)) {
            // We should use the same subpixel paint offset values for snapping regardless of whether a
            // transform is present. If there is a transform we round the paint offset but keep around
            // the residual fractional component for the transformed content to paint with.
            // In spv1 this was called "subpixel accumulation". For more information, see
            // PaintLayer::subpixelAccumulation() and PaintLayerPainter::paintFragmentByApplyingTransform.
            IntPoint roundedPaintOffset = roundedIntPoint(context.current.paintOffset);
            LayoutPoint fractionalPaintOffset = LayoutPoint(context.current.paintOffset - roundedPaintOffset);

            context.current.transform = object.getMutableForPainting().ensureObjectPaintProperties().createOrUpdatePaintOffsetTranslation(
                context.current.transform, TransformationMatrix().translate(roundedPaintOffset.x(), roundedPaintOffset.y()), FloatPoint3D(),
                context.current.shouldFlattenInheritedTransform, context.current.renderingContextID);
            context.current.paintOffset = fractionalPaintOffset;
            return;
        }
    }

    if (object.isLayoutView())
        return;

    if (ObjectPaintProperties* properties = object.getMutableForPainting().objectPaintProperties())
        properties->clearPaintOffsetTranslation();
}

static FloatPoint3D transformOrigin(const LayoutBox& box)
{
    const ComputedStyle& style = box.styleRef();
    FloatSize borderBoxSize(box.size());
    return FloatPoint3D(
        floatValueForLength(style.transformOriginX(), borderBoxSize.width()),
        floatValueForLength(style.transformOriginY(), borderBoxSize.height()),
        style.transformOriginZ());
}

void PaintPropertyTreeBuilder::updateTransform(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (object.isSVG() && !object.isSVGRoot()) {
        // SVG does not use paint offset internally.
        DCHECK(context.current.paintOffset == LayoutPoint());

        // FIXME(pdr): Check for the presence of a transform instead of the value. Checking for an
        // identity matrix will cause the property tree structure to change during animations if
        // the animation passes through the identity matrix.
        // FIXME(pdr): Refactor this so all non-root SVG objects use the same transform function.
        const AffineTransform& transform = object.isSVGForeignObject() ? object.localSVGTransform() : object.localToSVGParentTransform();
        if (!transform.isIdentity()) {
            // The origin is included in the local transform, so leave origin empty.
            context.current.transform = object.getMutableForPainting().ensureObjectPaintProperties().createOrUpdateTransform(
                context.current.transform, TransformationMatrix(transform), FloatPoint3D());
            context.current.renderingContextID = 0;
            context.current.shouldFlattenInheritedTransform = false;
            return;
        }
    } else {
        const ComputedStyle& style = object.styleRef();
        if (object.isBox() && (style.hasTransform() || style.preserves3D())) {
            TransformationMatrix matrix;
            style.applyTransform(matrix, toLayoutBox(object).size(), ComputedStyle::ExcludeTransformOrigin,
                ComputedStyle::IncludeMotionPath, ComputedStyle::IncludeIndependentTransformProperties);
            FloatPoint3D origin = transformOrigin(toLayoutBox(object));

            unsigned renderingContextID = context.current.renderingContextID;
            unsigned renderingContextIDForChildren = 0;
            bool flattensInheritedTransform = context.current.shouldFlattenInheritedTransform;
            bool childrenFlattenInheritedTransform = true;

            // TODO(trchen): transform-style should only be respected if a PaintLayer is
            // created.
            if (style.preserves3D()) {
                // If a node with transform-style: preserve-3d does not exist in an
                // existing rendering context, it establishes a new one.
                if (!renderingContextID)
                    renderingContextID = PtrHash<const LayoutObject>::hash(&object);
                renderingContextIDForChildren = renderingContextID;
                childrenFlattenInheritedTransform = false;
            }

            context.current.transform = object.getMutableForPainting().ensureObjectPaintProperties().createOrUpdateTransform(
                context.current.transform, matrix, origin, flattensInheritedTransform, renderingContextID);
            context.current.renderingContextID = renderingContextIDForChildren;
            context.current.shouldFlattenInheritedTransform = childrenFlattenInheritedTransform;
            return;
        }
    }

    if (ObjectPaintProperties* properties = object.getMutableForPainting().objectPaintProperties())
        properties->clearTransform();
}

void PaintPropertyTreeBuilder::updateEffect(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (object.isLayoutView() && !context.currentEffect) {
        const LayoutView& layoutView = toLayoutView(object);
        DCHECK(RuntimeEnabledFeatures::rootLayerScrollingEnabled());
        DCHECK(layoutView.frameView()->frame().isMainFrame());
        context.currentEffect = layoutView.getMutableForPainting().ensureObjectPaintProperties().createOrUpdateEffect(nullptr, 1.0);
        return;
    }

    if (!object.styleRef().hasOpacity()) {
        if (ObjectPaintProperties* properties = object.getMutableForPainting().objectPaintProperties())
            properties->clearEffect();
        return;
    }

    context.currentEffect = object.getMutableForPainting().ensureObjectPaintProperties().createOrUpdateEffect(
        context.currentEffect, object.styleRef().opacity());
}

void PaintPropertyTreeBuilder::updateCssClip(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (object.hasClip()) {
        // Create clip node for descendants that are not fixed position.
        // We don't have to setup context.absolutePosition.clip here because this object must be
        // a container for absolute position descendants, and will copy from in-flow context later
        // at updateOutOfFlowContext() step.
        DCHECK(object.canContainAbsolutePositionObjects());
        LayoutRect clipRect = toLayoutBox(object).clipRect(context.current.paintOffset);
        context.current.clip = object.getMutableForPainting().ensureObjectPaintProperties().createOrUpdateCssClip(
            context.current.clip, context.current.transform, FloatRoundedRect(FloatRect(clipRect)));
        return;
    }

    if (ObjectPaintProperties* properties = object.getMutableForPainting().objectPaintProperties())
        properties->clearCssClip();
}

void PaintPropertyTreeBuilder::updateLocalBorderBoxContext(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    // Avoid adding an ObjectPaintProperties for non-boxes to save memory, since we don't need them at the moment.
    if (!object.isBox() && !object.hasLayer())
        return;

    std::unique_ptr<ObjectPaintProperties::LocalBorderBoxProperties> borderBoxContext =
        wrapUnique(new ObjectPaintProperties::LocalBorderBoxProperties);
    borderBoxContext->paintOffset = context.current.paintOffset;
    borderBoxContext->propertyTreeState = PropertyTreeState(context.current.transform, context.current.clip, context.currentEffect);

    if (!context.current.clip) {
        DCHECK(object.isLayoutView());
        DCHECK(toLayoutView(object).frameView()->frame().isMainFrame());
        DCHECK(RuntimeEnabledFeatures::rootLayerScrollingEnabled());
        borderBoxContext->propertyTreeState.clip = ClipPaintPropertyNode::create(nullptr, context.current.transform, FloatRoundedRect(LayoutRect::infiniteIntRect()));
        context.current.clip = borderBoxContext->propertyTreeState.clip.get();
    }

    object.getMutableForPainting().ensureObjectPaintProperties().setLocalBorderBoxProperties(std::move(borderBoxContext));

}

// TODO(trchen): Remove this once we bake the paint offset into frameRect.
void PaintPropertyTreeBuilder::updateScrollbarPaintOffset(const LayoutObject& object, const PaintPropertyTreeBuilderContext& context)
{
    IntPoint roundedPaintOffset = roundedIntPoint(context.current.paintOffset);
    if (roundedPaintOffset != IntPoint() && object.isBoxModelObject()) {
        if (PaintLayerScrollableArea* scrollableArea = toLayoutBoxModelObject(object).getScrollableArea()) {
            if (scrollableArea->horizontalScrollbar() || scrollableArea->verticalScrollbar()) {
                auto paintOffset = TransformationMatrix().translate(roundedPaintOffset.x(), roundedPaintOffset.y());
                object.getMutableForPainting().ensureObjectPaintProperties().createOrUpdateScrollbarPaintOffset(
                    context.current.transform, paintOffset, FloatPoint3D());
                return;
            }
        }
    }

    if (ObjectPaintProperties* properties = object.getMutableForPainting().objectPaintProperties())
        properties->clearScrollbarPaintOffset();
}

void PaintPropertyTreeBuilder::updateOverflowClip(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (!object.isBox())
        return;
    const LayoutBox& box = toLayoutBox(object);

    // The <input> elements can't have contents thus CSS overflow property doesn't apply.
    // However for layout purposes we do generate child layout objects for them, e.g. button label.
    // We should clip the overflow from those children. This is called control clip and we
    // technically treat them like overflow clip.
    LayoutRect clipRect;
    if (box.hasControlClip()) {
        clipRect = box.controlClipRect(context.current.paintOffset);
    } else if (box.hasOverflowClip()) {
        clipRect = box.overflowClipRect(context.current.paintOffset);
    } else {
        if (ObjectPaintProperties* properties = object.getMutableForPainting().objectPaintProperties())
            properties->clearOverflowClip();
        return;
    }

    // This need to be in top-level block to hold the reference until we finish creating the normal clip node.
    RefPtr<ClipPaintPropertyNode> borderRadiusClip;
    if (box.styleRef().hasBorderRadius()) {
        auto innerBorder = box.styleRef().getRoundedInnerBorderFor(
            LayoutRect(context.current.paintOffset, box.size()));
        borderRadiusClip = ClipPaintPropertyNode::create(context.current.clip, context.current.transform, innerBorder);
        context.current.clip = borderRadiusClip.get();
    }

    context.current.clip = object.getMutableForPainting().ensureObjectPaintProperties().createOrUpdateOverflowClip(
        context.current.clip, context.current.transform, FloatRoundedRect(FloatRect(clipRect)));
}

static FloatPoint perspectiveOrigin(const LayoutBox& box)
{
    const ComputedStyle& style = box.styleRef();
    FloatSize borderBoxSize(box.size());
    return FloatPoint(
        floatValueForLength(style.perspectiveOriginX(), borderBoxSize.width()),
        floatValueForLength(style.perspectiveOriginY(), borderBoxSize.height()));
}

void PaintPropertyTreeBuilder::updatePerspective(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    const ComputedStyle& style = object.styleRef();
    if (!object.isBox() || !style.hasPerspective()) {
        if (ObjectPaintProperties* properties = object.getMutableForPainting().objectPaintProperties())
            properties->clearPerspective();
        return;
    }

    // The perspective node must not flatten (else nothing will get
    // perspective), but it should still extend the rendering context as most
    // transform nodes do.
    TransformationMatrix matrix = TransformationMatrix().applyPerspective(style.perspective());
    FloatPoint3D origin = perspectiveOrigin(toLayoutBox(object)) + toLayoutSize(context.current.paintOffset);
    context.current.transform = object.getMutableForPainting().ensureObjectPaintProperties().createOrUpdatePerspective(
        context.current.transform, matrix, origin, context.current.shouldFlattenInheritedTransform, context.current.renderingContextID);
    context.current.shouldFlattenInheritedTransform = false;
}

void PaintPropertyTreeBuilder::updateSvgLocalToBorderBoxTransform(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (!object.isSVGRoot())
        return;

    AffineTransform transformToBorderBox = SVGRootPainter(toLayoutSVGRoot(object)).transformToPixelSnappedBorderBox(context.current.paintOffset);

    // The paint offset is included in |transformToBorderBox| so SVG does not need to handle paint
    // offset internally.
    context.current.paintOffset = LayoutPoint();

    if (transformToBorderBox.isIdentity()) {
        if (ObjectPaintProperties* properties = object.getMutableForPainting().objectPaintProperties())
            properties->clearSvgLocalToBorderBoxTransform();
        return;
    }

    context.current.transform = object.getMutableForPainting().ensureObjectPaintProperties().createOrUpdateSvgLocalToBorderBoxTransform(
        context.current.transform, transformToBorderBox, FloatPoint3D());
    context.current.shouldFlattenInheritedTransform = false;
    context.current.renderingContextID = 0;
}

void PaintPropertyTreeBuilder::updateScrollTranslation(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (object.isBoxModelObject() && object.hasOverflowClip()) {
        PaintLayer* layer = toLayoutBoxModelObject(object).layer();
        DCHECK(layer);
        DoubleSize scrollOffset = layer->getScrollableArea()->scrollOffset();
        bool forceScrollingForLayoutView = object.isLayoutView() && RuntimeEnabledFeatures::rootLayerScrollingEnabled();
        if (forceScrollingForLayoutView || !scrollOffset.isZero() || layer->scrollsOverflow()) {
            TransformationMatrix matrix = TransformationMatrix().translate(-scrollOffset.width(), -scrollOffset.height());
            context.current.transform = object.getMutableForPainting().ensureObjectPaintProperties().createOrUpdateScrollTranslation(
                context.current.transform, matrix, FloatPoint3D(), context.current.shouldFlattenInheritedTransform, context.current.renderingContextID);
            context.current.shouldFlattenInheritedTransform = false;
            return;
        }
    }

    if (ObjectPaintProperties* properties = object.getMutableForPainting().objectPaintProperties())
        properties->clearScrollTranslation();
}

void PaintPropertyTreeBuilder::updateOutOfFlowContext(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (object.canContainAbsolutePositionObjects()) {
        context.absolutePosition = context.current;
        context.containerForAbsolutePosition = &object;
    }

    if (object.isLayoutView()) {
        if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
            context.fixedPosition = context.current;
            const TransformPaintPropertyNode* transform = object.objectPaintProperties()->paintOffsetTranslation();
            DCHECK(transform);
            context.fixedPosition.transform = transform;
        }
    } else if (object.canContainFixedPositionObjects()) {
        context.fixedPosition = context.current;
    } else if (object.getMutableForPainting().objectPaintProperties() && object.objectPaintProperties()->cssClip()) {
        // CSS clip applies to all descendants, even if this object is not a containing block
        // ancestor of the descendant. It is okay for absolute-position descendants because
        // having CSS clip implies being absolute position container. However for fixed-position
        // descendants we need to insert the clip here if we are not a containing block ancestor
        // of them.
        auto* cssClip = object.getMutableForPainting().objectPaintProperties()->cssClip();

        // Before we actually create anything, check whether in-flow context and fixed-position
        // context has exactly the same clip. Reuse if possible.
        if (context.fixedPosition.clip == cssClip->parent()) {
            context.fixedPosition.clip = cssClip;
        } else {
            context.fixedPosition.clip = object.getMutableForPainting().ensureObjectPaintProperties().createOrUpdateCssClipFixedPosition(
                context.fixedPosition.clip, const_cast<TransformPaintPropertyNode*>(cssClip->localTransformSpace()), cssClip->clipRect());
            return;
        }
    }

    if (ObjectPaintProperties* properties = object.getMutableForPainting().objectPaintProperties())
        properties->clearCssClipFixedPosition();
}

static void deriveBorderBoxFromContainerContext(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (!object.isBoxModelObject())
        return;

    const LayoutBoxModelObject& boxModelObject = toLayoutBoxModelObject(object);

    switch (object.styleRef().position()) {
    case StaticPosition:
        break;
    case RelativePosition:
        context.current.paintOffset += boxModelObject.offsetForInFlowPosition();
        break;
    case AbsolutePosition: {
        context.current = context.absolutePosition;

        // Absolutely positioned content in an inline should be positioned relative to the inline.
        const LayoutObject* container = context.containerForAbsolutePosition;
        if (container && container->isInFlowPositioned() && container->isLayoutInline()) {
            DCHECK(object.isBox());
            context.current.paintOffset += toLayoutInline(container)->offsetForInFlowPositionedInline(toLayoutBox(object));
        }
        break;
    }
    case StickyPosition:
        context.current.paintOffset += boxModelObject.offsetForInFlowPosition();
        break;
    case FixedPosition:
        context.current = context.fixedPosition;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    if (boxModelObject.isBox() && (!boxModelObject.isSVG() || boxModelObject.isSVGRoot())) {
        // TODO(pdr): Several calls in this function walk back up the tree to calculate containers
        // (e.g., topLeftLocation, offsetForInFlowPosition*). The containing block and other
        // containers can be stored on PaintPropertyTreeBuilderContext instead of recomputing them.
        context.current.paintOffset.moveBy(toLayoutBox(boxModelObject).topLeftLocation());
        // This is a weird quirk that table cells paint as children of table rows,
        // but their location have the row's location baked-in.
        // Similar adjustment is done in LayoutTableCell::offsetFromContainer().
        if (boxModelObject.isTableCell()) {
            LayoutObject* parentRow = boxModelObject.parent();
            ASSERT(parentRow && parentRow->isTableRow());
            context.current.paintOffset.moveBy(-toLayoutBox(parentRow)->topLeftLocation());
        }
    }
}

void PaintPropertyTreeBuilder::buildTreeNodesForSelf(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (!object.isBoxModelObject() && !object.isSVG())
        return;

    deriveBorderBoxFromContainerContext(object, context);

    updatePaintOffsetTranslation(object, context);
    updateTransform(object, context);
    updateEffect(object, context);
    updateCssClip(object, context);
    updateLocalBorderBoxContext(object, context);
    updateScrollbarPaintOffset(object, context);
}

void PaintPropertyTreeBuilder::buildTreeNodesForChildren(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (!object.isBoxModelObject() && !object.isSVG())
        return;

    updateOverflowClip(object, context);
    updatePerspective(object, context);
    updateSvgLocalToBorderBoxTransform(object, context);
    updateScrollTranslation(object, context);
    updateOutOfFlowContext(object, context);
}

} // namespace blink
