/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann
 * <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2011 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 * Copyright (C) 2012 University of Szeged
 * Copyright (C) 2012 Renata Hodovan <reni@webkit.org>
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

#include "core/svg/SVGUseElement.h"

#include "core/css/StyleChangeReason.h"
#include "core/dom/Document.h"
#include "core/dom/ElementShadow.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/IdTargetObserver.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/dom/events/Event.h"
#include "core/layout/svg/LayoutSVGTransformableContainer.h"
#include "core/svg/SVGGElement.h"
#include "core/svg/SVGLengthContext.h"
#include "core/svg/SVGSVGElement.h"
#include "core/svg/SVGSymbolElement.h"
#include "core/svg/SVGTitleElement.h"
#include "core/svg_names.h"
#include "core/xlink_names.h"
#include "core/xml/parser/XMLDocumentParser.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/wtf/Vector.h"

namespace blink {

inline SVGUseElement::SVGUseElement(Document& document)
    : SVGGraphicsElement(SVGNames::useTag, document),
      SVGURIReference(this),
      x_(SVGAnimatedLength::Create(this,
                                   SVGNames::xAttr,
                                   SVGLength::Create(SVGLengthMode::kWidth),
                                   CSSPropertyX)),
      y_(SVGAnimatedLength::Create(this,
                                   SVGNames::yAttr,
                                   SVGLength::Create(SVGLengthMode::kHeight),
                                   CSSPropertyY)),
      width_(
          SVGAnimatedLength::Create(this,
                                    SVGNames::widthAttr,
                                    SVGLength::Create(SVGLengthMode::kWidth))),
      height_(
          SVGAnimatedLength::Create(this,
                                    SVGNames::heightAttr,
                                    SVGLength::Create(SVGLengthMode::kHeight))),
      element_url_is_local_(true),
      have_fired_load_event_(false),
      needs_shadow_tree_recreation_(false) {
  DCHECK(HasCustomStyleCallbacks());

  AddToPropertyMap(x_);
  AddToPropertyMap(y_);
  AddToPropertyMap(width_);
  AddToPropertyMap(height_);
}

SVGUseElement* SVGUseElement::Create(Document& document) {
  // Always build a user agent #shadow-root for SVGUseElement.
  SVGUseElement* use = new SVGUseElement(document);
  use->EnsureShadow().AddShadowRoot(*use, ShadowRootType::kClosed);
  return use;
}

SVGUseElement::~SVGUseElement() {}

void SVGUseElement::Dispose() {
  SetDocumentResource(nullptr);
}

void SVGUseElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(x_);
  visitor->Trace(y_);
  visitor->Trace(width_);
  visitor->Trace(height_);
  visitor->Trace(target_element_instance_);
  visitor->Trace(target_id_observer_);
  visitor->Trace(resource_);
  SVGGraphicsElement::Trace(visitor);
  SVGURIReference::Trace(visitor);
  DocumentResourceClient::Trace(visitor);
}

#if DCHECK_IS_ON()
static inline bool IsWellFormedDocument(Document* document) {
  if (document->IsXMLDocument())
    return static_cast<XMLDocumentParser*>(document->Parser())->WellFormed();
  return true;
}
#endif

Node::InsertionNotificationRequest SVGUseElement::InsertedInto(
    ContainerNode* root_parent) {
  // This functions exists to assure assumptions made in the code regarding
  // SVGElementInstance creation/destruction are satisfied.
  SVGGraphicsElement::InsertedInto(root_parent);
  if (!root_parent->isConnected())
    return kInsertionDone;
#if DCHECK_IS_ON()
  DCHECK(!target_element_instance_ || !IsWellFormedDocument(&GetDocument()));
  DCHECK(!HasPendingResources() || !IsWellFormedDocument(&GetDocument()));
#endif
  InvalidateShadowTree();
  return kInsertionDone;
}

void SVGUseElement::RemovedFrom(ContainerNode* root_parent) {
  SVGGraphicsElement::RemovedFrom(root_parent);
  if (root_parent->isConnected()) {
    ClearResourceReference();
    CancelShadowTreeRecreation();
  }
}

