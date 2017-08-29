/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann
 * <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2009 Cameron McCormack <cam@mcc.id.au>
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

#include "core/svg/SVGElement.h"

#include "bindings/core/v8/ScriptEventListener.h"
#include "core/HTMLNames.h"
#include "core/SVGNames.h"
#include "core/XMLNames.h"
#include "core/animation/DocumentAnimations.h"
#include "core/animation/EffectStack.h"
#include "core/animation/ElementAnimations.h"
#include "core/animation/InvalidatableInterpolation.h"
#include "core/animation/SVGInterpolationEnvironment.h"
#include "core/animation/SVGInterpolationTypesMap.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/events/Event.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLElement.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/svg/LayoutSVGResourceContainer.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGElementRareData.h"
#include "core/svg/SVGGraphicsElement.h"
#include "core/svg/SVGSVGElement.h"
#include "core/svg/SVGTitleElement.h"
#include "core/svg/SVGTreeScopeResources.h"
#include "core/svg/SVGUseElement.h"
#include "core/svg/properties/SVGProperty.h"
#include "platform/wtf/AutoReset.h"
#include "platform/wtf/Threading.h"

namespace blink {

using namespace HTMLNames;
using namespace SVGNames;

SVGElement::SVGElement(const QualifiedName& tag_name,
                       Document& document,
                       ConstructionType construction_type)
    : Element(tag_name, &document, construction_type),
      svg_rare_data_(nullptr),
      class_name_(SVGAnimatedString::Create(this, HTMLNames::classAttr)) {
  AddToPropertyMap(class_name_);
  SetHasCustomStyleCallbacks();
}

SVGElement::~SVGElement() {
  DCHECK(isConnected() || !HasRelativeLengths());
}

void SVGElement::DetachLayoutTree(const AttachContext& context) {
  Element::DetachLayoutTree(context);
  if (SVGElement* element = CorrespondingElement())
    element->RemoveInstanceMapping(this);
}

void SVGElement::AttachLayoutTree(AttachContext& context) {
  Element::AttachLayoutTree(context);
  if (SVGElement* element = CorrespondingElement())
    element->MapInstanceToElement(this);
}

TreeScope& SVGElement::TreeScopeForIdResolution() const {
  const SVGElement* tree_scope_element = this;
  if (const SVGElement* element = CorrespondingUseElement())
    tree_scope_element = element;
  return tree_scope_element->GetTreeScope();
}

int SVGElement::tabIndex() const {
  if (SupportsFocus())
    return Element::tabIndex();
  return -1;
}

void SVGElement::WillRecalcStyle(StyleRecalcChange change) {
  if (!HasSVGRareData())
    return;
  // If the style changes because of a regular property change (not induced by
  // SMIL animations themselves) reset the "computed style without SMIL style
  // properties", so the base value change gets reflected.
  if (change > kNoChange || NeedsStyleRecalc())
    SvgRareData()->SetNeedsOverrideComputedStyleUpdate();
}

void SVGElement::BuildPendingResourcesIfNeeded() {
  if (!NeedsPendingResourceHandling() || !isConnected() || InUseShadowTree())
    return;
  GetTreeScope().EnsureSVGTreeScopedResources().NotifyResourceAvailable(
      GetIdAttribute());
}

SVGElementRareData* SVGElement::EnsureSVGRareData() {
  if (HasSVGRareData())
    return SvgRareData();

  svg_rare_data_ = new SVGElementRareData(this);
  return svg_rare_data_.Get();
}

bool SVGElement::IsOutermostSVGSVGElement() const {
  if (!isSVGSVGElement(*this))
    return false;

  // Element may not be in the document, pretend we're outermost for viewport(),
  // getCTM(), etc.
  if (!parentNode())
    return true;

  // We act like an outermost SVG element, if we're a direct child of a
  // <foreignObject> element.
  if (isSVGForeignObjectElement(*parentNode()))
    return true;

  // If we're living in a shadow tree, we're a <svg> element that got created as
  // replacement for a <symbol> element or a cloned <svg> element in the
  // referenced tree. In that case we're always an inner <svg> element.
  if (InUseShadowTree() && ParentOrShadowHostElement() &&
      ParentOrShadowHostElement()->IsSVGElement())
    return false;

  // This is true whenever this is the outermost SVG, even if there are HTML
  // elements outside it
  return !parentNode()->IsSVGElement();
}

void SVGElement::ReportAttributeParsingError(SVGParsingError error,
                                             const QualifiedName& name,
                                             const AtomicString& value) {
  if (error == SVGParseStatus::kNoError)
    return;
  // Don't report any errors on attribute removal.
  if (value.IsNull())
    return;
  GetDocument().AddConsoleMessage(
      ConsoleMessage::Create(kRenderingMessageSource, kErrorMessageLevel,
                             "Error: " + error.Format(tagName(), name, value)));
}

String SVGElement::title() const {
  // According to spec, we should not return titles when hovering over root
  // <svg> elements (those <title> elements are the title of the document, not a
  // tooltip) so we instantly return.
  if (IsOutermostSVGSVGElement())
    return String();

  if (InUseShadowTree()) {
    String use_title(OwnerShadowHost()->title());
    if (!use_title.IsEmpty())
      return use_title;
  }

  // If we aren't an instance in a <use> or the <use> title was not found, then
  // find the first <title> child of this element.
  // If a title child was found, return the text contents.
  if (Element* title_element = Traversal<SVGTitleElement>::FirstChild(*this))
    return title_element->innerText();

  // Otherwise return a null/empty string.
  return String();
}

bool SVGElement::InstanceUpdatesBlocked() const {
  return HasSVGRareData() && SvgRareData()->InstanceUpdatesBlocked();
}

void SVGElement::SetInstanceUpdatesBlocked(bool value) {
  if (HasSVGRareData())
    SvgRareData()->SetInstanceUpdatesBlocked(value);
}

void SVGElement::SetWebAnimationsPending() {
  GetDocument().AccessSVGExtensions().AddWebAnimationsPendingSVGElement(*this);
  EnsureSVGRareData()->SetWebAnimatedAttributesDirty(true);
  EnsureUniqueElementData().animated_svg_attributes_are_dirty_ = true;
}

static bool IsSVGAttributeHandle(const PropertyHandle& property_handle) {
  return property_handle.IsSVGAttribute();
}

void SVGElement::ApplyActiveWebAnimations() {
  ActiveInterpolationsMap active_interpolations_map =
      EffectStack::ActiveInterpolations(
          &GetElementAnimations()->GetEffectStack(), nullptr, nullptr,
          KeyframeEffectReadOnly::kDefaultPriority, IsSVGAttributeHandle);
  for (auto& entry : active_interpolations_map) {
    const QualifiedName& attribute = entry.key.SvgAttribute();
    SVGInterpolationTypesMap map;
    SVGInterpolationEnvironment environment(
        map, *this, PropertyFromAttribute(attribute)->BaseValueBase());
    InvalidatableInterpolation::ApplyStack(entry.value, environment);
  }
  if (!HasSVGRareData())
    return;
  SvgRareData()->SetWebAnimatedAttributesDirty(false);
}

static inline void NotifyAnimValChanged(SVGElement* target_element,
                                        const QualifiedName& attribute_name) {
  target_element->InvalidateSVGAttributes();
  target_element->SvgAttributeChanged(attribute_name);
}

template <typename T>
static void ForSelfAndInstances(SVGElement* element, T callback) {
  SVGElement::InstanceUpdateBlocker blocker(element);
  callback(element);
  for (SVGElement* instance : element->InstancesForElement())
    callback(instance);
}

void SVGElement::SetWebAnimatedAttribute(const QualifiedName& attribute,
                                         SVGPropertyBase* value) {
  ForSelfAndInstances(this, [&attribute, &value](SVGElement* element) {
    if (SVGAnimatedPropertyBase* animated_property =
            element->PropertyFromAttribute(attribute)) {
      animated_property->SetAnimatedValue(value);
      NotifyAnimValChanged(element, attribute);
    }
  });
  EnsureSVGRareData()->WebAnimatedAttributes().insert(&attribute);
}

void SVGElement::ClearWebAnimatedAttributes() {
  if (!HasSVGRareData())
    return;
  for (const QualifiedName* attribute :
       SvgRareData()->WebAnimatedAttributes()) {
    ForSelfAndInstances(this, [&attribute](SVGElement* element) {
      if (SVGAnimatedPropertyBase* animated_property =
              element->PropertyFromAttribute(*attribute)) {
        animated_property->AnimationEnded();
        NotifyAnimValChanged(element, *attribute);
      }
    });
  }
  SvgRareData()->WebAnimatedAttributes().clear();
}

void SVGElement::SetAnimatedAttribute(const QualifiedName& attribute,
                                      SVGPropertyBase* value) {
  ForSelfAndInstances(this, [&attribute, &value](SVGElement* element) {
    if (SVGAnimatedPropertyBase* animated_property =
            element->PropertyFromAttribute(attribute))
      animated_property->SetAnimatedValue(value);
  });
}

void SVGElement::InvalidateAnimatedAttribute(const QualifiedName& attribute) {
  ForSelfAndInstances(this, [&attribute](SVGElement* element) {
    NotifyAnimValChanged(element, attribute);
  });
}

void SVGElement::ClearAnimatedAttribute(const QualifiedName& attribute) {
  ForSelfAndInstances(this, [&attribute](SVGElement* element) {
    if (SVGAnimatedPropertyBase* animated_property =
            element->PropertyFromAttribute(attribute))
      animated_property->AnimationEnded();
  });
}

AffineTransform SVGElement::LocalCoordinateSpaceTransform(CTMScope) const {
  // To be overriden by SVGGraphicsElement (or as special case SVGTextElement
  // and SVGPatternElement)
  return AffineTransform();
}

bool SVGElement::HasTransform(
    ApplyMotionTransform apply_motion_transform) const {
  return (GetLayoutObject() && GetLayoutObject()->StyleRef().HasTransform()) ||
         (apply_motion_transform == kIncludeMotionTransform &&
          HasSVGRareData());
}

static inline bool TransformUsesBoxSize(
    const ComputedStyle& style,
    ComputedStyle::ApplyTransformOrigin apply_transform_origin) {
  if (apply_transform_origin == ComputedStyle::kIncludeTransformOrigin &&
      (style.TransformOriginX().GetType() == kPercent ||
       style.TransformOriginY().GetType() == kPercent) &&
      style.RequireTransformOrigin(ComputedStyle::kIncludeTransformOrigin,
                                   ComputedStyle::kExcludeMotionPath))
    return true;
  if (style.Transform().DependsOnBoxSize())
    return true;
  if (style.Translate() && style.Translate()->DependsOnBoxSize())
    return true;
  if (style.HasOffset())
    return true;
  return false;
}

static FloatRect ComputeTransformReferenceBox(const SVGElement& element) {
  const LayoutObject& layout_object = *element.GetLayoutObject();
  const ComputedStyle& style = layout_object.StyleRef();
  if (!RuntimeEnabledFeatures::CSSTransformBoxEnabled()) {
    FloatRect reference_box = layout_object.ObjectBoundingBox();
    // Set the reference origin to zero when transform-origin (x/y) has a
    // non-percentage unit.
    const TransformOrigin& transform_origin = style.GetTransformOrigin();
    if (transform_origin.X().GetType() != kPercent)
      reference_box.SetX(0);
    if (transform_origin.Y().GetType() != kPercent)
      reference_box.SetY(0);
    return reference_box;
  }
  if (style.TransformBox() == ETransformBox::kFillBox)
    return layout_object.ObjectBoundingBox();
  DCHECK(style.TransformBox() == ETransformBox::kBorderBox ||
         style.TransformBox() == ETransformBox::kViewBox);
  SVGLengthContext length_context(&element);
  FloatSize viewport_size;
  length_context.DetermineViewport(viewport_size);
  return FloatRect(FloatPoint(), viewport_size);
}

AffineTransform SVGElement::CalculateTransform(
    ApplyMotionTransform apply_motion_transform) const {
  const ComputedStyle* style =
      GetLayoutObject() ? GetLayoutObject()->Style() : nullptr;

  // If CSS property was set, use that, otherwise fallback to attribute (if
  // set).
  AffineTransform matrix;
  if (style && style->HasTransform()) {
    FloatRect reference_box = ComputeTransformReferenceBox(*this);
    ComputedStyle::ApplyTransformOrigin apply_transform_origin =
        ComputedStyle::kIncludeTransformOrigin;
    // SVGTextElements need special handling for the text positioning code.
    if (isSVGTextElement(this)) {
      // Do not take into account transform-origin, or percentage values.
      reference_box = FloatRect();
      apply_transform_origin = ComputedStyle::kExcludeTransformOrigin;
    }

    if (TransformUsesBoxSize(*style, apply_transform_origin))
      UseCounter::Count(GetDocument(), WebFeature::kTransformUsesBoxSizeOnSVG);

    // CSS transforms operate with pre-scaled lengths. To make this work with
    // SVG (which applies the zoom factor globally, at the root level) we
    //
    //   * pre-scale the reference box (to bring it into the same space as the
    //     other CSS values)
    //   * invert the zoom factor (to effectively compute the CSS transform
    //     under a 1.0 zoom)
    //
    // Note: objectBoundingBox is an emptyRect for elements like pattern or
    // clipPath. See the "Object bounding box units" section of
    // http://dev.w3.org/csswg/css3-transforms/
    float zoom = style->EffectiveZoom();
    TransformationMatrix transform;
    if (zoom != 1) {
      reference_box.Scale(zoom);
      transform.Scale(1 / zoom);
      style->ApplyTransform(
          transform, reference_box, apply_transform_origin,
          ComputedStyle::kIncludeMotionPath,
          ComputedStyle::kIncludeIndependentTransformProperties);
      transform.Scale(zoom);
    } else {
      style->ApplyTransform(
          transform, reference_box, apply_transform_origin,
          ComputedStyle::kIncludeMotionPath,
          ComputedStyle::kIncludeIndependentTransformProperties);
    }
    // Flatten any 3D transform.
    matrix = transform.ToAffineTransform();
  }

  // Apply any "motion transform" contribution if requested (and existing.)
  if (apply_motion_transform == kIncludeMotionTransform && HasSVGRareData())
    matrix.PreMultiply(*SvgRareData()->AnimateMotionTransform());

  return matrix;
}

Node::InsertionNotificationRequest SVGElement::InsertedInto(
    ContainerNode* root_parent) {
  Element::InsertedInto(root_parent);
  UpdateRelativeLengthsInformation();
  BuildPendingResourcesIfNeeded();

  if (hasAttribute(nonceAttr) && getAttribute(nonceAttr) != g_empty_atom) {
    setNonce(getAttribute(nonceAttr));
    if (RuntimeEnabledFeatures::HideNonceContentAttributeEnabled() &&
        InActiveDocument() &&
        GetDocument().GetContentSecurityPolicy()->HasHeaderDeliveredPolicy()) {
      setAttribute(nonceAttr, g_empty_atom);
    }
  }

  return kInsertionDone;
}

void SVGElement::RemovedFrom(ContainerNode* root_parent) {
  bool was_in_document = root_parent->isConnected();

  if (was_in_document && HasRelativeLengths()) {
    // The root of the subtree being removed should take itself out from its
    // parent's relative length set. For the other nodes in the subtree we don't
    // need to do anything: they will get their own removedFrom() notification
    // and just clear their sets.
    if (root_parent->IsSVGElement() && !parentNode()) {
      DCHECK(ToSVGElement(root_parent)
                 ->elements_with_relative_lengths_.Contains(this));
      ToSVGElement(root_parent)->UpdateRelativeLengthsInformation(false, this);
    }

    elements_with_relative_lengths_.clear();
  }

  SECURITY_DCHECK(!root_parent->IsSVGElement() ||
                  !ToSVGElement(root_parent)
                       ->elements_with_relative_lengths_.Contains(this));

  Element::RemovedFrom(root_parent);

  if (was_in_document) {
    RebuildAllIncomingReferences();
    RemoveAllIncomingReferences();
  }

  InvalidateInstances();
}

void SVGElement::ChildrenChanged(const ChildrenChange& change) {
  Element::ChildrenChanged(change);

  // Invalidate all instances associated with us.
  if (!change.by_parser)
    InvalidateInstances();
}

CSSPropertyID SVGElement::CssPropertyIdForSVGAttributeName(
    const QualifiedName& attr_name) {
  if (!attr_name.NamespaceURI().IsNull())
    return CSSPropertyInvalid;

  static HashMap<StringImpl*, CSSPropertyID>* property_name_to_id_map = 0;
  if (!property_name_to_id_map) {
    property_name_to_id_map = new HashMap<StringImpl*, CSSPropertyID>;
    // This is a list of all base CSS and SVG CSS properties which are exposed
    // as SVG XML attributes
    const QualifiedName* const attr_names[] = {
        &alignment_baselineAttr,
        &baseline_shiftAttr,
        &buffered_renderingAttr,
        &clipAttr,
        &clip_pathAttr,
        &clip_ruleAttr,
        &SVGNames::colorAttr,
        &color_interpolationAttr,
        &color_interpolation_filtersAttr,
        &color_renderingAttr,
        &cursorAttr,
        &SVGNames::directionAttr,
        &displayAttr,
        &dominant_baselineAttr,
        &fillAttr,
        &fill_opacityAttr,
        &fill_ruleAttr,
        &filterAttr,
        &flood_colorAttr,
        &flood_opacityAttr,
        &font_familyAttr,
        &font_sizeAttr,
        &font_stretchAttr,
        &font_styleAttr,
        &font_variantAttr,
        &font_weightAttr,
        &image_renderingAttr,
        &letter_spacingAttr,
        &lighting_colorAttr,
        &marker_endAttr,
        &marker_midAttr,
        &marker_startAttr,
        &maskAttr,
        &mask_typeAttr,
        &opacityAttr,
        &overflowAttr,
        &paint_orderAttr,
        &pointer_eventsAttr,
        &shape_renderingAttr,
        &stop_colorAttr,
        &stop_opacityAttr,
        &strokeAttr,
        &stroke_dasharrayAttr,
        &stroke_dashoffsetAttr,
        &stroke_linecapAttr,
        &stroke_linejoinAttr,
        &stroke_miterlimitAttr,
        &stroke_opacityAttr,
        &stroke_widthAttr,
        &text_anchorAttr,
        &text_decorationAttr,
        &text_renderingAttr,
        &transform_originAttr,
        &unicode_bidiAttr,
        &vector_effectAttr,
        &visibilityAttr,
        &word_spacingAttr,
        &writing_modeAttr,
    };
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(attr_names); i++) {
      CSSPropertyID property_id = cssPropertyID(attr_names[i]->LocalName());
      DCHECK_GT(property_id, 0);
      property_name_to_id_map->Set(attr_names[i]->LocalName().Impl(),
                                   property_id);
    }
  }

