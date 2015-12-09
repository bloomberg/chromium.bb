// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/PaintPropertyTreeBuilder.h"

#include "core/frame/FrameView.h"
#include "core/layout/LayoutView.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "platform/transforms/TransformationMatrix.h"

namespace blink {

// The context for layout tree walk.
// The walk will be done in the primary tree order (= DOM order), thus the context will also be
// responsible for bookkeeping tree state in other order, for example, the most recent position
// container seen.
struct PaintPropertyTreeBuilderContext {
    PaintPropertyTreeBuilderContext()
        : currentTransform(nullptr)
        , transformForOutOfFlowPositioned(nullptr)
        , transformForFixedPositioned(nullptr)
        , currentEffect(nullptr) { }

    // The combination of a transform and paint offset describes a linear space.
    // When a layout object recur to its children, the main context is expected to refer
    // the object's border box, then the callee will derive its own border box by translating
    // the space with its own layout location.
    TransformPaintPropertyNode* currentTransform;
    LayoutPoint paintOffset;

    // Separate context for out-of-flow positioned and fixed positioned elements are needed
    // because they don't use DOM parent as their positioning parent (i.e. containing block).
    // These additional contexts normally pass through untouched, and are only copied from
    // the main context when the current element serves as the positioning parent of corresponding
    // positioned descendants.
    TransformPaintPropertyNode* transformForOutOfFlowPositioned;
    LayoutPoint paintOffsetForOutOfFlowPositioned;

    TransformPaintPropertyNode* transformForFixedPositioned;
    LayoutPoint paintOffsetForFixedPositioned;