static void TransferUseWidthAndHeightIfNeeded(
    const SVGUseElement& use,
    SVGElement& shadow_element,
    const SVGElement& original_element) {
  DEFINE_STATIC_LOCAL(const AtomicString, hundred_percent_string, ("100%"));
  // Use |originalElement| for checking the element type, because we will
  // have replaced a <symbol> with an <svg> in the instance tree.
  if (IsSVGSymbolElement(original_element)) {
    // Spec (<use> on <symbol>): This generated 'svg' will always have
    // explicit values for attributes width and height.  If attributes
    // width and/or height are provided on the 'use' element, then these
    // attributes will be transferred to the generated 'svg'. If attributes
    // width and/or height are not specified, the generated 'svg' element
    // will use values of 100% for these attributes.
    shadow_element.setAttribute(
        SVGNames::widthAttr,
        use.width()->IsSpecified()
            ? AtomicString(use.width()->CurrentValue()->ValueAsString())
            : hundred_percent_string);
    shadow_element.setAttribute(
        SVGNames::heightAttr,
        use.height()->IsSpecified()
            ? AtomicString(use.height()->CurrentValue()->ValueAsString())
            : hundred_percent_string);
  } else if (IsSVGSVGElement(original_element)) {
    // Spec (<use> on <svg>): If attributes width and/or height are
    // provided on the 'use' element, then these values will override the
    // corresponding attributes on the 'svg' in the generated tree.
    shadow_element.setAttribute(
        SVGNames::widthAttr,
        use.width()->IsSpecified()
            ? AtomicString(use.width()->CurrentValue()->ValueAsString())
            : original_element.getAttribute(SVGNames::widthAttr));
    shadow_element.setAttribute(
        SVGNames::heightAttr,
        use.height()->IsSpecified()
            ? AtomicString(use.height()->CurrentValue()->ValueAsString())
            : original_element.getAttribute(SVGNames::heightAttr));
  }
}

void SVGUseElement::CollectStyleForPresentationAttribute(
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
  } else {
    SVGGraphicsElement::CollectStyleForPresentationAttribute(name, value,
                                                             style);
  }
}

bool SVGUseElement::IsStructurallyExternal() const {
  return !element_url_is_local_ &&
         !EqualIgnoringFragmentIdentifier(element_url_, GetDocument().Url());
}

void SVGUseElement::UpdateTargetReference() {
  const String& url_string = HrefString();
  element_url_ = GetDocument().CompleteURL(url_string);
  element_url_is_local_ = url_string.StartsWith('#');
  if (element_url_is_local_) {
    SetDocumentResource(nullptr);
    return;
  }
  if (!element_url_.HasFragmentIdentifier() ||
      (resource_ &&
       EqualIgnoringFragmentIdentifier(element_url_, resource_->Url())))
    return;

  ResourceLoaderOptions options;
  options.initiator_info.name = localName();
  FetchParameters params(ResourceRequest(element_url_), options);
  SetDocumentResource(
      DocumentResource::FetchSVGDocument(params, GetDocument().Fetcher()));
}

void SVGUseElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  if (attr_name == SVGNames::xAttr || attr_name == SVGNames::yAttr ||
      attr_name == SVGNames::widthAttr || attr_name == SVGNames::heightAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);

    if (attr_name == SVGNames::xAttr || attr_name == SVGNames::yAttr) {
      InvalidateSVGPresentationAttributeStyle();
      SetNeedsStyleRecalc(
          kLocalStyleChange,
          StyleChangeReasonForTracing::FromAttribute(attr_name));
    }

    UpdateRelativeLengthsInformation();
    if (target_element_instance_) {
      DCHECK(target_element_instance_->CorrespondingElement());
      TransferUseWidthAndHeightIfNeeded(
          *this, *target_element_instance_,
          *target_element_instance_->CorrespondingElement());
    }

    LayoutObject* object = this->GetLayoutObject();
    if (object)
      MarkForLayoutAndParentResourceInvalidation(object);
    return;
  }

  if (SVGURIReference::IsKnownAttribute(attr_name)) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    UpdateTargetReference();
    InvalidateShadowTree();
    return;
  }

  SVGGraphicsElement::SvgAttributeChanged(attr_name);
}