  return property_name_to_id_map->at(attr_name.LocalName().Impl());
}

void SVGElement::UpdateRelativeLengthsInformation(
    bool client_has_relative_lengths,
    SVGElement* client_element) {
  DCHECK(client_element);

  // Through an unfortunate chain of events, we can end up calling this while a
  // subtree is being removed, and before the subtree has been properly
  // "disconnected". Hence check the entire ancestor chain to avoid propagating
  // relative length clients up into ancestors that have already been
  // disconnected.
  // If we're not yet in a document, this function will be called again from
  // insertedInto(). Do nothing now.
  for (Node& current_node : NodeTraversal::InclusiveAncestorsOf(*this)) {
    if (!current_node.isConnected())
      return;
  }

  // An element wants to notify us that its own relative lengths state changed.
  // Register it in the relative length map, and register us in the parent
  // relative length map.  Register the parent in the grandparents map, etc.
  // Repeat procedure until the root of the SVG tree.
  for (Node& current_node : NodeTraversal::InclusiveAncestorsOf(*this)) {
    if (!current_node.IsSVGElement())
      break;
    SVGElement& current_element = ToSVGElement(current_node);
#if DCHECK_IS_ON()
    DCHECK(!current_element.in_relative_length_clients_invalidation_);
#endif

    bool had_relative_lengths = current_element.HasRelativeLengths();
    if (client_has_relative_lengths)
      current_element.elements_with_relative_lengths_.insert(client_element);
    else
      current_element.elements_with_relative_lengths_.erase(client_element);

    // If the relative length state hasn't changed, we can stop propagating the
    // notification.
    if (had_relative_lengths == current_element.HasRelativeLengths())
      return;

    client_element = &current_element;
    client_has_relative_lengths = client_element->HasRelativeLengths();
  }

  // Register root SVG elements for top level viewport change notifications.
  if (isSVGSVGElement(*client_element)) {
    SVGDocumentExtensions& svg_extensions = GetDocument().AccessSVGExtensions();
    if (client_element->HasRelativeLengths())
      svg_extensions.AddSVGRootWithRelativeLengthDescendents(
          toSVGSVGElement(client_element));
    else
      svg_extensions.RemoveSVGRootWithRelativeLengthDescendents(
          toSVGSVGElement(client_element));
  }
}

