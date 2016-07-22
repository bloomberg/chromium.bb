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
    // TODO(pdr): Creating paint properties for FrameView here will not be
    // needed once settings()->rootLayerScrolls() is enabled.
    // TODO(pdr): Make this conditional on the rootLayerScrolls setting.

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
    context.absolutePosition = context.current;
    context.containerForAbsolutePosition = nullptr;
    context.fixedPosition = context.current;
    context.fixedPosition.transform = frameView.preTranslation();
}

template <typename PropertyNode, void (ObjectPaintProperties::*Setter)(PassRefPtr<PropertyNode>)>
void PaintPropertyTreeBuilder::clearPaintProperty(const LayoutObject& object)
{
    if (ObjectPaintProperties* existingProperties = object.objectPaintProperties())
        (existingProperties->*Setter)(nullptr);
}

template <
    typename PropertyNode,
    PropertyNode* (ObjectPaintProperties::*Getter)() const,
    void (ObjectPaintProperties::*Setter)(PassRefPtr<PropertyNode>),
    typename... Args>
void PaintPropertyTreeBuilder::updateOrCreatePaintProperty(const LayoutObject& object, const PaintPropertyTreeBuilderContext& context, PropertyNode*& contextProperty, const Args&... args)
{
    ObjectPaintProperties* existingProperties = object.objectPaintProperties();
    PropertyNode* existingPropertyNode = existingProperties ? (existingProperties->*Getter)() : nullptr;
    if (existingPropertyNode) {
        existingPropertyNode->update(contextProperty, args...);
        contextProperty = existingPropertyNode;
    } else {
        RefPtr<PropertyNode> newPropertyNode = PropertyNode::create(contextProperty, args...);
        contextProperty = newPropertyNode.get();
        (object.getMutableForPainting().ensureObjectPaintProperties().*Setter)(newPropertyNode.release());
    }
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

            updateOrCreatePaintProperty<TransformPaintPropertyNode, &ObjectPaintProperties::paintOffsetTranslation, &ObjectPaintProperties::setPaintOffsetTranslation>(
                object, context, context.current.transform, TransformationMatrix().translate(roundedPaintOffset.x(), roundedPaintOffset.y()), FloatPoint3D());
            context.current.paintOffset = fractionalPaintOffset;
            return;
        }
    }
    clearPaintProperty<TransformPaintPropertyNode, &ObjectPaintProperties::setPaintOffsetTranslation>(object);
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
            updateOrCreatePaintProperty<TransformPaintPropertyNode, &ObjectPaintProperties::transform, &ObjectPaintProperties::setTransform>(
                object, context, context.current.transform, TransformationMatrix(transform), FloatPoint3D());
            return;
        }
    } else {
        const ComputedStyle& style = object.styleRef();
        if (object.isBox() && style.hasTransform()) {
            TransformationMatrix matrix;
            style.applyTransform(matrix, toLayoutBox(object).size(), ComputedStyle::ExcludeTransformOrigin,
                ComputedStyle::IncludeMotionPath, ComputedStyle::IncludeIndependentTransformProperties);
            FloatPoint3D origin = transformOrigin(toLayoutBox(object));
            updateOrCreatePaintProperty<TransformPaintPropertyNode, &ObjectPaintProperties::transform, &ObjectPaintProperties::setTransform>(
                object, context, context.current.transform, matrix, origin);
            return;
        }
    }
    clearPaintProperty<TransformPaintPropertyNode, &ObjectPaintProperties::setTransform>(object);
}

void PaintPropertyTreeBuilder::updateEffect(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (!object.styleRef().hasOpacity()) {
        clearPaintProperty<EffectPaintPropertyNode, &ObjectPaintProperties::setEffect>(object);
        return;
    }

    updateOrCreatePaintProperty<EffectPaintPropertyNode, &ObjectPaintProperties::effect, &ObjectPaintProperties::setEffect>(
        object, context, context.currentEffect, object.styleRef().opacity());
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
        updateOrCreatePaintProperty<ClipPaintPropertyNode, &ObjectPaintProperties::cssClip, &ObjectPaintProperties::setCssClip>(
            object, context, context.current.clip, context.current.transform, FloatRoundedRect(FloatRect(clipRect)));
        return;
    }
    clearPaintProperty<ClipPaintPropertyNode, &ObjectPaintProperties::setCssClip>(object);
}