static bool IsDisallowedElement(const Element& element) {
  // Spec: "Any 'svg', 'symbol', 'g', graphics element or other 'use' is
  // potentially a template object that can be re-used (i.e., "instanced") in
  // the SVG document via a 'use' element." "Graphics Element" is defined as
  // 'circle', 'ellipse', 'image', 'line', 'path', 'polygon', 'polyline',
  // 'rect', 'text' Excluded are anything that is used by reference or that only
  // make sense to appear once in a document.
  if (!element.IsSVGElement())
    return true;

  DEFINE_STATIC_LOCAL(
      HashSet<QualifiedName>, allowed_element_tags,
      ({
          SVGNames::aTag,       SVGNames::circleTag,   SVGNames::descTag,
          SVGNames::ellipseTag, SVGNames::gTag,        SVGNames::imageTag,
          SVGNames::lineTag,    SVGNames::metadataTag, SVGNames::pathTag,
          SVGNames::polygonTag, SVGNames::polylineTag, SVGNames::rectTag,
          SVGNames::svgTag,     SVGNames::switchTag,   SVGNames::symbolTag,
          SVGNames::textTag,    SVGNames::textPathTag, SVGNames::titleTag,
          SVGNames::tspanTag,   SVGNames::useTag,
      }));
  return !allowed_element_tags.Contains<SVGAttributeHashTranslator>(
      element.TagQName());
}

void SVGUseElement::ScheduleShadowTreeRecreation() {
  if (InUseShadowTree())
    return;
  needs_shadow_tree_recreation_ = true;
  GetDocument().ScheduleUseShadowTreeUpdate(*this);
}

void SVGUseElement::CancelShadowTreeRecreation() {
  needs_shadow_tree_recreation_ = false;
  GetDocument().UnscheduleUseShadowTreeUpdate(*this);
}

void SVGUseElement::ClearInstanceRoot() {
  target_element_instance_ = nullptr;
}

void SVGUseElement::ClearResourceReference() {
  UnobserveTarget(target_id_observer_);
  ClearInstanceRoot();
  RemoveAllOutgoingReferences();
}

Element* SVGUseElement::ResolveTargetElement(ObserveBehavior observe_behavior) {
  if (!element_url_.HasFragmentIdentifier())
    return nullptr;
  AtomicString element_identifier(element_url_.FragmentIdentifier());
  if (!IsStructurallyExternal()) {
    if (observe_behavior == kDontAddObserver)
      return GetTreeScope().getElementById(element_identifier);
    return ObserveTarget(target_id_observer_, GetTreeScope(),
                         element_identifier,
                         WTF::Bind(&SVGUseElement::InvalidateShadowTree,
                                   WrapWeakPersistent(this)));
  }
  if (!ResourceIsValid())
    return nullptr;
  return resource_->GetDocument()->getElementById(element_identifier);
}

void SVGUseElement::BuildPendingResource() {
  if (InUseShadowTree())
    return;
  // FIXME: We should try to optimize this, to at least allow partial reclones.
  UseShadowRoot().RemoveChildren(kOmitSubtreeModifiedEvent);
  ClearResourceReference();
  CancelShadowTreeRecreation();
  if (!isConnected())
    return;
  Element* target = ResolveTargetElement(kAddObserver);
  // TODO(fs): Why would the Element not be "connected" at this point?
  if (target && target->isConnected() && target->IsSVGElement()) {
    BuildShadowAndInstanceTree(ToSVGElement(*target));
    InvalidateDependentShadowTrees();
  }

  DCHECK(!needs_shadow_tree_recreation_);
}