void SVGElement::InvalidateRelativeLengthClients(
    SubtreeLayoutScope* layout_scope) {
  if (!isConnected())
    return;

#if DCHECK_IS_ON()
  DCHECK(!in_relative_length_clients_invalidation_);
  AutoReset<bool> in_relative_length_clients_invalidation_change(
      &in_relative_length_clients_invalidation_, true);
#endif

  if (LayoutObject* layout_object = this->GetLayoutObject()) {
    if (HasRelativeLengths() && layout_object->IsSVGResourceContainer())
      ToLayoutSVGResourceContainer(layout_object)
          ->InvalidateCacheAndMarkForLayout(layout_scope);
    else if (SelfHasRelativeLengths())
      layout_object->SetNeedsLayoutAndFullPaintInvalidation(
          LayoutInvalidationReason::kUnknown, kMarkContainerChain,
          layout_scope);
  }

  for (SVGElement* element : elements_with_relative_lengths_) {
    if (element != this)
      element->InvalidateRelativeLengthClients(layout_scope);
  }
}

SVGSVGElement* SVGElement::ownerSVGElement() const {
  ContainerNode* n = ParentOrShadowHostNode();
  while (n) {
    if (isSVGSVGElement(*n))
      return toSVGSVGElement(n);

    n = n->ParentOrShadowHostNode();
  }

  return nullptr;
}

