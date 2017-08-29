/*
 * Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2010 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/svg/SVGSVGElement.h"

#include "bindings/core/v8/ScriptEventListener.h"
#include "core/HTMLNames.h"
#include "core/SVGNames.h"
#include "core/css/CSSHelper.h"
#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/StaticNodeList.h"
#include "core/dom/StyleChangeReason.h"
#include "core/dom/events/EventListener.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/Deprecation.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/svg/LayoutSVGModelObject.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/layout/svg/LayoutSVGViewportContainer.h"
#include "core/svg/SVGAngleTearOff.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGLengthTearOff.h"
#include "core/svg/SVGMatrixTearOff.h"
#include "core/svg/SVGNumberTearOff.h"
#include "core/svg/SVGPointTearOff.h"
#include "core/svg/SVGPreserveAspectRatio.h"
#include "core/svg/SVGRectTearOff.h"
#include "core/svg/SVGTransform.h"
#include "core/svg/SVGTransformList.h"
#include "core/svg/SVGTransformTearOff.h"
#include "core/svg/SVGViewElement.h"
#include "core/svg/SVGViewSpec.h"
#include "core/svg/animation/SMILTimeContainer.h"
#include "platform/LengthFunctions.h"
#include "platform/geometry/FloatRect.h"
#include "platform/transforms/AffineTransform.h"
#include "platform/wtf/MathExtras.h"
#include "platform/wtf/StdLibExtras.h"

namespace blink {

inline SVGSVGElement::SVGSVGElement(Document& doc)
    : SVGGraphicsElement(SVGNames::svgTag, doc),
      SVGFitToViewBox(this),
      x_(SVGAnimatedLength::Create(this,
                                   SVGNames::xAttr,
                                   SVGLength::Create(SVGLengthMode::kWidth),
                                   CSSPropertyX)),
      y_(SVGAnimatedLength::Create(this,
                                   SVGNames::yAttr,
                                   SVGLength::Create(SVGLengthMode::kHeight),
                                   CSSPropertyY)),
      width_(SVGAnimatedLength::Create(this,
                                       SVGNames::widthAttr,
                                       SVGLength::Create(SVGLengthMode::kWidth),
                                       CSSPropertyWidth)),
      height_(
          SVGAnimatedLength::Create(this,
                                    SVGNames::heightAttr,
                                    SVGLength::Create(SVGLengthMode::kHeight),
                                    CSSPropertyHeight)),
      time_container_(SMILTimeContainer::Create(*this)),
      translation_(SVGPoint::Create()),
      current_scale_(1) {
  width_->SetDefaultValueAsString("100%");
  height_->SetDefaultValueAsString("100%");

  AddToPropertyMap(x_);
  AddToPropertyMap(y_);
  AddToPropertyMap(width_);
  AddToPropertyMap(height_);

  UseCounter::Count(doc, WebFeature::kSVGSVGElement);
}

DEFINE_NODE_FACTORY(SVGSVGElement)

SVGSVGElement::~SVGSVGElement() {}

float SVGSVGElement::currentScale() const {
  if (!isConnected() || !IsOutermostSVGSVGElement())
    return 1;

  return current_scale_;
}

void SVGSVGElement::setCurrentScale(float scale) {
  DCHECK(std::isfinite(scale));
  if (!isConnected() || !IsOutermostSVGSVGElement())
    return;

  current_scale_ = scale;
  UpdateUserTransform();
}

class SVGCurrentTranslateTearOff : public SVGPointTearOff {
 public:
  static SVGCurrentTranslateTearOff* Create(SVGSVGElement* context_element) {
    return new SVGCurrentTranslateTearOff(context_element);
  }

  void CommitChange() override {
    DCHECK(contextElement());
    toSVGSVGElement(contextElement())->UpdateUserTransform();
  }

 private:
  SVGCurrentTranslateTearOff(SVGSVGElement* context_element)
      : SVGPointTearOff(context_element->translation_,
                        context_element,
                        kPropertyIsNotAnimVal,
                        QualifiedName::Null()) {}
};

SVGPointTearOff* SVGSVGElement::currentTranslateFromJavascript() {
  return SVGCurrentTranslateTearOff::Create(this);
}

void SVGSVGElement::SetCurrentTranslate(const FloatPoint& point) {
  translation_->SetValue(point);
  UpdateUserTransform();
}

void SVGSVGElement::UpdateUserTransform() {
  if (LayoutObject* object = GetLayoutObject())
    object->SetNeedsLayoutAndFullPaintInvalidation(
        LayoutInvalidationReason::kUnknown);
}

bool SVGSVGElement::ZoomAndPanEnabled() const {
  SVGZoomAndPanType zoom_and_pan = this->zoomAndPan();
  if (view_spec_)
    zoom_and_pan = view_spec_->zoomAndPan();
  return zoom_and_pan == kSVGZoomAndPanMagnify;
}

void SVGSVGElement::ParseAttribute(const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  const AtomicString& value = params.new_value;
  if (!nearestViewportElement()) {
    bool set_listener = true;

    // Only handle events if we're the outermost <svg> element
    if (name == HTMLNames::onunloadAttr) {
      GetDocument().SetWindowAttributeEventListener(
          EventTypeNames::unload,
          CreateAttributeEventListener(GetDocument().GetFrame(), name, value,
                                       EventParameterName()));
    } else if (name == HTMLNames::onresizeAttr) {
      GetDocument().SetWindowAttributeEventListener(
          EventTypeNames::resize,
          CreateAttributeEventListener(GetDocument().GetFrame(), name, value,
                                       EventParameterName()));
    } else if (name == HTMLNames::onscrollAttr) {
      GetDocument().SetWindowAttributeEventListener(
          EventTypeNames::scroll,
          CreateAttributeEventListener(GetDocument().GetFrame(), name, value,
                                       EventParameterName()));
    } else {
      set_listener = false;
    }

    if (set_listener)
      return;
  }

  if (name == HTMLNames::onabortAttr) {
    GetDocument().SetWindowAttributeEventListener(
        EventTypeNames::abort,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value,
                                     EventParameterName()));
  } else if (name == HTMLNames::onerrorAttr) {
    GetDocument().SetWindowAttributeEventListener(
        EventTypeNames::error,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value,
                                     EventParameterName()));
  } else if (SVGZoomAndPan::ParseAttribute(name, value)) {
  } else if (name == SVGNames::widthAttr || name == SVGNames::heightAttr) {
    SVGAnimatedLength* property =
        name == SVGNames::widthAttr ? width_ : height_;
    SVGParsingError parse_error;
    if (!value.IsNull())
      parse_error = property->SetBaseValueAsString(value);
    if (parse_error != SVGParseStatus::kNoError || value.IsNull())
      property->SetDefaultValueAsString("100%");
    ReportAttributeParsingError(parse_error, name, value);
  } else {
    SVGElement::ParseAttribute(params);
  }
}

bool SVGSVGElement::IsPresentationAttribute(const QualifiedName& name) const {
  if ((name == SVGNames::widthAttr || name == SVGNames::heightAttr) &&
      !IsOutermostSVGSVGElement())
    return false;
  return SVGGraphicsElement::IsPresentationAttribute(name);
}

bool SVGSVGElement::IsPresentationAttributeWithSVGDOM(
    const QualifiedName& attr_name) const {
  if (attr_name == SVGNames::widthAttr || attr_name == SVGNames::heightAttr)
    return false;
  return SVGGraphicsElement::IsPresentationAttributeWithSVGDOM(attr_name);
}

void SVGSVGElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableStylePropertySet* style) {
  SVGAnimatedPropertyBase* property = PropertyFromAttribute(name);
  if (property == x_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            x_->CssValue());
  } else if (property == y_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            y_->CssValue());
  } else if (IsOutermostSVGSVGElement() &&
             (property == width_ || property == height_)) {
    if (property == width_) {
      AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                              width_->CssValue());
    } else if (property == height_) {
      AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                              height_->CssValue());
    }
  } else {
    SVGGraphicsElement::CollectStyleForPresentationAttribute(name, value,
                                                             style);
  }
}

void SVGSVGElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  bool update_relative_lengths_or_view_box = false;
  bool width_or_height_changed =
      attr_name == SVGNames::widthAttr || attr_name == SVGNames::heightAttr;
  if (width_or_height_changed || attr_name == SVGNames::xAttr ||
      attr_name == SVGNames::yAttr) {
    update_relative_lengths_or_view_box = true;
    UpdateRelativeLengthsInformation();
    InvalidateRelativeLengthClients();

    // At the SVG/HTML boundary (aka LayoutSVGRoot), the width and
    // height attributes can affect the replaced size so we need
    // to mark it for updating.
    if (width_or_height_changed) {
      LayoutObject* layout_object = this->GetLayoutObject();
      // If the element is not attached, we cannot be sure if it is (going to
      // be) an outermost root, so always mark presentation attributes dirty in
      // that case.
      if (!layout_object || layout_object->IsSVGRoot()) {
        InvalidateSVGPresentationAttributeStyle();
        SetNeedsStyleRecalc(kLocalStyleChange,
                            StyleChangeReasonForTracing::Create(
                                StyleChangeReason::kSVGContainerSizeChange));
      }
    } else {
      InvalidateSVGPresentationAttributeStyle();
      SetNeedsStyleRecalc(
          kLocalStyleChange,
          StyleChangeReasonForTracing::FromAttribute(attr_name));
    }
  }

  if (SVGFitToViewBox::IsKnownAttribute(attr_name)) {
    update_relative_lengths_or_view_box = true;
    InvalidateRelativeLengthClients();
    if (LayoutObject* object = GetLayoutObject())
      object->SetNeedsTransformUpdate();
  }

  if (update_relative_lengths_or_view_box ||
      SVGZoomAndPan::IsKnownAttribute(attr_name)) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    if (auto* layout_object = this->GetLayoutObject())
      MarkForLayoutAndParentResourceInvalidation(layout_object);
    return;
  }

  SVGGraphicsElement::SvgAttributeChanged(attr_name);
}

// FloatRect::intersects does not consider horizontal or vertical lines (because
// of isEmpty()).
static bool IntersectsAllowingEmpty(const FloatRect& r1, const FloatRect& r2) {
  if (r1.Width() < 0 || r1.Height() < 0 || r2.Width() < 0 || r2.Height() < 0)
    return false;

  return r1.X() < r2.MaxX() && r2.X() < r1.MaxX() && r1.Y() < r2.MaxY() &&
         r2.Y() < r1.MaxY();
}

// One of the element types that can cause graphics to be drawn onto the target
// canvas.  Specifically: circle, ellipse, image, line, path, polygon, polyline,
// rect, text and use.
static bool IsIntersectionOrEnclosureTarget(LayoutObject* layout_object) {
  return layout_object->IsSVGShape() || layout_object->IsSVGText() ||
         layout_object->IsSVGImage() ||
         isSVGUseElement(*layout_object->GetNode());
}

bool SVGSVGElement::CheckIntersectionOrEnclosure(
    const SVGElement& element,
    const FloatRect& rect,
    GeometryMatchingMode mode) const {
  LayoutObject* layout_object = element.GetLayoutObject();
  DCHECK(!layout_object || layout_object->Style());
  if (!layout_object ||
      layout_object->Style()->PointerEvents() == EPointerEvents::kNone)
    return false;

  if (!IsIntersectionOrEnclosureTarget(layout_object))
    return false;

  AffineTransform ctm =
      ToSVGGraphicsElement(element).ComputeCTM(kAncestorScope, this);
  FloatRect mapped_repaint_rect =
      ctm.MapRect(layout_object->VisualRectInLocalSVGCoordinates());

  bool result = false;
  switch (mode) {
    case kCheckIntersection:
      result = IntersectsAllowingEmpty(rect, mapped_repaint_rect);
      break;
    case kCheckEnclosure:
      result = rect.Contains(mapped_repaint_rect);
      break;
    default:
      NOTREACHED();
      break;
  }

  return result;
}

StaticNodeList* SVGSVGElement::CollectIntersectionOrEnclosureList(
    const FloatRect& rect,
    SVGElement* reference_element,
    GeometryMatchingMode mode) const {
  HeapVector<Member<Node>> nodes;

  const SVGElement* root = this;
  if (reference_element) {
    // Only the common subtree needs to be traversed.
    if (contains(reference_element)) {
      root = reference_element;
    } else if (!IsDescendantOf(reference_element)) {
      // No common subtree.
      return StaticNodeList::Adopt(nodes);
    }
  }

  for (SVGGraphicsElement& element :
       Traversal<SVGGraphicsElement>::DescendantsOf(*root)) {
    if (CheckIntersectionOrEnclosure(element, rect, mode))
      nodes.push_back(&element);
  }

  return StaticNodeList::Adopt(nodes);
}

StaticNodeList* SVGSVGElement::getIntersectionList(
    SVGRectTearOff* rect,
    SVGElement* reference_element) const {
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  return CollectIntersectionOrEnclosureList(
      rect->Target()->Value(), reference_element, kCheckIntersection);
}

StaticNodeList* SVGSVGElement::getEnclosureList(
    SVGRectTearOff* rect,
    SVGElement* reference_element) const {
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  return CollectIntersectionOrEnclosureList(rect->Target()->Value(),
                                            reference_element, kCheckEnclosure);
}

bool SVGSVGElement::checkIntersection(SVGElement* element,
                                      SVGRectTearOff* rect) const {
  DCHECK(element);
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  return CheckIntersectionOrEnclosure(*element, rect->Target()->Value(),
                                      kCheckIntersection);
}

bool SVGSVGElement::checkEnclosure(SVGElement* element,
                                   SVGRectTearOff* rect) const {
  DCHECK(element);
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  return CheckIntersectionOrEnclosure(*element, rect->Target()->Value(),
                                      kCheckEnclosure);
}

void SVGSVGElement::deselectAll() {
  if (LocalFrame* frame = GetDocument().GetFrame())
    frame->Selection().Clear();
}

SVGNumberTearOff* SVGSVGElement::createSVGNumber() {
  return SVGNumberTearOff::CreateDetached();
}

SVGLengthTearOff* SVGSVGElement::createSVGLength() {
  return SVGLengthTearOff::CreateDetached();
}

SVGAngleTearOff* SVGSVGElement::createSVGAngle() {
  return SVGAngleTearOff::CreateDetached();
}

SVGPointTearOff* SVGSVGElement::createSVGPoint() {
  return SVGPointTearOff::CreateDetached(FloatPoint(0, 0));
}

SVGMatrixTearOff* SVGSVGElement::createSVGMatrix() {
  return SVGMatrixTearOff::Create(AffineTransform());
}

SVGRectTearOff* SVGSVGElement::createSVGRect() {
  return SVGRectTearOff::CreateDetached(FloatRect(0, 0, 0, 0));
}

SVGTransformTearOff* SVGSVGElement::createSVGTransform() {
  return SVGTransformTearOff::CreateDetached();
}

SVGTransformTearOff* SVGSVGElement::createSVGTransformFromMatrix(
    SVGMatrixTearOff* matrix) {
  return SVGTransformTearOff::Create(matrix);
}

AffineTransform SVGSVGElement::LocalCoordinateSpaceTransform(
    CTMScope mode) const {
  AffineTransform transform;
  if (!IsOutermostSVGSVGElement()) {
    SVGLengthContext length_context(this);
    transform.Translate(x_->CurrentValue()->Value(length_context),
                        y_->CurrentValue()->Value(length_context));
  } else if (mode == kScreenScope) {
    if (LayoutObject* layout_object = this->GetLayoutObject()) {
      TransformationMatrix transform;
      // Adjust for the zoom level factored into CSS coordinates (WK bug
      // #96361).
      transform.Scale(1.0 / layout_object->StyleRef().EffectiveZoom());

      // Origin in the document. (This, together with the inverse-scale above,
      // performs the same operation as
      // Document::adjustFloatRectForScrollAndAbsoluteZoom, but in
      // transformation matrix form.)
      if (LocalFrameView* view = GetDocument().View()) {
        LayoutRect visible_content_rect(view->VisibleContentRect());
        transform.Translate(-visible_content_rect.X(),
                            -visible_content_rect.Y());
      }

      // Apply transforms from our ancestor coordinate space, including any
      // non-SVG ancestor transforms.
      transform.Multiply(layout_object->LocalToAbsoluteTransform());

      // At the SVG/HTML boundary (aka LayoutSVGRoot), we need to apply the
      // localToBorderBoxTransform to map an element from SVG viewport
      // coordinates to CSS box coordinates.
      transform.Multiply(
          ToLayoutSVGRoot(layout_object)->LocalToBorderBoxTransform());
      // Drop any potential non-affine parts, because we're not able to convey
      // that information further anyway until getScreenCTM returns a DOMMatrix
      // (4x4 matrix.)
      return transform.ToAffineTransform();
    }
  }
  if (!HasEmptyViewBox()) {
    FloatSize size = CurrentViewportSize();
    transform.Multiply(ViewBoxToViewTransform(size.Width(), size.Height()));
  }
  return transform;
}

bool SVGSVGElement::LayoutObjectIsNeeded(const ComputedStyle& style) {
  // FIXME: We should respect display: none on the documentElement svg element
  // but many things in LocalFrameView and SVGImage depend on the LayoutSVGRoot
  // when they should instead depend on the LayoutView.
  // https://bugs.webkit.org/show_bug.cgi?id=103493
  if (GetDocument().documentElement() == this)
    return true;

  // <svg> elements don't need an SVG parent to render, so we bypass
  // SVGElement::layoutObjectIsNeeded.
  return IsValid() && Element::LayoutObjectIsNeeded(style);
}

LayoutObject* SVGSVGElement::CreateLayoutObject(const ComputedStyle&) {
  if (IsOutermostSVGSVGElement())
    return new LayoutSVGRoot(this);

  return new LayoutSVGViewportContainer(this);
}

Node::InsertionNotificationRequest SVGSVGElement::InsertedInto(
    ContainerNode* root_parent) {
  if (root_parent->isConnected()) {
    UseCounter::Count(GetDocument(), WebFeature::kSVGSVGElementInDocument);
    if (root_parent->GetDocument().IsXMLDocument())
      UseCounter::Count(GetDocument(), WebFeature::kSVGSVGElementInXMLDocument);

    if (RuntimeEnabledFeatures::SMILEnabled()) {
      GetDocument().AccessSVGExtensions().AddTimeContainer(this);

      // Animations are started at the end of document parsing and after firing
      // the load event, but if we miss that train (deferred programmatic
      // element insertion for example) we need to initialize the time container
      // here.
      if (!GetDocument().Parsing() && GetDocument().LoadEventFinished() &&
          !TimeContainer()->IsStarted())
        TimeContainer()->Start();
    }
  }
  return SVGGraphicsElement::InsertedInto(root_parent);
}

void SVGSVGElement::RemovedFrom(ContainerNode* root_parent) {
  if (root_parent->isConnected()) {
    SVGDocumentExtensions& svg_extensions = GetDocument().AccessSVGExtensions();
    svg_extensions.RemoveTimeContainer(this);
    svg_extensions.RemoveSVGRootWithRelativeLengthDescendents(this);
  }

  SVGGraphicsElement::RemovedFrom(root_parent);
}

void SVGSVGElement::pauseAnimations() {
  if (!time_container_->IsPaused())
    time_container_->Pause();
}

void SVGSVGElement::unpauseAnimations() {
  if (time_container_->IsPaused())
    time_container_->Resume();
}

bool SVGSVGElement::animationsPaused() const {
  return time_container_->IsPaused();
}

float SVGSVGElement::getCurrentTime() const {
  return clampTo<float>(time_container_->Elapsed());
}

void SVGSVGElement::setCurrentTime(float seconds) {
  DCHECK(std::isfinite(seconds));
  seconds = max(seconds, 0.0f);
  time_container_->SetElapsed(seconds);
}

bool SVGSVGElement::SelfHasRelativeLengths() const {
  return x_->CurrentValue()->IsRelative() || y_->CurrentValue()->IsRelative() ||
         width_->CurrentValue()->IsRelative() ||
         height_->CurrentValue()->IsRelative();
}

bool SVGSVGElement::ShouldSynthesizeViewBox() const {
  return GetLayoutObject() && GetLayoutObject()->IsSVGRoot() &&
         ToLayoutSVGRoot(GetLayoutObject())->IsEmbeddedThroughSVGImage();
}

FloatRect SVGSVGElement::CurrentViewBoxRect() const {
  if (view_spec_)
    return view_spec_->ViewBox()->Value();

  FloatRect use_view_box = viewBox()->CurrentValue()->Value();
  if (!use_view_box.IsEmpty())
    return use_view_box;
  if (!ShouldSynthesizeViewBox())
    return FloatRect();

  // If no viewBox is specified but non-relative width/height values, then we
  // should always synthesize a viewBox if we're embedded through a SVGImage.
  FloatSize synthesized_view_box_size(IntrinsicWidth(), IntrinsicHeight());
  if (!HasIntrinsicWidth())
    synthesized_view_box_size.SetWidth(
        width()->CurrentValue()->ScaleByPercentage(
            CurrentViewportSize().Width()));
  if (!HasIntrinsicHeight())
    synthesized_view_box_size.SetHeight(
        height()->CurrentValue()->ScaleByPercentage(
            CurrentViewportSize().Height()));
  return FloatRect(FloatPoint(), synthesized_view_box_size);
}

SVGPreserveAspectRatio* SVGSVGElement::CurrentPreserveAspectRatio() const {
  if (view_spec_)
    return view_spec_->PreserveAspectRatio();

  if (!HasValidViewBox() && ShouldSynthesizeViewBox()) {
    // If no (valid) viewBox is specified and we're embedded through SVGImage,
    // then synthesize a pAR with the value 'none'.
    SVGPreserveAspectRatio* synthesized_par = SVGPreserveAspectRatio::Create();
    synthesized_par->SetAlign(
        SVGPreserveAspectRatio::kSvgPreserveaspectratioNone);
    return synthesized_par;
  }
  return preserveAspectRatio()->CurrentValue();
}

FloatSize SVGSVGElement::CurrentViewportSize() const {
  if (!GetLayoutObject())
    return FloatSize();

  if (GetLayoutObject()->IsSVGRoot()) {
    LayoutRect content_box_rect =
        ToLayoutSVGRoot(GetLayoutObject())->ContentBoxRect();
    return FloatSize(
        content_box_rect.Width() / GetLayoutObject()->Style()->EffectiveZoom(),
        content_box_rect.Height() /
            GetLayoutObject()->Style()->EffectiveZoom());
  }

  FloatRect viewport_rect =
      ToLayoutSVGViewportContainer(GetLayoutObject())->Viewport();
  return FloatSize(viewport_rect.Width(), viewport_rect.Height());
}

bool SVGSVGElement::HasIntrinsicWidth() const {
  return width()->CurrentValue()->TypeWithCalcResolved() !=
         CSSPrimitiveValue::UnitType::kPercentage;
}

bool SVGSVGElement::HasIntrinsicHeight() const {
  return height()->CurrentValue()->TypeWithCalcResolved() !=
         CSSPrimitiveValue::UnitType::kPercentage;
}

float SVGSVGElement::IntrinsicWidth() const {
  if (width()->CurrentValue()->TypeWithCalcResolved() ==
      CSSPrimitiveValue::UnitType::kPercentage)
    return 0;

  SVGLengthContext length_context(this);
  return width()->CurrentValue()->Value(length_context);
}

float SVGSVGElement::IntrinsicHeight() const {
  if (height()->CurrentValue()->TypeWithCalcResolved() ==
      CSSPrimitiveValue::UnitType::kPercentage)
    return 0;

  SVGLengthContext length_context(this);
  return height()->CurrentValue()->Value(length_context);
}

AffineTransform SVGSVGElement::ViewBoxToViewTransform(float view_width,
                                                      float view_height) const {
  AffineTransform ctm = SVGFitToViewBox::ViewBoxToViewTransform(
      CurrentViewBoxRect(), CurrentPreserveAspectRatio(), view_width,
      view_height);
  if (!view_spec_)
    return ctm;

  SVGTransformList* transform_list = view_spec_->Transform();
  if (transform_list->IsEmpty())
    return ctm;

  AffineTransform transform;
  if (transform_list->Concatenate(transform))
    ctm *= transform;

  return ctm;
}

void SVGSVGElement::SetViewSpec(SVGViewSpec* view_spec) {
  // Even if the viewspec object itself doesn't change, it could still
  // have been mutated, so only treat a "no viewspec" -> "no viewspec"
  // transition as a no-op.
  if (!view_spec_ && !view_spec)
    return;
  view_spec_ = view_spec;
  if (LayoutObject* layout_object = this->GetLayoutObject())
    MarkForLayoutAndParentResourceInvalidation(layout_object);
}

void SVGSVGElement::SetupInitialView(const String& fragment_identifier,
                                     Element* anchor_node) {
  if (fragment_identifier.StartsWith("svgView(")) {
    SVGViewSpec* view_spec = SVGViewSpec::CreateForElement(*this);
    if (view_spec->ParseViewSpec(fragment_identifier)) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kSVGSVGElementFragmentSVGView);
      SetViewSpec(view_spec);
      return;
    }
  }

  SetViewSpec(nullptr);

  if (!isSVGViewElement(anchor_node))
    return;

  SVGViewElement& view_element = toSVGViewElement(*anchor_node);

  // Spec: If the SVG fragment identifier addresses a 'view' element
  // within an SVG document (e.g., MyDrawing.svg#MyView) then the
  // closest ancestor 'svg' element is displayed in the
  // viewport. Any view specification attributes included on the
  // given 'view' element override the corresponding view
  // specification attributes on the closest ancestor 'svg' element.
  // TODO(ed): The spec text above is a bit unclear.
  // Should the transform from outermost svg to nested svg be applied to
  // "display" the inner svg in the viewport, then let the view element
  // override the inner svg's view specification attributes. Should it
  // fill/override the outer viewport?
  SVGSVGElement* svg = view_element.ownerSVGElement();
  if (!svg)
    return;
  SVGViewSpec* view_spec = SVGViewSpec::CreateForElement(*svg);
  view_spec->InheritViewAttributesFromElement(view_element);
  UseCounter::Count(svg->GetDocument(),
                    WebFeature::kSVGSVGElementFragmentSVGViewElement);
  svg->SetViewSpec(view_spec);
}

void SVGSVGElement::FinishParsingChildren() {
  SVGGraphicsElement::FinishParsingChildren();

  // The outermost SVGSVGElement SVGLoad event is fired through
  // LocalDOMWindow::dispatchWindowLoadEvent.
  if (IsOutermostSVGSVGElement())
    return;

  // finishParsingChildren() is called when the close tag is reached for an
  // element (e.g. </svg>) we send SVGLoad events here if we can, otherwise
  // they'll be sent when any required loads finish
  SendSVGLoadEventIfPossible();
}

DEFINE_TRACE(SVGSVGElement) {
  visitor->Trace(x_);
  visitor->Trace(y_);
  visitor->Trace(width_);
  visitor->Trace(height_);
  visitor->Trace(translation_);
  visitor->Trace(time_container_);
  visitor->Trace(view_spec_);
  SVGGraphicsElement::Trace(visitor);
  SVGFitToViewBox::Trace(visitor);
}

}  // namespace blink