String SVGUseElement::title() const {
  // Find the first <title> child in <use> which doesn't cover shadow tree.
  if (Element* title_element = Traversal<SVGTitleElement>::FirstChild(*this))
    return title_element->innerText();

  // If there is no <title> child in <use>, we lookup first <title> child in
  // shadow tree.
  if (target_element_instance_) {
    if (Element* title_element =
            Traversal<SVGTitleElement>::FirstChild(*target_element_instance_))
      return title_element->innerText();
  }
  // Otherwise return a null string.
  return String();
}

static void AssociateCorrespondingElements(SVGElement& target_root,
                                           SVGElement& instance_root) {
  auto target_range =
      Traversal<SVGElement>::InclusiveDescendantsOf(target_root);
  auto target_iterator = target_range.begin();
  for (SVGElement& instance :
       Traversal<SVGElement>::InclusiveDescendantsOf(instance_root)) {
    DCHECK(!instance.CorrespondingElement());
    instance.SetCorrespondingElement(&*target_iterator);
    ++target_iterator;
  }
  DCHECK(!(target_iterator != target_range.end()));
}

// We don't walk the target tree element-by-element, and clone each element,
// but instead use cloneNode(deep=true). This is an optimization for the common
// case where <use> doesn't contain disallowed elements (ie. <foreignObject>).
// Though if there are disallowed elements in the subtree, we have to remove
// them.  For instance: <use> on <g> containing <foreignObject> (indirect
// case).
static inline void RemoveDisallowedElementsFromSubtree(SVGElement& subtree) {
  DCHECK(!subtree.isConnected());
  Element* element = ElementTraversal::FirstWithin(subtree);
  while (element) {
    if (IsDisallowedElement(*element)) {
      Element* next =
          ElementTraversal::NextSkippingChildren(*element, &subtree);
      // The subtree is not in document so this won't generate events that could
      // mutate the tree.
      element->parentNode()->RemoveChild(element);
      element = next;
    } else {
      element = ElementTraversal::Next(*element, &subtree);
    }
  }
}

static void MoveChildrenToReplacementElement(ContainerNode& source_root,
                                             ContainerNode& destination_root) {
  for (Node* child = source_root.firstChild(); child;) {
    Node* next_child = child->nextSibling();
    destination_root.AppendChild(child);
    child = next_child;
  }
}

Element* SVGUseElement::CreateInstanceTree(SVGElement& target_root) const {
  Element* instance_root = target_root.CloneElementWithChildren();
  DCHECK(instance_root->IsSVGElement());
  if (IsSVGSymbolElement(target_root)) {
    // Spec: The referenced 'symbol' and its contents are deep-cloned into
    // the generated tree, with the exception that the 'symbol' is replaced
    // by an 'svg'. This generated 'svg' will always have explicit values
    // for attributes width and height. If attributes width and/or height
    // are provided on the 'use' element, then these attributes will be
    // transferred to the generated 'svg'. If attributes width and/or
    // height are not specified, the generated 'svg' element will use
    // values of 100% for these attributes.
    SVGSVGElement* svg_element =
        SVGSVGElement::Create(target_root.GetDocument());
    // Transfer all data (attributes, etc.) from the <symbol> to the new
    // <svg> element.
    svg_element->CloneDataFromElement(*instance_root);
    // Move already cloned elements to the new <svg> element.
    MoveChildrenToReplacementElement(*instance_root, *svg_element);
    instance_root = svg_element;
  }
  TransferUseWidthAndHeightIfNeeded(*this, ToSVGElement(*instance_root),
                                    target_root);
  AssociateCorrespondingElements(target_root, ToSVGElement(*instance_root));
  RemoveDisallowedElementsFromSubtree(ToSVGElement(*instance_root));
  return instance_root;
}