SVGElement* SVGElement::viewportElement() const {
  // This function needs shadow tree support - as LayoutSVGContainer uses this
  // function to determine the "overflow" property. <use> on <symbol> wouldn't
  // work otherwhise.
  ContainerNode* n = ParentOrShadowHostNode();
  while (n) {
    if (isSVGSVGElement(*n) || isSVGImageElement(*n) || isSVGSymbolElement(*n))
      return ToSVGElement(n);

    n = n->ParentOrShadowHostNode();
  }

  return nullptr;
}

void SVGElement::MapInstanceToElement(SVGElement* instance) {
  DCHECK(instance);
  DCHECK(instance->InUseShadowTree());

  HeapHashSet<WeakMember<SVGElement>>& instances =
      EnsureSVGRareData()->ElementInstances();
  DCHECK(!instances.Contains(instance));

  instances.insert(instance);
}

void SVGElement::RemoveInstanceMapping(SVGElement* instance) {
  DCHECK(instance);
  DCHECK(instance->InUseShadowTree());

  if (!HasSVGRareData())
    return;

  HeapHashSet<WeakMember<SVGElement>>& instances =
      SvgRareData()->ElementInstances();

  instances.erase(instance);
}

static HeapHashSet<WeakMember<SVGElement>>& EmptyInstances() {
  DEFINE_STATIC_LOCAL(HeapHashSet<WeakMember<SVGElement>>, empty_instances,
                      (new HeapHashSet<WeakMember<SVGElement>>));
  return empty_instances;
}

const HeapHashSet<WeakMember<SVGElement>>& SVGElement::InstancesForElement()
    const {
  if (!HasSVGRareData())
    return EmptyInstances();
  return SvgRareData()->ElementInstances();
}

SVGElement* SVGElement::CorrespondingElement() const {
  DCHECK(!HasSVGRareData() || !SvgRareData()->CorrespondingElement() ||
         ContainingShadowRoot());
  return HasSVGRareData() ? SvgRareData()->CorrespondingElement() : 0;
}

SVGUseElement* SVGElement::CorrespondingUseElement() const {
  if (ShadowRoot* root = ContainingShadowRoot()) {
    if (isSVGUseElement(root->host()))
      return &toSVGUseElement(root->host());
  }
  return nullptr;
}

