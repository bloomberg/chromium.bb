// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ClipPathClipper.h"

#include "core/dom/ElementTraversal.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/svg/LayoutSVGResourceClipper.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/paint/PaintInfo.h"
#include "core/style/ClipPathOperation.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/graphics/paint/PaintRecordBuilder.h"

namespace blink {

namespace {

class SVGClipExpansionCycleHelper {
 public:
  void Lock(LayoutSVGResourceClipper& clipper) {
    DCHECK(!clipper.HasCycle());
    clipper.BeginClipExpansion();
    clippers_.push_back(&clipper);
  }
  ~SVGClipExpansionCycleHelper() {
    for (auto* clipper : clippers_)
      clipper->EndClipExpansion();
  }

 private:
  Vector<LayoutSVGResourceClipper*, 1> clippers_;
};

LayoutSVGResourceClipper* ResolveElementReference(
    const LayoutObject& layout_object,
    const ReferenceClipPathOperation& reference_clip_path_operation) {
  if (layout_object.IsSVGChild()) {
    // The reference will have been resolved in
    // SVGResources::buildResources, so we can just use the LayoutObject's
    // SVGResources.
    SVGResources* resources =
        SVGResourcesCache::CachedResourcesForLayoutObject(&layout_object);
    return resources ? resources->Clipper() : nullptr;
  }
  // TODO(fs): Doesn't work with external SVG references (crbug.com/109212.)
  Node* target_node = layout_object.GetNode();
  if (!target_node)
    return nullptr;
  SVGElement* element =
      reference_clip_path_operation.FindElement(target_node->GetTreeScope());
  if (!IsSVGClipPathElement(element) || !element->GetLayoutObject())
    return nullptr;
  return ToLayoutSVGResourceClipper(
      ToLayoutSVGResourceContainer(element->GetLayoutObject()));
}

}  // namespace

FloatRect ClipPathClipper::LocalReferenceBox(const LayoutObject& object) {
  if (object.IsSVGChild())
    return object.ObjectBoundingBox();

  if (object.IsBox())
    return FloatRect(ToLayoutBox(object).BorderBoxRect());

  SECURITY_DCHECK(object.IsLayoutInline());
  const LayoutInline& layout_inline = ToLayoutInline(object);
  // This somewhat convoluted computation matches what Gecko does.
  // See crbug.com/641907.
  LayoutRect inline_b_box = layout_inline.LinesBoundingBox();
  const InlineFlowBox* flow_box = layout_inline.FirstLineBox();
  inline_b_box.SetHeight(flow_box ? flow_box->FrameRect().Height()
                                  : LayoutUnit(0));
  return FloatRect(inline_b_box);
}

Optional<FloatRect> ClipPathClipper::LocalClipPathBoundingBox(
    const LayoutObject& object) {
  if (object.IsText() || !object.StyleRef().ClipPath())
    return WTF::nullopt;

  FloatRect reference_box = LocalReferenceBox(object);
  ClipPathOperation& clip_path = *object.StyleRef().ClipPath();
  if (clip_path.GetType() == ClipPathOperation::SHAPE) {
    ShapeClipPathOperation& shape = ToShapeClipPathOperation(clip_path);
    if (!shape.IsValid())
      return WTF::nullopt;
    return shape.GetPath(reference_box).BoundingRect();
  }

  DCHECK_EQ(clip_path.GetType(), ClipPathOperation::REFERENCE);
  LayoutSVGResourceClipper* clipper =
      ResolveElementReference(object, ToReferenceClipPathOperation(clip_path));
  if (!clipper)
    return WTF::nullopt;

  FloatRect bounding_box = clipper->ResourceBoundingBox(reference_box);
  if (!object.IsSVGChild() &&
      clipper->ClipPathUnits() == SVGUnitTypes::kSvgUnitTypeUserspaceonuse) {
    bounding_box.Scale(clipper->StyleRef().EffectiveZoom());
    // With kSvgUnitTypeUserspaceonuse, the clip path layout is relative to
    // the current transform space, and the reference box is unused.
    // While SVG object has no concept of paint offset, HTML object's
    // local space is shifted by paint offset.
    bounding_box.MoveBy(reference_box.Location());
  }
  return bounding_box;
}

// Note: Return resolved LayoutSVGResourceClipper for caller's convenience,
// if the clip path is a reference to SVG.
static bool IsClipPathOperationValid(
    const ClipPathOperation& clip_path,
    const LayoutObject& search_scope,
    LayoutSVGResourceClipper*& resource_clipper) {
  if (clip_path.GetType() == ClipPathOperation::SHAPE) {
    if (!ToShapeClipPathOperation(clip_path).IsValid())
      return false;
  } else {
    DCHECK_EQ(clip_path.GetType(), ClipPathOperation::REFERENCE);
    resource_clipper = ResolveElementReference(
        search_scope, ToReferenceClipPathOperation(clip_path));
    if (!resource_clipper)
      return false;
    SECURITY_DCHECK(!resource_clipper->NeedsLayout());
    resource_clipper->ClearInvalidationMask();
    if (resource_clipper->HasCycle())
      return false;
  }
  return true;
}

ClipPathClipper::ClipPathClipper(GraphicsContext& context,
                                 const LayoutObject& layout_object,
                                 const LayoutPoint& paint_offset)
    : context_(context),
      layout_object_(layout_object),
      paint_offset_(paint_offset) {
  DCHECK(layout_object.StyleRef().ClipPath());

  if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled())
    return;