    EffectPaintPropertyNode* currentEffect;
};

void PaintPropertyTreeBuilder::buildPropertyTrees(FrameView& rootFrame)
{
    walk(rootFrame, PaintPropertyTreeBuilderContext());
}

void PaintPropertyTreeBuilder::walk(FrameView& frameView, const PaintPropertyTreeBuilderContext& context)
{
    PaintPropertyTreeBuilderContext localContext(context);

    TransformationMatrix frameTranslate;
    frameTranslate.translate(frameView.x(), frameView.y());
    // The frame owner applies paint offset already.
    // This assumption may change in the future.
    ASSERT(context.paintOffset == LayoutPoint());
    RefPtr<TransformPaintPropertyNode> newTransformNodeForPreTranslation = TransformPaintPropertyNode::create(frameTranslate, FloatPoint3D(), context.currentTransform);
    localContext.transformForFixedPositioned = newTransformNodeForPreTranslation.get();
    localContext.paintOffsetForFixedPositioned = LayoutPoint();

    // This is going away in favor of Settings::rootLayerScrolls.
    DoubleSize scrollOffset = frameView.scrollOffsetDouble();
    TransformationMatrix frameScroll;
    frameScroll.translate(-scrollOffset.width(), -scrollOffset.height());
    RefPtr<TransformPaintPropertyNode> newTransformNodeForScrollTranslation = TransformPaintPropertyNode::create(frameScroll, FloatPoint3D(), newTransformNodeForPreTranslation);
    localContext.currentTransform = localContext.transformForOutOfFlowPositioned = newTransformNodeForScrollTranslation.get();
    localContext.paintOffset = localContext.paintOffsetForOutOfFlowPositioned = LayoutPoint();

    frameView.setPreTranslation(newTransformNodeForPreTranslation.release());
    frameView.setScrollTranslation(newTransformNodeForScrollTranslation.release());

    if (LayoutView* layoutView = frameView.layoutView())
        walk(*layoutView, localContext);
}

static void deriveBorderBoxFromContainerContext(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (!object.isBoxModelObject())
        return;

    const LayoutBoxModelObject& boxModelObject = toLayoutBoxModelObject(object);

    // TODO(trchen): There is some insanity going on with tables. Double check results.
    switch (object.styleRef().position()) {
    case StaticPosition:
        break;
    case RelativePosition:
        context.paintOffset += boxModelObject.offsetForInFlowPosition();
        break;
    case AbsolutePosition:
        context.currentTransform = context.transformForOutOfFlowPositioned;
        context.paintOffset = context.paintOffsetForOutOfFlowPositioned;
        break;
    case StickyPosition:
        context.paintOffset += boxModelObject.offsetForInFlowPosition();
        break;
    case FixedPosition:
        context.currentTransform = context.transformForFixedPositioned;
        context.paintOffset = context.paintOffsetForFixedPositioned;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    if (boxModelObject.isBox())
        context.paintOffset += toLayoutBox(boxModelObject).locationOffset();
}

static PassRefPtr<TransformPaintPropertyNode> createPaintOffsetTranslationIfNeeded(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    bool shouldCreatePaintOffsetTranslationNode = false;
    if (object.isSVGRoot()) {
        // SVG doesn't use paint offset internally so emit a paint offset at the html->svg boundary.
        shouldCreatePaintOffsetTranslationNode = true;
    } else if (object.isBoxModelObject()) {
        // TODO(trchen): Eliminate PaintLayer dependency.
        PaintLayer* layer = toLayoutBoxModelObject(object).layer();
        shouldCreatePaintOffsetTranslationNode = layer && layer->paintsWithTransform(GlobalPaintNormalPhase);
    }

    if (context.paintOffset == LayoutPoint() || !shouldCreatePaintOffsetTranslationNode)
        return nullptr;

    RefPtr<TransformPaintPropertyNode> newTransformNodeForPaintOffsetTranslation = TransformPaintPropertyNode::create(
        TransformationMatrix().translate(context.paintOffset.x(), context.paintOffset.y()),
        FloatPoint3D(), context.currentTransform);
    context.currentTransform = newTransformNodeForPaintOffsetTranslation.get();
    context.paintOffset = LayoutPoint();
    return newTransformNodeForPaintOffsetTranslation.release();
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

static PassRefPtr<TransformPaintPropertyNode> createTransformIfNeeded(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (object.isSVG() && !object.isSVGRoot()) {
        const AffineTransform& transform = object.localToParentTransform();
        if (transform.isIdentity())
            return nullptr;

        // SVG's transform origin is baked into the localToParentTransform.
        RefPtr<TransformPaintPropertyNode> newTransformNodeForTransform = TransformPaintPropertyNode::create(
            transform, FloatPoint3D(0, 0, 0), context.currentTransform);
        context.currentTransform = newTransformNodeForTransform.get();
        return newTransformNodeForTransform.release();
    }

    const ComputedStyle& style = object.styleRef();
    if (!object.isBox() || !style.hasTransform())
        return nullptr;

    ASSERT(context.paintOffset == LayoutPoint());

    TransformationMatrix matrix;
    style.applyTransform(matrix, toLayoutBox(object).size(), ComputedStyle::ExcludeTransformOrigin,
        ComputedStyle::IncludeMotionPath, ComputedStyle::IncludeIndependentTransformProperties);
    RefPtr<TransformPaintPropertyNode> newTransformNodeForTransform = TransformPaintPropertyNode::create(
        matrix, transformOrigin(toLayoutBox(object)), context.currentTransform);
    context.currentTransform = newTransformNodeForTransform.get();
    return newTransformNodeForTransform.release();
}

static PassRefPtr<EffectPaintPropertyNode> createEffectIfNeeded(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    const ComputedStyle& style = object.styleRef();
    if (!style.hasOpacity())
        return nullptr;
    RefPtr<EffectPaintPropertyNode> newEffectNode = EffectPaintPropertyNode::create(style.opacity(), context.currentEffect);
    context.currentEffect = newEffectNode.get();
    return newEffectNode.release();
}

static FloatPoint perspectiveOrigin(const LayoutBox& box)
{
    const ComputedStyle& style = box.styleRef();
    FloatSize borderBoxSize(box.size());
    return FloatPoint(
        floatValueForLength(style.perspectiveOriginX(), borderBoxSize.width()),
        floatValueForLength(style.perspectiveOriginY(), borderBoxSize.height()));
}

static PassRefPtr<TransformPaintPropertyNode> createPerspectiveIfNeeded(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    const ComputedStyle& style = object.styleRef();
    if (!object.isBox() || !style.hasPerspective())
        return nullptr;

    RefPtr<TransformPaintPropertyNode> newTransformNodeForPerspective = TransformPaintPropertyNode::create(
        TransformationMatrix().applyPerspective(style.perspective()),
        perspectiveOrigin(toLayoutBox(object)) + toLayoutSize(context.paintOffset), context.currentTransform);
    context.currentTransform = newTransformNodeForPerspective.get();
    return newTransformNodeForPerspective.release();
}

static PassRefPtr<TransformPaintPropertyNode> createScrollTranslationIfNeeded(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (!object.isBoxModelObject() || !object.hasOverflowClip())
        return nullptr;

    PaintLayer* layer = toLayoutBoxModelObject(object).layer();
    ASSERT(layer);
    DoubleSize scrollOffset = layer->scrollableArea()->scrollOffset();
    if (scrollOffset.isZero() && !layer->scrollsOverflow())
        return nullptr;

    RefPtr<TransformPaintPropertyNode> newTransformNodeForScrollTranslation = TransformPaintPropertyNode::create(
        TransformationMatrix().translate(-scrollOffset.width(), -scrollOffset.height()),
        FloatPoint3D(), context.currentTransform);
    context.currentTransform = newTransformNodeForScrollTranslation.get();
    return newTransformNodeForScrollTranslation.release();
}

static void updateOutOfFlowContext(const LayoutObject& object, bool createdNewTransform, PaintPropertyTreeBuilderContext& context)
{
    // At the html->svg boundary (see: createPaintOffsetTranslationIfNeeded) the currentTransform is
    // up-to-date for all children of the svg root element. Additionally, inside SVG, all positioning
    // uses transforms. Therefore, we only need to check createdNewTransform and isSVGRoot() to
    // ensure out-of-flow and fixed positioning is correct at the svg->html boundary.

    if (object.isPositioned() || createdNewTransform || object.isSVGRoot()) {
        context.transformForOutOfFlowPositioned = context.currentTransform;
        context.paintOffsetForOutOfFlowPositioned = context.paintOffset;
    }

    if (createdNewTransform || object.isSVGRoot()) {
        context.transformForFixedPositioned = context.currentTransform;
        context.paintOffsetForFixedPositioned = context.paintOffset;
    }
}

void PaintPropertyTreeBuilder::walk(LayoutObject& object, const PaintPropertyTreeBuilderContext& context)
{
    PaintPropertyTreeBuilderContext localContext(context);

    deriveBorderBoxFromContainerContext(object, localContext);
    RefPtr<TransformPaintPropertyNode> newTransformNodeForPaintOffsetTranslation = createPaintOffsetTranslationIfNeeded(object, localContext);
    RefPtr<TransformPaintPropertyNode> newTransformNodeForTransform = createTransformIfNeeded(object, localContext);
    RefPtr<EffectPaintPropertyNode> newEffectNode = createEffectIfNeeded(object, localContext);
    RefPtr<TransformPaintPropertyNode> newTransformNodeForPerspective = createPerspectiveIfNeeded(object, localContext);
    RefPtr<TransformPaintPropertyNode> newTransformNodeForScrollTranslation = createScrollTranslationIfNeeded(object, localContext);
    updateOutOfFlowContext(object, newTransformNodeForTransform, localContext);

    if (newTransformNodeForPaintOffsetTranslation || newTransformNodeForTransform || newEffectNode || newTransformNodeForPerspective || newTransformNodeForScrollTranslation) {
        OwnPtr<ObjectPaintProperties> updatedPaintProperties = ObjectPaintProperties::create(
            newTransformNodeForPaintOffsetTranslation.release(),
            newTransformNodeForTransform.release(),
            newEffectNode.release(),
            newTransformNodeForPerspective.release(),
            newTransformNodeForScrollTranslation.release());
        object.setObjectPaintProperties(updatedPaintProperties.release());
    } else {
        object.clearObjectPaintProperties();
    }

    // TODO(trchen): Walk subframes for LayoutFrame.

    for (LayoutObject* child = object.slowFirstChild(); child; child = child->nextSibling()) {
        if (child->isBoxModelObject() || child->isSVG())
            walk(*child, localContext);
    }
}

} // namespace blink