void SVGElement::SetCorrespondingElement(SVGElement* corresponding_element) {
  EnsureSVGRareData()->SetCorrespondingElement(corresponding_element);
}

bool SVGElement::InUseShadowTree() const {
  return CorrespondingUseElement();
}

void SVGElement::ParseAttribute(const AttributeModificationParams& params) {
  if (SVGAnimatedPropertyBase* property = PropertyFromAttribute(params.name)) {
    SVGParsingError parse_error =
        property->SetBaseValueAsString(params.new_value);
    ReportAttributeParsingError(parse_error, params.name, params.new_value);
    return;
  }

  if (params.name == HTMLNames::classAttr) {
    // SVG animation has currently requires special storage of values so we set
    // the className here. svgAttributeChanged actually causes the resulting
    // style updates (instead of Element::parseAttribute). We don't
    // tell Element about the change to avoid parsing the class list twice
    SVGParsingError parse_error =
        class_name_->SetBaseValueAsString(params.new_value);
    ReportAttributeParsingError(parse_error, params.name, params.new_value);
  } else if (params.name == tabindexAttr) {
    Element::ParseAttribute(params);
  } else {
    // standard events
    const AtomicString& event_name =
        HTMLElement::EventNameForAttributeName(params.name);
    if (!event_name.IsNull()) {
      SetAttributeEventListener(
          event_name,
          CreateAttributeEventListener(this, params.name, params.new_value,
                                       EventParameterName()));
    } else {
      Element::ParseAttribute(params);
    }
  }
}

// If the attribute is not present in the map, the map will return the "empty
// value" - which is kAnimatedUnknown.
struct AnimatedPropertyTypeHashTraits : HashTraits<AnimatedPropertyType> {
  static const bool kEmptyValueIsZero = true;
  static AnimatedPropertyType EmptyValue() { return kAnimatedUnknown; }
};

using AttributeToPropertyTypeMap = HashMap<QualifiedName,
                                           AnimatedPropertyType,
                                           DefaultHash<QualifiedName>::Hash,
                                           HashTraits<QualifiedName>,
                                           AnimatedPropertyTypeHashTraits>;
AnimatedPropertyType SVGElement::AnimatedPropertyTypeForCSSAttribute(
    const QualifiedName& attribute_name) {
  DEFINE_STATIC_LOCAL(AttributeToPropertyTypeMap, css_property_map, ());

  if (css_property_map.IsEmpty()) {
    // Fill the map for the first use.
    struct AttrToTypeEntry {
      const QualifiedName& attr;
      const AnimatedPropertyType prop_type;
    };
    const AttrToTypeEntry attr_to_types[] = {
        {alignment_baselineAttr, kAnimatedString},
        {baseline_shiftAttr, kAnimatedString},
        {buffered_renderingAttr, kAnimatedString},
        {clip_pathAttr, kAnimatedString},
        {clip_ruleAttr, kAnimatedString},
        {SVGNames::colorAttr, kAnimatedColor},
        {color_interpolationAttr, kAnimatedString},
        {color_interpolation_filtersAttr, kAnimatedString},
        {color_renderingAttr, kAnimatedString},
        {cursorAttr, kAnimatedString},
        {displayAttr, kAnimatedString},
        {dominant_baselineAttr, kAnimatedString},
        {fillAttr, kAnimatedColor},
        {fill_opacityAttr, kAnimatedNumber},
        {fill_ruleAttr, kAnimatedString},
        {filterAttr, kAnimatedString},
        {flood_colorAttr, kAnimatedColor},
        {flood_opacityAttr, kAnimatedNumber},
        {font_familyAttr, kAnimatedString},
        {font_sizeAttr, kAnimatedLength},
        {font_stretchAttr, kAnimatedString},
        {font_styleAttr, kAnimatedString},
        {font_variantAttr, kAnimatedString},
        {font_weightAttr, kAnimatedString},
        {image_renderingAttr, kAnimatedString},
        {letter_spacingAttr, kAnimatedLength},
        {lighting_colorAttr, kAnimatedColor},
        {marker_endAttr, kAnimatedString},
        {marker_midAttr, kAnimatedString},
        {marker_startAttr, kAnimatedString},
        {maskAttr, kAnimatedString},
        {mask_typeAttr, kAnimatedString},
        {opacityAttr, kAnimatedNumber},
        {overflowAttr, kAnimatedString},
        {paint_orderAttr, kAnimatedString},
        {pointer_eventsAttr, kAnimatedString},
        {shape_renderingAttr, kAnimatedString},
        {stop_colorAttr, kAnimatedColor},
        {stop_opacityAttr, kAnimatedNumber},
        {strokeAttr, kAnimatedColor},
        {stroke_dasharrayAttr, kAnimatedLengthList},
        {stroke_dashoffsetAttr, kAnimatedLength},
        {stroke_linecapAttr, kAnimatedString},
        {stroke_linejoinAttr, kAnimatedString},
        {stroke_miterlimitAttr, kAnimatedNumber},
        {stroke_opacityAttr, kAnimatedNumber},
        {stroke_widthAttr, kAnimatedLength},
        {text_anchorAttr, kAnimatedString},
        {text_decorationAttr, kAnimatedString},
        {text_renderingAttr, kAnimatedString},
        {vector_effectAttr, kAnimatedString},
        {visibilityAttr, kAnimatedString},
        {word_spacingAttr, kAnimatedLength},
    };
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(attr_to_types); i++)
      css_property_map.Set(attr_to_types[i].attr, attr_to_types[i].prop_type);
  }
  return css_property_map.at(attribute_name);
}

void SVGElement::AddToPropertyMap(SVGAnimatedPropertyBase* property) {
  attribute_to_property_map_.Set(property->AttributeName(), property);
}

SVGAnimatedPropertyBase* SVGElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  AttributeToPropertyMap::const_iterator it =
      attribute_to_property_map_.Find<SVGAttributeHashTranslator>(
          attribute_name);
  if (it == attribute_to_property_map_.end())
    return nullptr;

  return it->value.Get();
}

bool SVGElement::IsAnimatableCSSProperty(const QualifiedName& attr_name) {
  return AnimatedPropertyTypeForCSSAttribute(attr_name) != kAnimatedUnknown;
}