void SVGUseElement::BuildShadowAndInstanceTree(SVGElement& target) {
  DCHECK(!target_element_instance_);
  DCHECK(!needs_shadow_tree_recreation_);

  // <use> creates a closed shadow root. Do not build the shadow/instance
  // tree for <use> elements living in a closed tree because they
  // will get expanded in a second pass -- see expandUseElementsInShadowTree().
  if (InUseShadowTree())
    return;

  // Do not allow self-referencing.
  if (&target == this || IsDisallowedElement(target))
    return;

  // Set up root SVG element in shadow tree.
  // Clone the target subtree into the shadow tree, not handling <use> and
  // <symbol> yet.
  Element* instance_root = CreateInstanceTree(target);
  target_element_instance_ = ToSVGElement(instance_root);
  ShadowRoot& shadow_root = UseShadowRoot();
  shadow_root.AppendChild(instance_root);

  AddReferencesToFirstDegreeNestedUseElements(target);

  if (InstanceTreeIsLoading()) {
    CloneNonMarkupEventListeners();
    return;
  }

  // Assure shadow tree building was successful.
  DCHECK(target_element_instance_);
  DCHECK_EQ(target_element_instance_->CorrespondingUseElement(), this);
  DCHECK_EQ(target_element_instance_->CorrespondingElement(), &target);

  // Expand all <use> elements in the shadow tree.
  // Expand means: replace the actual <use> element by what it references.
  if (!ExpandUseElementsInShadowTree()) {
    shadow_root.RemoveChildren(kOmitSubtreeModifiedEvent);
    ClearResourceReference();
    return;
  }

  // If the instance root was a <use>, it could have been replaced now, so
  // reset |m_targetElementInstance|.
  target_element_instance_ = ToSVGElementOrDie(shadow_root.firstChild());
  DCHECK_EQ(target_element_instance_->parentNode(), shadow_root);

  CloneNonMarkupEventListeners();

  // Update relative length information.
  UpdateRelativeLengthsInformation();
}

LayoutObject* SVGUseElement::CreateLayoutObject(const ComputedStyle&) {
  return new LayoutSVGTransformableContainer(this);
}

static bool IsDirectReference(const SVGElement& element) {
  return IsSVGPathElement(element) || IsSVGRectElement(element) ||
         IsSVGCircleElement(element) || IsSVGEllipseElement(element) ||
         IsSVGPolygonElement(element) || IsSVGPolylineElement(element) ||
         IsSVGTextElement(element);
}

void SVGUseElement::ToClipPath(Path& path) const {
  DCHECK(path.IsEmpty());

  const SVGGraphicsElement* element = VisibleTargetGraphicsElementForClipping();

  if (!element)
    return;

  if (element->IsSVGGeometryElement()) {
    ToSVGGeometryElement(*element).ToClipPath(path);
    // FIXME: Avoid manual resolution of x/y here. Its potentially harmful.
    SVGLengthContext length_context(this);
    path.Translate(FloatSize(x_->CurrentValue()->Value(length_context),
                             y_->CurrentValue()->Value(length_context)));
    path.Transform(CalculateTransform(SVGElement::kIncludeMotionTransform));
  }
}

SVGGraphicsElement* SVGUseElement::VisibleTargetGraphicsElementForClipping()
    const {
  Node* n = UseShadowRoot().firstChild();
  if (!n || !n->IsSVGElement())
    return nullptr;

  SVGElement& element = ToSVGElement(*n);

  if (!element.IsSVGGraphicsElement())
    return nullptr;

  // Spec: "If a <use> element is a child of a clipPath element, it must
  // directly reference <path>, <text> or basic shapes elements. Indirect
  // references are an error and the clipPath element must be ignored."
  // http://dev.w3.org/fxtf/css-masking-1/#the-clip-path
  if (!IsDirectReference(element)) {
    // Spec: Indirect references are an error (14.3.5)
    return nullptr;
  }

  return &ToSVGGraphicsElement(element);
}

void SVGUseElement::AddReferencesToFirstDegreeNestedUseElements(
    SVGElement& target) {
  // Don't track references to external documents.
  if (IsStructurallyExternal())
    return;
  // We only need to track first degree <use> dependencies. Indirect
  // references are handled as the invalidation bubbles up the dependency
  // chain.
  SVGUseElement* use_element =
      IsSVGUseElement(target) ? ToSVGUseElement(&target)
                              : Traversal<SVGUseElement>::FirstWithin(target);
  for (; use_element;
       use_element = Traversal<SVGUseElement>::NextSkippingChildren(
           *use_element, &target))
    AddReferenceTo(use_element);
}