void PaintPropertyTreeBuilder::updateLocalBorderBoxContext(const LayoutObject& object, const PaintPropertyTreeBuilderContext& context)
{
    // Avoid adding an ObjectPaintProperties for non-boxes to save memory, since we don't need them at the moment.
    if (!object.isBox() && !object.hasLayer())
        return;

    std::unique_ptr<ObjectPaintProperties::LocalBorderBoxProperties> borderBoxContext =
        wrapUnique(new ObjectPaintProperties::LocalBorderBoxProperties);
    borderBoxContext->paintOffset = context.current.paintOffset;
    borderBoxContext->propertyTreeState = PropertyTreeState(context.current.transform, context.current.clip, context.currentEffect);
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
                // Make a copy of context.current.transform because we don't want to set the scrollbarPaintOffset node
                // as the current transform.
                TransformPaintPropertyNode* parentTransform = context.current.transform;
                updateOrCreatePaintProperty<TransformPaintPropertyNode, &ObjectPaintProperties::scrollbarPaintOffset, &ObjectPaintProperties::setScrollbarPaintOffset>(
                    object, context, parentTransform, paintOffset, FloatPoint3D());
                return;
            }
        }
    }
    clearPaintProperty<TransformPaintPropertyNode, &ObjectPaintProperties::setScrollbarPaintOffset>(object);
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
        clearPaintProperty<ClipPaintPropertyNode, &ObjectPaintProperties::setOverflowClip>(object);
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

    updateOrCreatePaintProperty<ClipPaintPropertyNode, &ObjectPaintProperties::overflowClip, &ObjectPaintProperties::setOverflowClip>(
        object, context, context.current.clip, context.current.transform, FloatRoundedRect(FloatRect(clipRect)));
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
        clearPaintProperty<TransformPaintPropertyNode, &ObjectPaintProperties::setPerspective>(object);
        return;
    }

    FloatPoint3D origin = perspectiveOrigin(toLayoutBox(object)) + toLayoutSize(context.current.paintOffset);
    updateOrCreatePaintProperty<TransformPaintPropertyNode, &ObjectPaintProperties::perspective, &ObjectPaintProperties::setPerspective>(
        object, context, context.current.transform, TransformationMatrix().applyPerspective(style.perspective()), origin);
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
        clearPaintProperty<TransformPaintPropertyNode, &ObjectPaintProperties::setSvgLocalToBorderBoxTransform>(object);
        return;
    }

    updateOrCreatePaintProperty<TransformPaintPropertyNode, &ObjectPaintProperties::svgLocalToBorderBoxTransform, &ObjectPaintProperties::setSvgLocalToBorderBoxTransform>(
        object, context, context.current.transform, transformToBorderBox, FloatPoint3D());
}

void PaintPropertyTreeBuilder::updateScrollTranslation(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (object.isBoxModelObject() && object.hasOverflowClip()) {
        PaintLayer* layer = toLayoutBoxModelObject(object).layer();
        DCHECK(layer);
        DoubleSize scrollOffset = layer->getScrollableArea()->scrollOffset();

        if (!scrollOffset.isZero() || layer->scrollsOverflow()) {
            TransformationMatrix matrix = TransformationMatrix().translate(-scrollOffset.width(), -scrollOffset.height());
            updateOrCreatePaintProperty<TransformPaintPropertyNode, &ObjectPaintProperties::scrollTranslation, &ObjectPaintProperties::setScrollTranslation>(
                object, context, context.current.transform, matrix, FloatPoint3D());
            return;
        }
    }
    clearPaintProperty<TransformPaintPropertyNode, &ObjectPaintProperties::setScrollTranslation>(object);
}

void PaintPropertyTreeBuilder::updateOutOfFlowContext(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (object.canContainAbsolutePositionObjects()) {
        context.absolutePosition = context.current;
        context.containerForAbsolutePosition = &object;
    }

    // TODO(pdr): Remove the !object.isLayoutView() condition when removing FrameView
    // paint properties for rootLayerScrolls.
    if (!object.isLayoutView() && object.canContainFixedPositionObjects()) {
        context.fixedPosition = context.current;
    } else if (object.objectPaintProperties() && object.objectPaintProperties()->cssClip()) {
        // CSS clip applies to all descendants, even if this object is not a containing block
        // ancestor of the descendant. It is okay for absolute-position descendants because
        // having CSS clip implies being absolute position container. However for fixed-position
        // descendants we need to insert the clip here if we are not a containing block ancestor
        // of them.
        auto* cssClip = object.objectPaintProperties()->cssClip();

        // Before we actually create anything, check whether in-flow context and fixed-position
        // context has exactly the same clip. Reuse if possible.
        if (context.fixedPosition.clip == cssClip->parent()) {
            context.fixedPosition.clip = cssClip;
        } else {
            updateOrCreatePaintProperty<ClipPaintPropertyNode, &ObjectPaintProperties::cssClipFixedPosition, &ObjectPaintProperties::setCssClipFixedPosition>(
                object, context, context.fixedPosition.clip, const_cast<TransformPaintPropertyNode*>(cssClip->localTransformSpace()), cssClip->clipRect());
            return;
        }
    }
    clearPaintProperty<ClipPaintPropertyNode, &ObjectPaintProperties::setCssClipFixedPosition>(object);
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

void PaintPropertyTreeBuilder::buildTreeNodes(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
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
    updateOverflowClip(object, context);
    // TODO(trchen): Insert flattening transform here, as specified by
    // http://www.w3.org/TR/css3-transforms/#transform-style-property
    updatePerspective(object, context);
    updateSvgLocalToBorderBoxTransform(object, context);
    updateScrollTranslation(object, context);
    updateOutOfFlowContext(object, context);
}

} // namespace blink