bool SVGElement::IsPresentationAttribute(const QualifiedName& name) const {
  if (const SVGAnimatedPropertyBase* property = PropertyFromAttribute(name))
    return property->HasPresentationAttributeMapping();
  return CssPropertyIdForSVGAttributeName(name) > 0;
}

bool SVGElement::IsPresentationAttributeWithSVGDOM(
    const QualifiedName& name) const {
  const SVGAnimatedPropertyBase* property = PropertyFromAttribute(name);
  return property && property->HasPresentationAttributeMapping();
}

void SVGElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableStylePropertySet* style) {
  CSSPropertyID property_id = CssPropertyIdForSVGAttributeName(name);
  if (property_id > 0)
    AddPropertyToPresentationAttributeStyle(style, property_id, value);
}

bool SVGElement::HaveLoadedRequiredResources() {
  for (SVGElement* child = Traversal<SVGElement>::FirstChild(*this); child;
       child = Traversal<SVGElement>::NextSibling(*child)) {
    if (!child->HaveLoadedRequiredResources())
      return false;
  }
  return true;
}

static inline void CollectInstancesForSVGElement(
    SVGElement* element,
    HeapHashSet<WeakMember<SVGElement>>& instances) {
  DCHECK(element);
  if (element->ContainingShadowRoot())
    return;

  DCHECK(!element->InstanceUpdatesBlocked());

  instances = element->InstancesForElement();
}

void SVGElement::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  // Add event listener to regular DOM element
  Node::AddedEventListener(event_type, registered_listener);

  // Add event listener to all shadow tree DOM element instances
  HeapHashSet<WeakMember<SVGElement>> instances;
  CollectInstancesForSVGElement(this, instances);
  AddEventListenerOptionsResolved options = registered_listener.Options();
  EventListener* listener = registered_listener.Listener();
  for (SVGElement* element : instances) {
    bool result =
        element->Node::AddEventListenerInternal(event_type, listener, options);
    DCHECK(result);
  }
}

void SVGElement::RemovedEventListener(
    const AtomicString& event_type,
    const RegisteredEventListener& registered_listener) {
  Node::RemovedEventListener(event_type, registered_listener);

  // Remove event listener from all shadow tree DOM element instances
  HeapHashSet<WeakMember<SVGElement>> instances;
  CollectInstancesForSVGElement(this, instances);
  EventListenerOptions options = registered_listener.Options();
  const EventListener* listener = registered_listener.Listener();
  for (SVGElement* shadow_tree_element : instances) {
    DCHECK(shadow_tree_element);

    shadow_tree_element->Node::RemoveEventListenerInternal(event_type, listener,
                                                           options);
  }
}

static bool HasLoadListener(Element* element) {
  if (element->HasEventListeners(EventTypeNames::load))
    return true;

  for (element = element->ParentOrShadowHostElement(); element;
       element = element->ParentOrShadowHostElement()) {
    EventListenerVector* entry =
        element->GetEventListeners(EventTypeNames::load);
    if (!entry)
      continue;
    for (size_t i = 0; i < entry->size(); ++i) {
      if (entry->at(i).Capture())
        return true;
    }
  }

  return false;
}

bool SVGElement::SendSVGLoadEventIfPossible() {
  if (!HaveLoadedRequiredResources())
    return false;
  if ((IsStructurallyExternal() || isSVGSVGElement(*this)) &&
      HasLoadListener(this))
    DispatchEvent(Event::Create(EventTypeNames::load));
  return true;
}

void SVGElement::SendSVGLoadEventToSelfAndAncestorChainIfPossible() {
  // Let Document::implicitClose() dispatch the 'load' to the outermost SVG
  // root.
  if (IsOutermostSVGSVGElement())
    return;

  // Save the next parent to dispatch to in case dispatching the event mutates
  // the tree.
  Element* parent = ParentOrShadowHostElement();
  if (!SendSVGLoadEventIfPossible())
    return;

  // If document/window 'load' has been sent already, then only deliver to
  // the element in question.
  if (GetDocument().LoadEventFinished())
    return;

  if (!parent || !parent->IsSVGElement())
    return;

  ToSVGElement(parent)->SendSVGLoadEventToSelfAndAncestorChainIfPossible();
}

void SVGElement::AttributeChanged(const AttributeModificationParams& params) {
  Element::AttributeChanged(params);

  if (params.name == HTMLNames::idAttr) {
    RebuildAllIncomingReferences();

    LayoutObject* object = GetLayoutObject();
    // Notify resources about id changes, this is important as we cache
    // resources by id in SVGDocumentExtensions
    if (object && object->IsSVGResourceContainer()) {
      ToLayoutSVGResourceContainer(object)->IdChanged(params.old_value,
                                                      params.new_value);
    }
    if (isConnected())
      BuildPendingResourcesIfNeeded();
    InvalidateInstances();
    return;
  }

  // Changes to the style attribute are processed lazily (see
  // Element::getAttribute() and related methods), so we don't want changes to
  // the style attribute to result in extra work here.
  if (params.name == HTMLNames::styleAttr)
    return;

  SvgAttributeBaseValChanged(params.name);
}

void SVGElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  CSSPropertyID prop_id =
      SVGElement::CssPropertyIdForSVGAttributeName(attr_name);
  if (prop_id > 0) {
    InvalidateInstances();
    return;
  }

  if (attr_name == HTMLNames::classAttr) {
    ClassAttributeChanged(AtomicString(class_name_->CurrentValue()->Value()));
    InvalidateInstances();
    return;
  }
}

void SVGElement::SvgAttributeBaseValChanged(const QualifiedName& attribute) {
  SvgAttributeChanged(attribute);

  if (!HasSVGRareData() || SvgRareData()->WebAnimatedAttributes().IsEmpty())
    return;

  // TODO(alancutter): Only mark attributes as dirty if their animation depends
  // on the underlying value.
  SvgRareData()->SetWebAnimatedAttributesDirty(true);
  GetElementData()->animated_svg_attributes_are_dirty_ = true;
}

void SVGElement::EnsureAttributeAnimValUpdated() {
  if (!RuntimeEnabledFeatures::WebAnimationsSVGEnabled())
    return;

  if ((HasSVGRareData() && SvgRareData()->WebAnimatedAttributesDirty()) ||
      (GetElementAnimations() &&
       DocumentAnimations::NeedsAnimationTimingUpdate(GetDocument()))) {
    DocumentAnimations::UpdateAnimationTimingIfNeeded(GetDocument());
    ApplyActiveWebAnimations();
  }
}