  Optional<FloatRect> bounding_box = LocalClipPathBoundingBox(layout_object);
  if (!bounding_box)
    return;

  FloatRect adjusted_bounding_box = *bounding_box;
  adjusted_bounding_box.MoveBy(FloatPoint(paint_offset));
  clip_recorder_.emplace(context, layout_object,
                         DisplayItem::kFloatClipClipPathBounds,
                         adjusted_bounding_box);

  bool is_valid = false;
  if (Optional<Path> as_path =
          PathBasedClip(layout_object, layout_object.IsSVGChild(),
                        LocalReferenceBox(layout_object), is_valid)) {
    as_path->Translate(FloatSize(paint_offset.X(), paint_offset.Y()));
    clip_path_recorder_.emplace(context, layout_object, *as_path);
  } else if (is_valid) {
    mask_isolation_recorder_.emplace(context, layout_object,
                                     SkBlendMode::kSrcOver, 1.f);
  }
}

static AffineTransform MaskToContentTransform(
    const LayoutSVGResourceClipper& resource_clipper,
    bool is_svg_child,
    const FloatRect& reference_box) {
  AffineTransform mask_to_content;
  if (resource_clipper.ClipPathUnits() ==
      SVGUnitTypes::kSvgUnitTypeUserspaceonuse) {
    if (!is_svg_child) {
      mask_to_content.Translate(reference_box.X(), reference_box.Y());
      mask_to_content.Scale(resource_clipper.StyleRef().EffectiveZoom());
    }
  }

  mask_to_content.Multiply(
      ToSVGClipPathElement(resource_clipper.GetElement())
          ->CalculateTransform(SVGElement::kIncludeMotionTransform));

  if (resource_clipper.ClipPathUnits() ==
      SVGUnitTypes::kSvgUnitTypeObjectboundingbox) {
    mask_to_content.Translate(reference_box.X(), reference_box.Y());
    mask_to_content.ScaleNonUniform(reference_box.Width(),
                                    reference_box.Height());
  }
  return mask_to_content;
}

ClipPathClipper::~ClipPathClipper() {
  if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled())
    return;
  if (!mask_isolation_recorder_)
    return;

  bool is_svg_child = layout_object_.IsSVGChild();
  FloatRect reference_box = LocalReferenceBox(layout_object_);

  CompositingRecorder mask_recorder(context_, layout_object_,
                                    SkBlendMode::kDstIn, 1.f);
  if (DrawingRecorder::UseCachedDrawingIfPossible(context_, layout_object_,
                                                  DisplayItem::kSVGClip))
    return;
  DrawingRecorder recorder(context_, layout_object_, DisplayItem::kSVGClip);
  context_.Save();
  context_.Translate(paint_offset_.X(), paint_offset_.Y());

  SVGClipExpansionCycleHelper locks;
  bool is_first = true;
  bool rest_of_the_chain_already_appled = false;
  const LayoutObject* current_object = &layout_object_;
  while (!rest_of_the_chain_already_appled && current_object) {
    const ClipPathOperation* clip_path = current_object->StyleRef().ClipPath();
    if (!clip_path)
      break;
    LayoutSVGResourceClipper* resource_clipper = nullptr;
    if (!IsClipPathOperationValid(*clip_path, *current_object,
                                  resource_clipper))
      break;

    if (is_first)
      context_.Save();
    else
      context_.BeginLayer(1.f, SkBlendMode::kDstIn);

    // We wouldn't have reached here if the current clip-path is a shape,
    // because it would have been applied as path-based clip already.
    DCHECK(resource_clipper);
    DCHECK_EQ(clip_path->GetType(), ClipPathOperation::REFERENCE);
    locks.Lock(*resource_clipper);
    if (resource_clipper->StyleRef().ClipPath()) {
      // Try to apply nested clip-path as path-based clip.
      bool unused;
      if (Optional<Path> path = PathBasedClip(*resource_clipper, is_svg_child,
                                              reference_box, unused)) {
        context_.ClipPath(path->GetSkPath(), kAntiAliased);
        rest_of_the_chain_already_appled = true;
      }
    }
    context_.ConcatCTM(
        MaskToContentTransform(*resource_clipper, is_svg_child, reference_box));
    context_.DrawRecord(resource_clipper->CreatePaintRecord());

    if (is_first)
      context_.Restore();
    else
      context_.EndLayer();

    is_first = false;
    current_object = resource_clipper;
  }
  context_.Restore();
}

Optional<Path> ClipPathClipper::PathBasedClip(
    const LayoutObject& clip_path_owner,
    bool is_svg_child,
    const FloatRect& reference_box,
    bool& is_valid) {
  const ClipPathOperation& clip_path = *clip_path_owner.StyleRef().ClipPath();
  LayoutSVGResourceClipper* resource_clipper = nullptr;
  is_valid =
      IsClipPathOperationValid(clip_path, clip_path_owner, resource_clipper);
  if (!is_valid)
    return WTF::nullopt;

  if (resource_clipper) {
    DCHECK_EQ(clip_path.GetType(), ClipPathOperation::REFERENCE);
    Optional<Path> path = resource_clipper->AsPath();
    if (!path)
      return path;
    path->Transform(
        MaskToContentTransform(*resource_clipper, is_svg_child, reference_box));
    return path;
  }

  DCHECK_EQ(clip_path.GetType(), ClipPathOperation::SHAPE);
  auto& shape = ToShapeClipPathOperation(clip_path);
  return Optional<Path>(shape.GetPath(reference_box));
}

}  // namespace blink