void SVGUseElement::CloneNonMarkupEventListeners() {
  for (SVGElement& element :
       Traversal<SVGElement>::DescendantsOf(UseShadowRoot())) {
    if (EventTargetData* data =
            element.CorrespondingElement()->GetEventTargetData()) {
      data->event_listener_map.CopyEventListenersNotCreatedFromMarkupToTarget(
          &element);
    }
  }
}

bool SVGUseElement::HasCycleUseReferencing(SVGUseElement& use,
                                           const ContainerNode& target_instance,
                                           SVGElement*& new_target) const {
  Element* target_element = use.ResolveTargetElement(kDontAddObserver);
  new_target = nullptr;
  if (target_element && target_element->IsSVGElement())
    new_target = ToSVGElement(target_element);

  if (!new_target)
    return false;

  // Shortcut for self-references
  if (new_target == this)
    return true;

  AtomicString target_id = new_target->GetIdAttribute();
  ContainerNode* instance = target_instance.parentNode();
  while (instance && instance->IsSVGElement()) {
    SVGElement* element = ToSVGElement(instance);
    if (element->HasID() && element->GetIdAttribute() == target_id &&
        element->GetDocument() == new_target->GetDocument())
      return true;

    instance = instance->parentNode();
  }
  return false;
}

// Spec: In the generated content, the 'use' will be replaced by 'g', where all
// attributes from the 'use' element except for x, y, width, height and
// xlink:href are transferred to the generated 'g' element.
static void RemoveAttributesFromReplacementElement(
    SVGElement& replacement_element) {
  replacement_element.removeAttribute(SVGNames::xAttr);
  replacement_element.removeAttribute(SVGNames::yAttr);
  replacement_element.removeAttribute(SVGNames::widthAttr);
  replacement_element.removeAttribute(SVGNames::heightAttr);
  replacement_element.removeAttribute(SVGNames::hrefAttr);
  replacement_element.removeAttribute(XLinkNames::hrefAttr);
}

bool SVGUseElement::ExpandUseElementsInShadowTree() {
  // Why expand the <use> elements in the shadow tree here, and not just
  // do this directly in buildShadowTree, if we encounter a <use> element?
  //
  // Short answer: Because we may miss to expand some elements. For example, if
  // a <symbol> contains <use> tags, we'd miss them. So once we're done with
  // setting up the actual shadow tree (after the special case modification for
  // svg/symbol) we have to walk it completely and expand all <use> elements.
  ShadowRoot& shadow_root = UseShadowRoot();
  for (SVGUseElement* use = Traversal<SVGUseElement>::FirstWithin(shadow_root);
       use;) {
    DCHECK(!use->ResourceIsStillLoading());

    SVGUseElement& original_use = ToSVGUseElement(*use->CorrespondingElement());
    SVGElement* target = nullptr;
    if (HasCycleUseReferencing(original_use, *use, target))
      return false;

    if (target && IsDisallowedElement(*target))
      return false;
    // Don't DCHECK(target) here, it may be "pending", too.
    // Setup sub-shadow tree root node
    SVGGElement* clone_parent = SVGGElement::Create(original_use.GetDocument());
    // Transfer all data (attributes, etc.) from <use> to the new <g> element.
    clone_parent->CloneDataFromElement(*use);
    clone_parent->SetCorrespondingElement(&original_use);

    RemoveAttributesFromReplacementElement(*clone_parent);

    // Move already cloned elements to the new <g> element.
    MoveChildrenToReplacementElement(*use, *clone_parent);

    if (target)
      clone_parent->AppendChild(use->CreateInstanceTree(*target));

    SVGElement* replacing_element(clone_parent);

    // Replace <use> with referenced content.
    use->parentNode()->ReplaceChild(clone_parent, use);

    use = Traversal<SVGUseElement>::Next(*replacing_element, &shadow_root);
  }
  return true;
}