void SVGElement::SynchronizeAnimatedSVGAttribute(
    const QualifiedName& name) const {
  if (!GetElementData() ||
      !GetElementData()->animated_svg_attributes_are_dirty_)
    return;

  // We const_cast here because we have deferred baseVal mutation animation
  // updates to this point in time.
  const_cast<SVGElement*>(this)->EnsureAttributeAnimValUpdated();

  if (name == AnyQName()) {
    AttributeToPropertyMap::const_iterator::ValuesIterator it =
        attribute_to_property_map_.Values().begin();
    AttributeToPropertyMap::const_iterator::ValuesIterator end =
        attribute_to_property_map_.Values().end();
    for (; it != end; ++it) {
      if ((*it)->NeedsSynchronizeAttribute())
        (*it)->SynchronizeAttribute();
    }

    GetElementData()->animated_svg_attributes_are_dirty_ = false;
  } else {
    SVGAnimatedPropertyBase* property = attribute_to_property_map_.at(name);
    if (property && property->NeedsSynchronizeAttribute())
      property->SynchronizeAttribute();
  }
}

RefPtr<ComputedStyle> SVGElement::CustomStyleForLayoutObject() {
  if (!CorrespondingElement())
    return GetDocument().EnsureStyleResolver().StyleForElement(this);

  const ComputedStyle* style = nullptr;
  if (Element* parent = ParentOrShadowHostElement()) {
    if (LayoutObject* layout_object = parent->GetLayoutObject())
      style = layout_object->Style();
  }

  return GetDocument().EnsureStyleResolver().StyleForElement(
      CorrespondingElement(), style, style);
}

bool SVGElement::LayoutObjectIsNeeded(const ComputedStyle& style) {
  return IsValid() && HasSVGParent() && Element::LayoutObjectIsNeeded(style);
}

bool SVGElement::HasSVGParent() const {
  // Should we use the flat tree parent instead? If so, we should probably fix a
  // few other checks.
  return ParentOrShadowHostElement() &&
         ParentOrShadowHostElement()->IsSVGElement();
}

MutableStylePropertySet* SVGElement::AnimatedSMILStyleProperties() const {
  if (HasSVGRareData())
    return SvgRareData()->AnimatedSMILStyleProperties();
  return nullptr;
}

MutableStylePropertySet* SVGElement::EnsureAnimatedSMILStyleProperties() {
  return EnsureSVGRareData()->EnsureAnimatedSMILStyleProperties();
}

void SVGElement::SetUseOverrideComputedStyle(bool value) {
  if (HasSVGRareData())
    SvgRareData()->SetUseOverrideComputedStyle(value);
}

const ComputedStyle* SVGElement::EnsureComputedStyle(
    PseudoId pseudo_element_specifier) {
  if (!HasSVGRareData() || !SvgRareData()->UseOverrideComputedStyle())
    return Element::EnsureComputedStyle(pseudo_element_specifier);

  const ComputedStyle* parent_style = nullptr;
  if (Element* parent = ParentOrShadowHostElement()) {
    if (LayoutObject* layout_object = parent->GetLayoutObject())
      parent_style = layout_object->Style();
  }

  return SvgRareData()->OverrideComputedStyle(this, parent_style);
}

bool SVGElement::HasFocusEventListeners() const {
  return HasEventListeners(EventTypeNames::focusin) ||
         HasEventListeners(EventTypeNames::focusout) ||
         HasEventListeners(EventTypeNames::focus) ||
         HasEventListeners(EventTypeNames::blur);
}

void SVGElement::MarkForLayoutAndParentResourceInvalidation(
    LayoutObject* layout_object) {
  DCHECK(layout_object);
  LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(
      layout_object, true);
}

void SVGElement::InvalidateInstances() {
  if (InstanceUpdatesBlocked())
    return;

  const HeapHashSet<WeakMember<SVGElement>>& set = InstancesForElement();
  if (set.IsEmpty())
    return;

  // Mark all use elements referencing 'element' for rebuilding
  for (SVGElement* instance : set) {
    instance->SetCorrespondingElement(0);

    if (SVGUseElement* element = instance->CorrespondingUseElement()) {
      if (element->isConnected())
        element->InvalidateShadowTree();
    }
  }

  SvgRareData()->ElementInstances().clear();
}

void SVGElement::SetNeedsStyleRecalcForInstances(
    StyleChangeType change_type,
    const StyleChangeReasonForTracing& reason) {
  const HeapHashSet<WeakMember<SVGElement>>& set = InstancesForElement();
  if (set.IsEmpty())
    return;

  for (SVGElement* instance : set)
    instance->SetNeedsStyleRecalc(change_type, reason);
}

SVGElement::InstanceUpdateBlocker::InstanceUpdateBlocker(
    SVGElement* target_element)
    : target_element_(target_element) {
  if (target_element_)
    target_element_->SetInstanceUpdatesBlocked(true);
}

SVGElement::InstanceUpdateBlocker::~InstanceUpdateBlocker() {
  if (target_element_)
    target_element_->SetInstanceUpdatesBlocked(false);
}