void SVGUseElement::InvalidateShadowTree() {
  if (!InActiveDocument() || needs_shadow_tree_recreation_)
    return;
  ClearInstanceRoot();
  ScheduleShadowTreeRecreation();
  InvalidateDependentShadowTrees();
}

void SVGUseElement::InvalidateDependentShadowTrees() {
  // Recursively invalidate dependent <use> shadow trees
  const HeapHashSet<WeakMember<SVGElement>>& raw_instances =
      InstancesForElement();
  HeapVector<Member<SVGElement>> instances;
  instances.AppendRange(raw_instances.begin(), raw_instances.end());
  for (auto& instance : instances) {
    if (SVGUseElement* element = instance->CorrespondingUseElement()) {
      DCHECK(element->isConnected());
      element->InvalidateShadowTree();
    }
  }
}

bool SVGUseElement::SelfHasRelativeLengths() const {
  if (x_->CurrentValue()->IsRelative() || y_->CurrentValue()->IsRelative() ||
      width_->CurrentValue()->IsRelative() ||
      height_->CurrentValue()->IsRelative())
    return true;

  if (!target_element_instance_)
    return false;

  return target_element_instance_->HasRelativeLengths();
}

FloatRect SVGUseElement::GetBBox() {
  DCHECK(GetLayoutObject());
  LayoutSVGTransformableContainer& transformable_container =
      ToLayoutSVGTransformableContainer(*GetLayoutObject());
  // Don't apply the additional translation if the oBB is invalid.
  if (!transformable_container.IsObjectBoundingBoxValid())
    return FloatRect();

  // TODO(fs): Preferably this would just use objectBoundingBox() (and hence
  // don't need to override SVGGraphicsElement::getBBox at all) and be
  // correct without additional work. That will not work out ATM without
  // additional quirks. The problem stems from including the additional
  // translation directly on the LayoutObject corresponding to the
  // SVGUseElement.
  FloatRect bbox = transformable_container.ObjectBoundingBox();
  bbox.Move(transformable_container.AdditionalTranslation());
  return bbox;
}

void SVGUseElement::DispatchPendingEvent() {
  DCHECK(IsStructurallyExternal());
  DCHECK(have_fired_load_event_);
  DispatchEvent(Event::Create(EventTypeNames::load));
}

void SVGUseElement::NotifyFinished(Resource* resource) {
  DCHECK_EQ(resource_, resource);
  if (!isConnected())
    return;

  InvalidateShadowTree();
  if (!ResourceIsValid()) {
    DispatchEvent(Event::Create(EventTypeNames::error));
  } else if (!resource->WasCanceled()) {
    if (have_fired_load_event_)
      return;
    if (!IsStructurallyExternal())
      return;
    DCHECK(!have_fired_load_event_);
    have_fired_load_event_ = true;
    TaskRunnerHelper::Get(TaskType::kDOMManipulation, &GetDocument())
        ->PostTask(BLINK_FROM_HERE,
                   WTF::Bind(&SVGUseElement::DispatchPendingEvent,
                             WrapPersistent(this)));
  }
}

bool SVGUseElement::ResourceIsStillLoading() const {
  return resource_ && resource_->IsLoading();
}

bool SVGUseElement::ResourceIsValid() const {
  return resource_ && resource_->IsLoaded() && !resource_->ErrorOccurred() &&
         resource_->GetDocument();
}

bool SVGUseElement::InstanceTreeIsLoading() const {
  for (const SVGUseElement& use_element :
       Traversal<SVGUseElement>::DescendantsOf(UseShadowRoot())) {
    if (use_element.ResourceIsStillLoading())
      return true;
  }
  return false;
}

void SVGUseElement::SetDocumentResource(DocumentResource* resource) {
  if (resource_ == resource)
    return;

  if (resource_)
    resource_->RemoveClient(this);

  resource_ = resource;
  if (resource_)
    resource_->AddClient(this);
}

}  // namespace blink