#if DCHECK_IS_ON()
bool SVGElement::IsAnimatableAttribute(const QualifiedName& name) const {
  // This static is atomically initialized to dodge a warning about
  // a race when dumping debug data for a layer.
  DEFINE_THREAD_SAFE_STATIC_LOCAL(HashSet<QualifiedName>, animatable_attributes,
                                  ({
                                      SVGNames::amplitudeAttr,
                                      SVGNames::azimuthAttr,
                                      SVGNames::baseFrequencyAttr,
                                      SVGNames::biasAttr,
                                      SVGNames::clipPathUnitsAttr,
                                      SVGNames::cxAttr,
                                      SVGNames::cyAttr,
                                      SVGNames::diffuseConstantAttr,
                                      SVGNames::divisorAttr,
                                      SVGNames::dxAttr,
                                      SVGNames::dyAttr,
                                      SVGNames::edgeModeAttr,
                                      SVGNames::elevationAttr,
                                      SVGNames::exponentAttr,
                                      SVGNames::filterUnitsAttr,
                                      SVGNames::fxAttr,
                                      SVGNames::fyAttr,
                                      SVGNames::gradientTransformAttr,
                                      SVGNames::gradientUnitsAttr,
                                      SVGNames::heightAttr,
                                      SVGNames::hrefAttr,
                                      SVGNames::in2Attr,
                                      SVGNames::inAttr,
                                      SVGNames::interceptAttr,
                                      SVGNames::k1Attr,
                                      SVGNames::k2Attr,
                                      SVGNames::k3Attr,
                                      SVGNames::k4Attr,
                                      SVGNames::kernelMatrixAttr,
                                      SVGNames::kernelUnitLengthAttr,
                                      SVGNames::lengthAdjustAttr,
                                      SVGNames::limitingConeAngleAttr,
                                      SVGNames::markerHeightAttr,
                                      SVGNames::markerUnitsAttr,
                                      SVGNames::markerWidthAttr,
                                      SVGNames::maskContentUnitsAttr,
                                      SVGNames::maskUnitsAttr,
                                      SVGNames::methodAttr,
                                      SVGNames::modeAttr,
                                      SVGNames::numOctavesAttr,
                                      SVGNames::offsetAttr,
                                      SVGNames::operatorAttr,
                                      SVGNames::orderAttr,
                                      SVGNames::orientAttr,
                                      SVGNames::pathLengthAttr,
                                      SVGNames::patternContentUnitsAttr,
                                      SVGNames::patternTransformAttr,
                                      SVGNames::patternUnitsAttr,
                                      SVGNames::pointsAtXAttr,
                                      SVGNames::pointsAtYAttr,
                                      SVGNames::pointsAtZAttr,
                                      SVGNames::preserveAlphaAttr,
                                      SVGNames::preserveAspectRatioAttr,
                                      SVGNames::primitiveUnitsAttr,
                                      SVGNames::radiusAttr,
                                      SVGNames::rAttr,
                                      SVGNames::refXAttr,
                                      SVGNames::refYAttr,
                                      SVGNames::resultAttr,
                                      SVGNames::rotateAttr,
                                      SVGNames::rxAttr,
                                      SVGNames::ryAttr,
                                      SVGNames::scaleAttr,
                                      SVGNames::seedAttr,
                                      SVGNames::slopeAttr,
                                      SVGNames::spacingAttr,
                                      SVGNames::specularConstantAttr,
                                      SVGNames::specularExponentAttr,
                                      SVGNames::spreadMethodAttr,
                                      SVGNames::startOffsetAttr,
                                      SVGNames::stdDeviationAttr,
                                      SVGNames::stitchTilesAttr,
                                      SVGNames::surfaceScaleAttr,
                                      SVGNames::tableValuesAttr,
                                      SVGNames::targetAttr,
                                      SVGNames::targetXAttr,
                                      SVGNames::targetYAttr,
                                      SVGNames::transformAttr,
                                      SVGNames::typeAttr,
                                      SVGNames::valuesAttr,
                                      SVGNames::viewBoxAttr,
                                      SVGNames::widthAttr,
                                      SVGNames::x1Attr,
                                      SVGNames::x2Attr,
                                      SVGNames::xAttr,
                                      SVGNames::xChannelSelectorAttr,
                                      SVGNames::y1Attr,
                                      SVGNames::y2Attr,
                                      SVGNames::yAttr,
                                      SVGNames::yChannelSelectorAttr,
                                      SVGNames::zAttr,
                                  }));

  if (name == classAttr)
    return true;

  return animatable_attributes.Contains(name);
}
#endif  // DCHECK_IS_ON()

SVGElementProxySet* SVGElement::ElementProxySet() {
  // Limit to specific element types.
  if (!isSVGFilterElement(*this) && !isSVGClipPathElement(*this))
    return nullptr;
  return &EnsureSVGRareData()->EnsureElementProxySet();
}

SVGElementSet* SVGElement::SetOfIncomingReferences() const {
  if (!HasSVGRareData())
    return nullptr;
  return &SvgRareData()->IncomingReferences();
}

void SVGElement::AddReferenceTo(SVGElement* target_element) {
  DCHECK(target_element);

  EnsureSVGRareData()->OutgoingReferences().insert(target_element);
  target_element->EnsureSVGRareData()->IncomingReferences().insert(this);
}

void SVGElement::RebuildAllIncomingReferences() {
  if (!HasSVGRareData())
    return;

  const SVGElementSet& incoming_references =
      SvgRareData()->IncomingReferences();

  // Iterate on a snapshot as |incomingReferences| may be altered inside loop.
  HeapVector<Member<SVGElement>> incoming_references_snapshot;
  CopyToVector(incoming_references, incoming_references_snapshot);

  // Force rebuilding the |sourceElement| so it knows about this change.
  for (SVGElement* source_element : incoming_references_snapshot) {
    // Before rebuilding |sourceElement| ensure it was not removed from under
    // us.
    if (incoming_references.Contains(source_element))
      source_element->SvgAttributeChanged(SVGNames::hrefAttr);
  }
}

void SVGElement::RemoveAllIncomingReferences() {
  if (!HasSVGRareData())
    return;

  SVGElementSet& incoming_references = SvgRareData()->IncomingReferences();
  for (SVGElement* source_element : incoming_references) {
    DCHECK(source_element->HasSVGRareData());
    source_element->EnsureSVGRareData()->OutgoingReferences().erase(this);
  }
  incoming_references.clear();
}

void SVGElement::RemoveAllOutgoingReferences() {
  if (!HasSVGRareData())
    return;

  SVGElementSet& outgoing_references = SvgRareData()->OutgoingReferences();
  for (SVGElement* target_element : outgoing_references) {
    DCHECK(target_element->HasSVGRareData());
    target_element->EnsureSVGRareData()->IncomingReferences().erase(this);
  }
  outgoing_references.clear();
}

DEFINE_TRACE(SVGElement) {
  visitor->Trace(elements_with_relative_lengths_);
  visitor->Trace(attribute_to_property_map_);
  visitor->Trace(svg_rare_data_);
  visitor->Trace(class_name_);
  Element::Trace(visitor);
}

const AtomicString& SVGElement::EventParameterName() {
  DEFINE_STATIC_LOCAL(const AtomicString, evt_string, ("evt"));
  return evt_string;
}

}  // namespace blink
