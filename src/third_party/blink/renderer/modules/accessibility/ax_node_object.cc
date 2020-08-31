/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/accessibility/ax_node_object.h"

#include <math.h>

#include <algorithm>

#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/aom/accessible_node.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/custom/element_internals.h"
#include "third_party/blink/renderer/core/html/forms/html_field_set_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/core/html/forms/html_legend_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/forms/labels_node_list.h"
#include "third_party/blink/renderer/core/html/forms/radio_input_type.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_dlist_element.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_meter_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/html_table_caption_element.h"
#include "third_party/blink/renderer/core/html/html_table_cell_element.h"
#include "third_party/blink/renderer/core/html/html_table_element.h"
#include "third_party/blink/renderer/core/html/html_table_row_element.h"
#include "third_party/blink/renderer/core/html/html_table_section_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/portal/html_portal_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_image_map_link.h"
#include "third_party/blink/renderer/modules/accessibility/ax_inline_text_box.h"
#include "third_party/blink/renderer/modules/accessibility/ax_layout_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_position.h"
#include "third_party/blink/renderer/modules/accessibility/ax_range.h"
#include "third_party/blink/renderer/modules/accessibility/ax_svg_root.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace {
bool IsNeutralWithinTable(blink::AXObject* obj) {
  if (!obj)
    return false;
  ax::mojom::Role role = obj->RoleValue();
  return role == ax::mojom::Role::kGroup ||
         role == ax::mojom::Role::kGenericContainer ||
         role == ax::mojom::Role::kIgnored ||
         role == ax::mojom::Role::kRowGroup;
}
}  // namespace

namespace blink {

using html_names::kAltAttr;
using html_names::kTitleAttr;
using html_names::kTypeAttr;
using html_names::kValueAttr;

// In ARIA 1.1, default value of aria-level was changed to 2.
const int kDefaultHeadingLevel = 2;

AXNodeObject::AXNodeObject(Node* node, AXObjectCacheImpl& ax_object_cache)
    : AXObject(ax_object_cache),
      children_dirty_(false),
      native_role_(ax::mojom::Role::kUnknown),
      node_(node) {}

AXNodeObject::~AXNodeObject() {
  DCHECK(!node_);
}

void AXNodeObject::AlterSliderOrSpinButtonValue(bool increase) {
  if (!IsSlider() && !IsSpinButton())
    return;

  float value;
  if (!ValueForRange(&value))
    return;

  float step;
  if (!StepValueForRange(&step))
    return;

  value += increase ? step : -step;

  OnNativeSetValueAction(String::Number(value));
  AXObjectCache().PostNotification(GetNode(), ax::mojom::Event::kValueChanged);
}

AXObject* AXNodeObject::ActiveDescendant() {
  Element* element = GetElement();
  if (!element)
    return nullptr;

  Element* descendant =
      GetAOMPropertyOrARIAAttribute(AOMRelationProperty::kActiveDescendant);
  if (!descendant)
    return nullptr;

  AXObject* ax_descendant = AXObjectCache().GetOrCreate(descendant);
  return ax_descendant;
}

AXObjectInclusion AXNodeObject::ShouldIncludeBasedOnSemantics(
    IgnoredReasons* ignored_reasons) const {
  // If this element is within a parent that cannot have children, it should not
  // be exposed.
  if (IsDescendantOfLeafNode()) {
    if (ignored_reasons)
      ignored_reasons->push_back(
          IgnoredReason(kAXAncestorIsLeafNode, LeafNodeAncestor()));
    return kIgnoreObject;
  }

  if (RoleValue() == ax::mojom::Role::kIgnored) {
    if (ignored_reasons)
      ignored_reasons->push_back(IgnoredReason(kAXUninteresting));
    return kIgnoreObject;
  }

  if (HasInheritedPresentationalRole()) {
    if (ignored_reasons) {
      const AXObject* inherits_from = InheritsPresentationalRoleFrom();
      if (inherits_from == this) {
        ignored_reasons->push_back(IgnoredReason(kAXPresentational));
      } else {
        ignored_reasons->push_back(
            IgnoredReason(kAXInheritsPresentation, inherits_from));
      }
    }
    return kIgnoreObject;
  }

  // Objects inside a portal should be ignored. Portals don't directly expose
  // their contents as the contents are not focusable (portals do not currently
  // support input events). Portals do use their contents to compute a default
  // accessible name.
  if (GetDocument() && GetDocument()->GetPage() &&
      GetDocument()->GetPage()->InsidePortal()) {
    return kIgnoreObject;
  }

  if (IsTableLikeRole() || IsTableRowLikeRole() || IsTableCellLikeRole())
    return kIncludeObject;

  // Ignore labels that are already referenced by a control but are not set to
  // be focusable.
  AXObject* control_ax_object = CorrespondingControlAXObjectForLabelElement();
  if (control_ax_object && control_ax_object->IsCheckboxOrRadio() &&
      control_ax_object->NameFromLabelElement() &&
      AccessibleNode::GetPropertyOrARIAAttribute(
          LabelElementContainer(), AOMStringProperty::kRole) == g_null_atom) {
    AXObject* label_ax_object = CorrespondingLabelAXObject();
    // If the label is set to be focusable, we should expose it.
    if (label_ax_object && label_ax_object->CanSetFocusAttribute())
      return kIncludeObject;

    if (ignored_reasons) {
      if (label_ax_object && label_ax_object != this)
        ignored_reasons->push_back(
            IgnoredReason(kAXLabelContainer, label_ax_object));

      ignored_reasons->push_back(IgnoredReason(kAXLabelFor, control_ax_object));
    }
    return kIgnoreObject;
  }

  if (GetNode() && !IsA<HTMLBodyElement>(GetNode()) && CanSetFocusAttribute())
    return kIncludeObject;

  if (IsLink() || IsInPageLinkTarget())
    return kIncludeObject;

  // A click handler might be placed on an otherwise ignored non-empty block
  // element, e.g. a div. We shouldn't ignore such elements because if an AT
  // sees the |ax::mojom::DefaultActionVerb::kClickAncestor|, it will look for
  // the clickable ancestor and it expects to find one.
  if (IsClickable())
    return kIncludeObject;

  if (IsHeading() || IsLandmarkRelated())
    return kIncludeObject;

  // Header and footer tags may also be exposed as landmark roles but not
  // always.
  if (GetNode() && (GetNode()->HasTagName(html_names::kHeaderTag) ||
                    GetNode()->HasTagName(html_names::kFooterTag)))
    return kIncludeObject;

  // All controls are accessible.
  if (IsControl())
    return kIncludeObject;

  // Anything with an explicit ARIA role should be included.
  if (AriaRoleAttribute() != ax::mojom::Role::kUnknown)
    return kIncludeObject;

  // Anything with CSS alt should be included.
  // Note: this is duplicated from AXLayoutObject because CSS alt text may apply
  // to both Elements and pseudo-elements.
  base::Optional<String> alt_text = GetCSSAltText(GetNode());
  if (alt_text && !alt_text->IsEmpty())
    return kIncludeObject;

  // Don't ignore labels, because they serve as TitleUIElements.
  Node* node = GetNode();
  if (IsA<HTMLLabelElement>(node))
    return kIncludeObject;

  // Anything that is content editable should not be ignored.
  // However, one cannot just call node->hasEditableStyle() since that will ask
  // if its parents are also editable. Only the top level content editable
  // region should be exposed.
  if (HasContentEditableAttributeSet())
    return kIncludeObject;

  static const HashSet<ax::mojom::Role> always_included_computed_roles = {
      ax::mojom::Role::kAbbr,
      ax::mojom::Role::kBlockquote,
      ax::mojom::Role::kContentDeletion,
      ax::mojom::Role::kContentInsertion,
      ax::mojom::Role::kDetails,
      ax::mojom::Role::kDialog,
      ax::mojom::Role::kFigcaption,
      ax::mojom::Role::kFigure,
      ax::mojom::Role::kListItem,
      ax::mojom::Role::kMark,
      ax::mojom::Role::kMath,
      ax::mojom::Role::kMeter,
      ax::mojom::Role::kPluginObject,
      ax::mojom::Role::kProgressIndicator,
      ax::mojom::Role::kRuby,
      ax::mojom::Role::kSplitter,
      ax::mojom::Role::kTime,
  };

  if (always_included_computed_roles.find(RoleValue()) !=
      always_included_computed_roles.end())
    return kIncludeObject;

  // If this element has aria attributes on it, it should not be ignored.
  if (HasGlobalARIAAttribute())
    return kIncludeObject;

  bool has_non_empty_alt_attribute = !GetAttribute(kAltAttr).IsEmpty();
  if (IsImage()) {
    if (has_non_empty_alt_attribute || GetAttribute(kAltAttr).IsNull())
      return kIncludeObject;
    else if (ignored_reasons)
      ignored_reasons->push_back(IgnoredReason(kAXEmptyAlt));
    return kIgnoreObject;
  }
  // Using the title or accessibility description (so we
  // check if there's some kind of accessible name for the element)
  // to decide an element's visibility is not as definitive as
  // previous checks, so this should remain as one of the last.
  //
  // These checks are simplified in the interest of execution speed;
  // for example, any element having an alt attribute will make it
  // not ignored, rather than just images.
  if (HasAriaAttribute() || !GetAttribute(kTitleAttr).IsEmpty() ||
      has_non_empty_alt_attribute)
    return kIncludeObject;

  // <span> tags are inline tags and not meant to convey information if they
  // have no other ARIA information on them. If we don't ignore them, they may
  // emit signals expected to come from their parent.
  if (node && IsA<HTMLSpanElement>(node)) {
    if (ignored_reasons)
      ignored_reasons->push_back(IgnoredReason(kAXUninteresting));
    return kIgnoreObject;
  }

  return kDefaultBehavior;
}

base::Optional<String> AXNodeObject::GetCSSAltText(Node* node) {
  if (!node || !node->GetComputedStyle() ||
      node->GetComputedStyle()->ContentBehavesAsNormal()) {
    return base::nullopt;
  }

  const ComputedStyle* style = node->GetComputedStyle();
  if (node->IsPseudoElement()) {
    for (const ContentData* content_data = style->GetContentData();
         content_data; content_data = content_data->Next()) {
      if (content_data->IsAltText())
        return To<AltTextContentData>(content_data)->GetText();
    }
    return base::nullopt;
  }

  // If the content property is used on a non-pseudo element, match the
  // behaviour of LayoutObject::CreateObject and only honour the style if
  // there is exactly one piece of content, which is an image.
  const ContentData* content_data = style->GetContentData();
  if (content_data && content_data->IsImage() && content_data->Next() &&
      content_data->Next()->IsAltText()) {
    return To<AltTextContentData>(content_data->Next())->GetText();
  }

  return base::nullopt;
}

bool AXNodeObject::ComputeAccessibilityIsIgnored(
    IgnoredReasons* ignored_reasons) const {
#if DCHECK_IS_ON()
  // Double-check that an AXObject is never accessed before
  // it's been initialized.
  DCHECK(initialized_);
#endif

  // All nodes must have an unignored parent within their tree under
  // kRootWebArea, so force kRootWebArea to always be unignored.
  if (role_ == ax::mojom::Role::kRootWebArea)
    return false;

  if (GetLayoutObject()) {
    if (role_ == ax::mojom::Role::kUnknown) {
      if (ignored_reasons)
        ignored_reasons->push_back(IgnoredReason(kAXUninteresting));
      return true;
    }
    return false;
  }

  if (DisplayLockUtilities::NearestLockedExclusiveAncestor(*GetNode())) {
    if (DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
            *GetNode(), DisplayLockActivationReason::kAccessibility)) {
      if (ignored_reasons)
        ignored_reasons->push_back(IgnoredReason(kAXNotRendered));
      return true;
    }
    return ShouldIncludeBasedOnSemantics(ignored_reasons) == kIgnoreObject;
  }

  auto* element = DynamicTo<Element>(GetNode());
  if (!element)
    element = GetNode()->parentElement();

  if (!element)
    return true;

  if (element->IsInCanvasSubtree())
    return ShouldIncludeBasedOnSemantics(ignored_reasons) == kIgnoreObject;

  if (AOMPropertyOrARIAAttributeIsFalse(AOMBooleanProperty::kHidden))
    return false;

  if (element->HasDisplayContentsStyle()) {
    if (ShouldIncludeBasedOnSemantics(ignored_reasons) == kIncludeObject)
      return false;
  }

  if (ignored_reasons)
    ignored_reasons->push_back(IgnoredReason(kAXNotRendered));
  return true;
}

static bool IsListElement(Node* node) {
  return IsA<HTMLUListElement>(*node) || IsA<HTMLOListElement>(*node) ||
         IsA<HTMLDListElement>(*node);
}

static bool IsRequiredOwnedElement(AXObject* parent,
                                   ax::mojom::Role current_role,
                                   HTMLElement* current_element) {
  Node* parent_node = parent->GetNode();
  auto* parent_html_element = DynamicTo<HTMLElement>(parent_node);
  if (!parent_html_element)
    return false;

  if (current_role == ax::mojom::Role::kListItem)
    return IsListElement(parent_node);
  if (current_role == ax::mojom::Role::kListMarker)
    return IsA<HTMLLIElement>(*parent_node);
  if (current_role == ax::mojom::Role::kMenuItemCheckBox ||
      current_role == ax::mojom::Role::kMenuItem ||
      current_role == ax::mojom::Role::kMenuItemRadio)
    return IsA<HTMLMenuElement>(*parent_node);

  if (!current_element)
    return false;
  if (IsA<HTMLTableCellElement>(*current_element))
    return IsA<HTMLTableRowElement>(*parent_node);
  if (IsA<HTMLTableRowElement>(*current_element))
    return IsA<HTMLTableSectionElement>(parent_html_element);

  // In case of ListboxRole and its child, ListBoxOptionRole, inheritance of
  // presentation role is handled in AXListBoxOption because ListBoxOption Role
  // doesn't have any child.
  // If it's just ignored because of presentation, we can't see any AX tree
  // related to ListBoxOption.
  return false;
}

const AXObject* AXNodeObject::InheritsPresentationalRoleFrom() const {
  // ARIA states if an item can get focus, it should not be presentational.
  if (CanSetFocusAttribute())
    return nullptr;

  if (IsPresentational())
    return this;

  // http://www.w3.org/TR/wai-aria/complete#presentation
  // ARIA spec says that the user agent MUST apply an inherited role of
  // presentation
  // to any owned elements that do not have an explicit role defined.
  if (AriaRoleAttribute() != ax::mojom::Role::kUnknown)
    return nullptr;

  AXObject* parent = ParentObject();
  if (!parent)
    return nullptr;

  auto* element = DynamicTo<HTMLElement>(GetNode());
  if (!parent->HasInheritedPresentationalRole())
    return nullptr;

  // ARIA spec says that when a parent object is presentational and this object
  // is a required owned element of that parent, then this object is also
  // presentational.
  if (IsRequiredOwnedElement(parent, RoleValue(), element))
    return parent;
  return nullptr;
}

// There should only be one banner/contentInfo per page. If header/footer are
// being used within an article, aside, nave, section, blockquote, details,
// fieldset, figure, td, or main, then it should not be exposed as whole
// page's banner/contentInfo.
static HashSet<QualifiedName>& GetLandmarkRolesNotAllowed() {
  DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, landmark_roles_not_allowed, ());
  if (landmark_roles_not_allowed.IsEmpty()) {
    landmark_roles_not_allowed.insert(html_names::kArticleTag);
    landmark_roles_not_allowed.insert(html_names::kAsideTag);
    landmark_roles_not_allowed.insert(html_names::kNavTag);
    landmark_roles_not_allowed.insert(html_names::kSectionTag);
    landmark_roles_not_allowed.insert(html_names::kBlockquoteTag);
    landmark_roles_not_allowed.insert(html_names::kDetailsTag);
    landmark_roles_not_allowed.insert(html_names::kFieldsetTag);
    landmark_roles_not_allowed.insert(html_names::kFigureTag);
    landmark_roles_not_allowed.insert(html_names::kTdTag);
    landmark_roles_not_allowed.insert(html_names::kMainTag);
  }
  return landmark_roles_not_allowed;
}

bool AXNodeObject::IsDescendantOfElementType(
    HashSet<QualifiedName>& tag_names) const {
  if (!GetNode())
    return false;

  for (Element* parent = GetNode()->parentElement(); parent;
       parent = parent->parentElement()) {
    if (tag_names.Contains(parent->TagQName()))
      return true;
  }
  return false;
}

static bool IsNonEmptyNonHeaderCell(const Node* cell) {
  return cell && cell->hasChildren() && cell->HasTagName(html_names::kTdTag);
}

static bool IsHeaderCell(const Node* cell) {
  return cell && cell->HasTagName(html_names::kThTag);
}

static ax::mojom::Role DecideRoleFromSiblings(Element* cell) {
  // If this header is only cell in its row, it is a column header.
  // It is also a column header if it has a header on either side of it.
  // If instead it has a non-empty td element next to it, it is a row header.

  const Node* next_cell = LayoutTreeBuilderTraversal::NextSibling(*cell);
  const Node* previous_cell =
      LayoutTreeBuilderTraversal::PreviousSibling(*cell);
  if (!next_cell && !previous_cell)
    return ax::mojom::Role::kColumnHeader;
  if (IsHeaderCell(next_cell) && IsHeaderCell(previous_cell))
    return ax::mojom::Role::kColumnHeader;
  if (IsNonEmptyNonHeaderCell(next_cell) ||
      IsNonEmptyNonHeaderCell(previous_cell))
    return ax::mojom::Role::kRowHeader;

  const auto* row = To<Element>(cell->parentNode());
  if (!row || !row->HasTagName(html_names::kTrTag))
    return ax::mojom::Role::kColumnHeader;

  // If this row's first or last cell is a non-empty td, this is a row header.
  // Do the same check for the second and second-to-last cells because tables
  // often have an empty cell at the intersection of the row and column headers.
  const Element* first_cell = ElementTraversal::FirstChild(*row);
  DCHECK(first_cell);

  const Element* last_cell = ElementTraversal::LastChild(*row);
  DCHECK(last_cell);

  if (IsNonEmptyNonHeaderCell(first_cell) || IsNonEmptyNonHeaderCell(last_cell))
    return ax::mojom::Role::kRowHeader;

  if (IsNonEmptyNonHeaderCell(ElementTraversal::NextSibling(*first_cell)) ||
      IsNonEmptyNonHeaderCell(ElementTraversal::PreviousSibling(*last_cell)))
    return ax::mojom::Role::kRowHeader;

  // We have no evidence that this is not a column header.
  return ax::mojom::Role::kColumnHeader;
}

ax::mojom::Role AXNodeObject::DetermineTableSectionRole() const {
  if (!GetElement())
    return ax::mojom::Role::kUnknown;

  AXObject* parent = ParentObject();
  if (!parent || !parent->IsTableLikeRole())
    return ax::mojom::Role::kGenericContainer;

  if (parent->RoleValue() == ax::mojom::Role::kLayoutTable)
    return ax::mojom::Role::kIgnored;

  return ax::mojom::Role::kRowGroup;
}

ax::mojom::Role AXNodeObject::DetermineTableRowRole() const {
  AXObject* parent = ParentObject();
  while (IsNeutralWithinTable(parent))
    parent = parent->ParentObject();

  if (!parent || !parent->IsTableLikeRole())
    return ax::mojom::Role::kGenericContainer;

  if (parent->RoleValue() == ax::mojom::Role::kLayoutTable)
    return ax::mojom::Role::kLayoutTableRow;

  if (parent->IsTableLikeRole())
    return ax::mojom::Role::kRow;

  return ax::mojom::Role::kGenericContainer;
}

ax::mojom::Role AXNodeObject::DetermineTableCellRole() const {
  AXObject* parent = ParentObject();
  if (!parent || !parent->IsTableRowLikeRole())
    return ax::mojom::Role::kGenericContainer;

  // Ensure table container.
  AXObject* grandparent = parent->ParentObject();
  while (IsNeutralWithinTable(grandparent))
    grandparent = grandparent->ParentObject();
  if (!grandparent || !grandparent->IsTableLikeRole())
    return ax::mojom::Role::kGenericContainer;

  if (parent->RoleValue() == ax::mojom::Role::kLayoutTableRow)
    return ax::mojom::Role::kLayoutTableCell;

  if (!GetElement() || !GetNode()->HasTagName(html_names::kThTag))
    return ax::mojom::Role::kCell;

  const AtomicString& scope = GetAttribute(html_names::kScopeAttr);
  if (EqualIgnoringASCIICase(scope, "row") ||
      EqualIgnoringASCIICase(scope, "rowgroup"))
    return ax::mojom::Role::kRowHeader;
  if (EqualIgnoringASCIICase(scope, "col") ||
      EqualIgnoringASCIICase(scope, "colgroup"))
    return ax::mojom::Role::kColumnHeader;

  return DecideRoleFromSiblings(GetElement());
}

// TODO(accessibility) Needs a new name as it does check ARIA, including
// checking the @role for an iframe, and @aria-haspopup/aria-pressed via
// ButtonType().
// TODO(accessibility) This value is cached in native_role_ so it needs to
// be recached if anything it depends on change, such as IsClickable(),
// DataList(), aria-pressed, the parent's tag, role on an iframe, etc.
ax::mojom::Role AXNodeObject::NativeRoleIgnoringAria() const {
  if (!GetNode())
    return ax::mojom::Role::kUnknown;

  // |HTMLAnchorElement| sets isLink only when it has kHrefAttr.
  if (GetNode()->IsLink()) {
    return IsA<HTMLImageElement>(GetNode()) ? ax::mojom::Role::kImageMap
                                            : ax::mojom::Role::kLink;
  }

  if (IsA<HTMLPortalElement>(*GetNode())) {
    return ax::mojom::Role::kPortal;
  }

  if (IsA<HTMLAnchorElement>(*GetNode())) {
    // We assume that an anchor element is LinkRole if it has event listeners
    // even though it doesn't have kHrefAttr.
    if (IsClickable())
      return ax::mojom::Role::kLink;
    return ax::mojom::Role::kAnchor;
  }

  if (IsA<HTMLButtonElement>(*GetNode()))
    return ButtonRoleType();

  if (IsA<HTMLDetailsElement>(*GetNode()))
    return ax::mojom::Role::kDetails;

  if (IsA<HTMLSummaryElement>(*GetNode())) {
    ContainerNode* parent = LayoutTreeBuilderTraversal::Parent(*GetNode());
    if (IsA<HTMLSlotElement>(parent))
      parent = LayoutTreeBuilderTraversal::Parent(*parent);
    if (parent && IsA<HTMLDetailsElement>(parent))
      return ax::mojom::Role::kDisclosureTriangle;
    return ax::mojom::Role::kUnknown;
  }

  // Chrome exposes both table markup and table CSS as a tables, letting
  // the screen reader determine what to do for CSS tables.
  if (IsA<HTMLTableElement>(*GetNode())) {
    return IsDataTable() ? ax::mojom::Role::kTable
                         : ax::mojom::Role::kLayoutTable;
  }
  if (IsA<HTMLTableRowElement>(*GetNode()))
    return DetermineTableRowRole();
  if (IsA<HTMLTableCellElement>(*GetNode()))
    return DetermineTableCellRole();
  if (IsA<HTMLTableSectionElement>(*GetNode()))
    return DetermineTableSectionRole();

  if (const auto* input = DynamicTo<HTMLInputElement>(*GetNode())) {
    const AtomicString& type = input->type();
    if (input->DataList() && type != input_type_names::kColor)
      return ax::mojom::Role::kTextFieldWithComboBox;
    if (type == input_type_names::kButton) {
      if ((GetNode()->parentNode() &&
           IsA<HTMLMenuElement>(GetNode()->parentNode())) ||
          (ParentObject() &&
           ParentObject()->RoleValue() == ax::mojom::Role::kMenu))
        return ax::mojom::Role::kMenuItem;
      return ButtonRoleType();
    }
    if (type == input_type_names::kCheckbox) {
      if ((GetNode()->parentNode() &&
           IsA<HTMLMenuElement>(GetNode()->parentNode())) ||
          (ParentObject() &&
           ParentObject()->RoleValue() == ax::mojom::Role::kMenu))
        return ax::mojom::Role::kMenuItemCheckBox;
      return ax::mojom::Role::kCheckBox;
    }
    if (type == input_type_names::kDate)
      return ax::mojom::Role::kDate;
    if (type == input_type_names::kDatetime ||
        type == input_type_names::kDatetimeLocal ||
        type == input_type_names::kMonth || type == input_type_names::kWeek)
      return ax::mojom::Role::kDateTime;
    if (type == input_type_names::kFile)
      return ax::mojom::Role::kButton;
    if (type == input_type_names::kRadio) {
      if ((GetNode()->parentNode() &&
           IsA<HTMLMenuElement>(GetNode()->parentNode())) ||
          (ParentObject() &&
           ParentObject()->RoleValue() == ax::mojom::Role::kMenu))
        return ax::mojom::Role::kMenuItemRadio;
      return ax::mojom::Role::kRadioButton;
    }
    if (type == input_type_names::kNumber)
      return ax::mojom::Role::kSpinButton;
    if (input->IsTextButton())
      return ButtonRoleType();
    if (type == input_type_names::kRange)
      return ax::mojom::Role::kSlider;
    if (type == input_type_names::kSearch)
      return ax::mojom::Role::kSearchBox;
    if (type == input_type_names::kColor)
      return ax::mojom::Role::kColorWell;
    if (type == input_type_names::kTime)
      return ax::mojom::Role::kInputTime;
    if (type == input_type_names::kButton || type == input_type_names::kImage ||
        type == input_type_names::kReset || type == input_type_names::kSubmit) {
      return ax::mojom::Role::kButton;
    }
    return ax::mojom::Role::kTextField;
  }

  if (auto* select_element = DynamicTo<HTMLSelectElement>(*GetNode())) {
    return select_element->IsMultiple() ? ax::mojom::Role::kListBox
                                        : ax::mojom::Role::kPopUpButton;
  }

  if (auto* option = DynamicTo<HTMLOptionElement>(*GetNode())) {
    HTMLSelectElement* select_element = option->OwnerSelectElement();
    return !select_element || select_element->IsMultiple()
               ? ax::mojom::Role::kListBoxOption
               : ax::mojom::Role::kMenuListOption;
  }

  if (IsA<HTMLTextAreaElement>(*GetNode()))
    return ax::mojom::Role::kTextField;

  if (HeadingLevel())
    return ax::mojom::Role::kHeading;

  if (IsA<HTMLDivElement>(*GetNode()))
    return ax::mojom::Role::kGenericContainer;

  if (IsA<HTMLMeterElement>(*GetNode()))
    return ax::mojom::Role::kMeter;

  if (IsA<HTMLProgressElement>(*GetNode()))
    return ax::mojom::Role::kProgressIndicator;

  if (IsA<HTMLOutputElement>(*GetNode()))
    return ax::mojom::Role::kStatus;

  if (IsA<HTMLParagraphElement>(*GetNode()))
    return ax::mojom::Role::kParagraph;

  if (IsA<HTMLLabelElement>(*GetNode()))
    return ax::mojom::Role::kLabelText;

  if (IsA<HTMLLegendElement>(*GetNode()))
    return ax::mojom::Role::kLegend;

  if (IsA<HTMLRubyElement>(*GetNode()))
    return ax::mojom::Role::kRuby;

  if (IsA<HTMLDListElement>(*GetNode()))
    return ax::mojom::Role::kDescriptionList;

  if (IsA<HTMLAudioElement>(*GetNode()))
    return ax::mojom::Role::kAudio;
  if (IsA<HTMLVideoElement>(*GetNode()))
    return ax::mojom::Role::kVideo;

  if (GetNode()->HasTagName(html_names::kDdTag))
    return ax::mojom::Role::kDescriptionListDetail;

  if (GetNode()->HasTagName(html_names::kDtTag))
    return ax::mojom::Role::kDescriptionListTerm;

  if (GetNode()->nodeName() == mathml_names::kMathTag.LocalName())
    return ax::mojom::Role::kMath;

  if (GetNode()->HasTagName(html_names::kRpTag) ||
      GetNode()->HasTagName(html_names::kRtTag))
    return ax::mojom::Role::kRubyAnnotation;

  if (IsA<HTMLFormElement>(*GetNode()))
    return ax::mojom::Role::kForm;

  if (GetNode()->HasTagName(html_names::kAbbrTag))
    return ax::mojom::Role::kAbbr;

  if (GetNode()->HasTagName(html_names::kArticleTag))
    return ax::mojom::Role::kArticle;

  if (GetNode()->HasTagName(html_names::kCodeTag))
    return ax::mojom::Role::kCode;

  if (GetNode()->HasTagName(html_names::kEmTag))
    return ax::mojom::Role::kEmphasis;

  if (GetNode()->HasTagName(html_names::kStrongTag))
    return ax::mojom::Role::kStrong;

  if (GetNode()->HasTagName(html_names::kDelTag))
    return ax::mojom::Role::kContentDeletion;

  if (GetNode()->HasTagName(html_names::kInsTag))
    return ax::mojom::Role::kContentInsertion;

  if (GetNode()->HasTagName(html_names::kMainTag))
    return ax::mojom::Role::kMain;

  if (GetNode()->HasTagName(html_names::kMarkTag))
    return ax::mojom::Role::kMark;

  if (GetNode()->HasTagName(html_names::kNavTag))
    return ax::mojom::Role::kNavigation;

  if (GetNode()->HasTagName(html_names::kAsideTag))
    return ax::mojom::Role::kComplementary;

  if (GetNode()->HasTagName(html_names::kPreTag))
    return ax::mojom::Role::kPre;

  if (GetNode()->HasTagName(html_names::kSectionTag))
    return ax::mojom::Role::kSection;

  if (GetNode()->HasTagName(html_names::kAddressTag))
    return ax::mojom::Role::kGenericContainer;

  if (IsA<HTMLDialogElement>(*GetNode()))
    return ax::mojom::Role::kDialog;

  // The HTML element.
  if (IsA<HTMLHtmlElement>(GetNode()))
    return ax::mojom::Role::kGenericContainer;

  // Treat <iframe> and <frame> the same.
  if (IsA<HTMLIFrameElement>(*GetNode()) || IsA<HTMLFrameElement>(*GetNode())) {
    const AtomicString& aria_role =
        GetAOMPropertyOrARIAAttribute(AOMStringProperty::kRole);
    if (aria_role == "none" || aria_role == "presentation")
      return ax::mojom::Role::kIframePresentational;
    return ax::mojom::Role::kIframe;
  }

  // There should only be one banner/contentInfo per page. If header/footer are
  // being used within an article or section then it should not be exposed as
  // whole page's banner/contentInfo but as a generic container role.
  if (GetNode()->HasTagName(html_names::kHeaderTag)) {
    if (IsDescendantOfElementType(GetLandmarkRolesNotAllowed()))
      return ax::mojom::Role::kHeaderAsNonLandmark;
    return ax::mojom::Role::kHeader;
  }

  if (GetNode()->HasTagName(html_names::kFooterTag)) {
    if (IsDescendantOfElementType(GetLandmarkRolesNotAllowed()))
      return ax::mojom::Role::kFooterAsNonLandmark;
    return ax::mojom::Role::kFooter;
  }

  if (GetNode()->HasTagName(html_names::kBlockquoteTag))
    return ax::mojom::Role::kBlockquote;

  if (GetNode()->HasTagName(html_names::kCaptionTag))
    return ax::mojom::Role::kCaption;

  if (GetNode()->HasTagName(html_names::kFigcaptionTag))
    return ax::mojom::Role::kFigcaption;

  if (GetNode()->HasTagName(html_names::kFigureTag))
    return ax::mojom::Role::kFigure;

  if (GetNode()->HasTagName(html_names::kTimeTag))
    return ax::mojom::Role::kTime;

  if (IsA<HTMLPlugInElement>(GetNode())) {
    if (IsA<HTMLEmbedElement>(GetNode()))
      return ax::mojom::Role::kEmbeddedObject;
    return ax::mojom::Role::kPluginObject;
  }

  if (IsA<HTMLHRElement>(*GetNode()))
    return ax::mojom::Role::kSplitter;

  if (IsFieldset())
    return ax::mojom::Role::kGroup;

  return ax::mojom::Role::kUnknown;
}

ax::mojom::Role AXNodeObject::DetermineAccessibilityRole() {
  if (!GetNode())
    return ax::mojom::Role::kUnknown;

  native_role_ = NativeRoleIgnoringAria();

  if ((aria_role_ = DetermineAriaRoleAttribute()) != ax::mojom::Role::kUnknown)
    return aria_role_;
  if (GetNode()->IsTextNode())
    return ax::mojom::Role::kStaticText;

  return native_role_ == ax::mojom::Role::kUnknown
             ? ax::mojom::Role::kGenericContainer
             : native_role_;
}

void AXNodeObject::AccessibilityChildrenFromAOMProperty(
    AOMRelationListProperty property,
    AXObject::AXObjectVector& children) const {
  HeapVector<Member<Element>> elements;
  if (!HasAOMPropertyOrARIAAttribute(property, elements))
    return;
  AXObjectCacheImpl& cache = AXObjectCache();
  for (const auto& element : elements) {
    if (AXObject* child = cache.GetOrCreate(element)) {
      // Only aria-labelledby and aria-describedby can target hidden elements.
      if (child->AccessibilityIsIgnored() &&
          property != AOMRelationListProperty::kLabeledBy &&
          property != AOMRelationListProperty::kDescribedBy) {
        continue;
      }
      children.push_back(child);
    }
  }
}

bool AXNodeObject::IsMultiline() const {
  Node* node = this->GetNode();
  if (!node)
    return false;

  const ax::mojom::Role role = RoleValue();
  const bool is_edit_box = role == ax::mojom::Role::kSearchBox ||
                           role == ax::mojom::Role::kTextField;
  if (!IsEditable() && !is_edit_box)
    return false;  // Doesn't support multiline.

  // Supports aria-multiline, so check for attribute.
  bool is_multiline = false;
  if (HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kMultiline,
                                    is_multiline)) {
    return is_multiline;
  }

  // Default for <textarea> is true.
  if (IsA<HTMLTextAreaElement>(*node))
    return true;

  // Default for other edit boxes is false, including for ARIA, says CORE-AAM.
  if (is_edit_box)
    return false;

  // If root of contenteditable area and no ARIA role of textbox/searchbox used,
  // default to multiline=true which is what the default behavior is.
  return HasContentEditableAttributeSet();
}

// This only returns true if this is the element that actually has the
// contentEditable attribute set, unlike node->hasEditableStyle() which will
// also return true if an ancestor is editable.
bool AXNodeObject::HasContentEditableAttributeSet() const {
  const AtomicString& content_editable_value =
      GetAttribute(html_names::kContenteditableAttr);
  if (content_editable_value.IsNull())
    return false;
  // Both "true" (case-insensitive) and the empty string count as true.
  return content_editable_value.IsEmpty() ||
         EqualIgnoringASCIICase(content_editable_value, "true");
}

bool AXNodeObject::IsTextControl() const {
  if (!GetNode())
    return false;

  if (IsNativeTextControl() || HasContentEditableAttributeSet() ||
      IsARIATextControl()) {
    return true;
  }

  return false;
}

AXObject* AXNodeObject::MenuButtonForMenu() const {
  Element* menu_item = MenuItemElementForMenu();

  if (menu_item) {
    // ARIA just has generic menu items. AppKit needs to know if this is a top
    // level items like MenuBarButton or MenuBarItem
    AXObject* menu_item_ax = AXObjectCache().GetOrCreate(menu_item);
    if (menu_item_ax && menu_item_ax->IsMenuButton())
      return menu_item_ax;
  }
  return nullptr;
}

AXObject* AXNodeObject::MenuButtonForMenuIfExists() const {
  Element* menu_item = MenuItemElementForMenu();

  if (menu_item) {
    // ARIA just has generic menu items. AppKit needs to know if this is a top
    // level items like MenuBarButton or MenuBarItem
    AXObject* menu_item_ax = AXObjectCache().Get(menu_item);
    if (menu_item_ax && menu_item_ax->IsMenuButton())
      return menu_item_ax;
  }
  return nullptr;
}

static Element* SiblingWithAriaRole(String role, Node* node) {
  Node* parent = LayoutTreeBuilderTraversal::Parent(*node);
  if (!parent)
    return nullptr;

  for (Node* sibling = LayoutTreeBuilderTraversal::FirstChild(*parent); sibling;
       sibling = LayoutTreeBuilderTraversal::NextSibling(*sibling)) {
    auto* element = DynamicTo<Element>(sibling);
    if (!element)
      continue;
    const AtomicString& sibling_aria_role =
        AccessibleNode::GetPropertyOrARIAAttribute(element,
                                                   AOMStringProperty::kRole);
    if (EqualIgnoringASCIICase(sibling_aria_role, role))
      return element;
  }

  return nullptr;
}

Element* AXNodeObject::MenuItemElementForMenu() const {
  if (AriaRoleAttribute() != ax::mojom::Role::kMenu)
    return nullptr;

  return SiblingWithAriaRole("menuitem", GetNode());
}

Element* AXNodeObject::MouseButtonListener() const {
  Node* node = this->GetNode();
  if (!node)
    return nullptr;

  auto* element = DynamicTo<Element>(node);
  if (!element)
    node = node->parentElement();

  if (!node)
    return nullptr;

  for (element = To<Element>(node); element;
       element = element->parentElement()) {
    if (element->HasEventListeners(event_type_names::kClick) ||
        element->HasEventListeners(event_type_names::kMousedown) ||
        element->HasEventListeners(event_type_names::kMouseup) ||
        element->HasEventListeners(event_type_names::kDOMActivate))
      return element;
  }

  return nullptr;
}

void AXNodeObject::Init() {
#if DCHECK_IS_ON()
  DCHECK(!initialized_);
  initialized_ = true;
#endif
  AXObject::Init();
}

void AXNodeObject::Detach() {
  AXObject::Detach();
  node_ = nullptr;
}

bool AXNodeObject::IsDetached() const {
  return !node_ || AXObject::IsDetached();
}

bool AXNodeObject::IsAXNodeObject() const {
  return true;
}

bool AXNodeObject::IsControl() const {
  Node* node = this->GetNode();
  if (!node)
    return false;

  auto* element = DynamicTo<Element>(node);
  return ((element && element->IsFormControlElement()) ||
          AXObject::IsARIAControl(AriaRoleAttribute()));
}

bool AXNodeObject::IsControllingVideoElement() const {
  Node* node = this->GetNode();
  if (!node)
    return true;

  return IsA<HTMLVideoElement>(
      MediaControlElementsHelper::ToParentMediaElement(node));
}

bool AXNodeObject::ComputeIsEditableRoot() const {
  Node* node = GetNode();
  if (!node)
    return false;
  if (IsNativeTextControl())
    return true;
  if (IsRootEditableElement(*node)) {
    // Editable roots created by the user agent are handled by
    // |IsNativeTextControl| above.
    ShadowRoot* root = node->ContainingShadowRoot();
    return !root || !root->IsUserAgent();
  }
  return false;
}

bool AXNodeObject::IsFieldset() const {
  return IsA<HTMLFieldSetElement>(GetNode());
}

bool AXNodeObject::IsHovered() const {
  if (Node* node = this->GetNode())
    return node->IsHovered();
  return false;
}

bool AXNodeObject::IsImageButton() const {
  return IsNativeImage() && IsButton();
}

bool AXNodeObject::IsInputImage() const {
  auto* html_input_element = DynamicTo<HTMLInputElement>(this->GetNode());
  if (html_input_element && RoleValue() == ax::mojom::Role::kButton)
    return html_input_element->type() == input_type_names::kImage;

  return false;
}

// It is not easily possible to find out if an element is the target of an
// in-page link.
// As a workaround, we check if the element is a sectioning element with an ID,
// or an anchor with a name.
bool AXNodeObject::IsInPageLinkTarget() const {
  auto* element = DynamicTo<Element>(node_.Get());
  if (!element)
    return false;
  // We exclude elements that are in the shadow DOM.
  if (element->ContainingShadowRoot())
    return false;

  if (auto* anchor = DynamicTo<HTMLAnchorElement>(element)) {
    return anchor->HasName() || anchor->HasID();
  }

  if (element->HasID() &&
      (IsLandmarkRelated() || IsA<HTMLSpanElement>(element) ||
       IsA<HTMLDivElement>(element))) {
    return true;
  }
  return false;
}

bool AXNodeObject::IsMultiSelectable() const {
  switch (RoleValue()) {
    case ax::mojom::Role::kGrid:
    case ax::mojom::Role::kTreeGrid:
    case ax::mojom::Role::kTree:
    case ax::mojom::Role::kListBox:
    case ax::mojom::Role::kTabList: {
      bool multiselectable = false;
      if (HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kMultiselectable,
                                        multiselectable)) {
        return multiselectable;
      }
      break;
    }
    default:
      break;
  }

  auto* html_select_element = DynamicTo<HTMLSelectElement>(GetNode());
  return html_select_element && html_select_element->IsMultiple();
}

bool AXNodeObject::IsNativeCheckboxOrRadio() const {
  if (const auto* input = DynamicTo<HTMLInputElement>(GetNode())) {
    return input->type() == input_type_names::kCheckbox ||
           input->type() == input_type_names::kRadio;
  }
  return false;
}

bool AXNodeObject::IsNativeImage() const {
  Node* node = this->GetNode();
  if (!node)
    return false;

  if (IsA<HTMLImageElement>(*node) || IsA<HTMLPlugInElement>(*node))
    return true;

  if (const auto* input = DynamicTo<HTMLInputElement>(*node))
    return input->type() == input_type_names::kImage;

  return false;
}

bool AXNodeObject::IsNativeTextControl() const {
  Node* node = this->GetNode();
  if (!node)
    return false;

  if (IsA<HTMLTextAreaElement>(*node))
    return true;

  if (const auto* input = DynamicTo<HTMLInputElement>(*node))
    return input->IsTextField();

  return false;
}

bool AXNodeObject::IsNonNativeTextControl() const {
  if (IsNativeTextControl())
    return false;

  if (HasContentEditableAttributeSet())
    return true;

  if (IsARIATextControl())
    return true;

  return false;
}

bool AXNodeObject::IsOffScreen() const {
  return DisplayLockUtilities::NearestLockedExclusiveAncestor(*GetNode());
}

bool AXNodeObject::IsPasswordField() const {
  auto* html_input_element = DynamicTo<HTMLInputElement>(this->GetNode());
  if (!html_input_element)
    return false;

  ax::mojom::Role aria_role = AriaRoleAttribute();
  if (aria_role != ax::mojom::Role::kTextField &&
      aria_role != ax::mojom::Role::kUnknown)
    return false;

  return html_input_element->type() == input_type_names::kPassword;
}

bool AXNodeObject::IsProgressIndicator() const {
  return RoleValue() == ax::mojom::Role::kProgressIndicator;
}

bool AXNodeObject::IsRichlyEditable() const {
  return HasContentEditableAttributeSet();
}

bool AXNodeObject::IsSlider() const {
  return RoleValue() == ax::mojom::Role::kSlider;
}

bool AXNodeObject::IsSpinButton() const {
  return RoleValue() == ax::mojom::Role::kSpinButton;
}

bool AXNodeObject::IsNativeSlider() const {
  if (const auto* input = DynamicTo<HTMLInputElement>(GetNode()))
    return input->type() == input_type_names::kRange;
  return false;
}

bool AXNodeObject::IsNativeSpinButton() const {
  if (const auto* input = DynamicTo<HTMLInputElement>(GetNode()))
    return input->type() == input_type_names::kNumber;
  return false;
}

bool AXNodeObject::IsClickable() const {
  Node* node = GetNode();
  if (!node)
    return false;
  auto* element = DynamicTo<Element>(node);
  if (element && element->IsDisabledFormControl()) {
    return false;
  }

  // Note: we can't call |node->WillRespondToMouseClickEvents()| because that
  // triggers a style recalc and can delete this.
  if (node->HasEventListeners(event_type_names::kMouseup) ||
      node->HasEventListeners(event_type_names::kMousedown) ||
      node->HasEventListeners(event_type_names::kClick) ||
      node->HasEventListeners(event_type_names::kDOMActivate)) {
    return true;
  }

  return IsTextControl() || AXObject::IsClickable();
}

AXRestriction AXNodeObject::Restriction() const {
  Element* elem = GetElement();
  if (!elem)
    return kRestrictionNone;

  // An <optgroup> is not exposed directly in the AX tree.
  if (IsA<HTMLOptGroupElement>(elem))
    return kRestrictionNone;

  // According to ARIA, all elements of the base markup can be disabled.
  // According to CORE-AAM, any focusable descendant of aria-disabled
  // ancestor is also disabled.
  bool is_disabled;
  if (HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kDisabled,
                                    is_disabled)) {
    // Has aria-disabled, overrides native markup determining disabled.
    if (is_disabled)
      return kRestrictionDisabled;
  } else if (elem->IsDisabledFormControl() ||
             (CanSetFocusAttribute() && IsDescendantOfDisabledNode())) {
    // No aria-disabled, but other markup says it's disabled.
    return kRestrictionDisabled;
  }

  // Check aria-readonly if supported by current role.
  bool is_read_only;
  if (SupportsARIAReadOnly() &&
      HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kReadOnly,
                                    is_read_only)) {
    // ARIA overrides other readonly state markup.
    return is_read_only ? kRestrictionReadOnly : kRestrictionNone;
  }

  // Only editable fields can be marked @readonly (unlike @aria-readonly).
  auto* text_area_element = DynamicTo<HTMLTextAreaElement>(*elem);
  if (text_area_element && text_area_element->IsReadOnly())
    return kRestrictionReadOnly;
  if (const auto* input = DynamicTo<HTMLInputElement>(*elem)) {
    if (input->IsTextField() && input->IsReadOnly())
      return kRestrictionReadOnly;
  }

  // If a grid cell does not have it's own ARIA input restriction,
  // fall back on parent grid's readonly state.
  // See ARIA specification regarding grid/treegrid and readonly.
  if (IsTableCellLikeRole()) {
    AXObject* row = ParentObjectUnignored();
    if (row && row->IsTableRowLikeRole()) {
      AXObject* table = row->ParentObjectUnignored();
      if (table && table->IsTableLikeRole() &&
          (table->RoleValue() == ax::mojom::Role::kGrid ||
           table->RoleValue() == ax::mojom::Role::kTreeGrid)) {
        if (table->Restriction() == kRestrictionReadOnly)
          return kRestrictionReadOnly;
      }
    }
  }

  // This is a node that is not readonly and not disabled.
  return kRestrictionNone;
}

AccessibilityExpanded AXNodeObject::IsExpanded() const {
  if (!SupportsARIAExpanded())
    return kExpandedUndefined;

  if (GetNode() && IsA<HTMLSummaryElement>(*GetNode())) {
    if (GetNode()->parentNode() &&
        IsA<HTMLDetailsElement>(GetNode()->parentNode()))
      return To<Element>(GetNode()->parentNode())
                     ->FastHasAttribute(html_names::kOpenAttr)
                 ? kExpandedExpanded
                 : kExpandedCollapsed;
  }

  bool expanded = false;
  if (HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kExpanded, expanded)) {
    return expanded ? kExpandedExpanded : kExpandedCollapsed;
  }

  return kExpandedUndefined;
}

bool AXNodeObject::IsModal() const {
  if (RoleValue() != ax::mojom::Role::kDialog &&
      RoleValue() != ax::mojom::Role::kAlertDialog)
    return false;

  bool modal = false;
  if (HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kModal, modal))
    return modal;

  if (GetNode() && IsA<HTMLDialogElement>(*GetNode()))
    return To<Element>(GetNode())->IsInTopLayer();

  return false;
}

bool AXNodeObject::IsRequired() const {
  auto* form_control = DynamicTo<HTMLFormControlElement>(GetNode());
  if (form_control && form_control->IsRequired())
    return true;

  if (AOMPropertyOrARIAAttributeIsTrue(AOMBooleanProperty::kRequired))
    return true;

  return false;
}

bool AXNodeObject::CanvasHasFallbackContent() const {
  if (IsDetached())
    return false;
  Node* node = this->GetNode();
  return IsA<HTMLCanvasElement>(node) && node->hasChildren();
}

int AXNodeObject::HeadingLevel() const {
  // headings can be in block flow and non-block flow
  Node* node = this->GetNode();
  if (!node)
    return 0;

  if (RoleValue() == ax::mojom::Role::kHeading) {
    uint32_t level;
    if (HasAOMPropertyOrARIAAttribute(AOMUIntProperty::kLevel, level)) {
      if (level >= 1 && level <= 9)
        return level;
    }
  }

  auto* element = DynamicTo<HTMLElement>(node);
  if (!element)
    return 0;

  if (element->HasTagName(html_names::kH1Tag))
    return 1;

  if (element->HasTagName(html_names::kH2Tag))
    return 2;

  if (element->HasTagName(html_names::kH3Tag))
    return 3;

  if (element->HasTagName(html_names::kH4Tag))
    return 4;

  if (element->HasTagName(html_names::kH5Tag))
    return 5;

  if (element->HasTagName(html_names::kH6Tag))
    return 6;

  if (RoleValue() == ax::mojom::Role::kHeading)
    return kDefaultHeadingLevel;

  return 0;
}

unsigned AXNodeObject::HierarchicalLevel() const {
  Element* element = GetElement();
  if (!element)
    return 0;

  uint32_t level;
  if (HasAOMPropertyOrARIAAttribute(AOMUIntProperty::kLevel, level)) {
    if (level >= 1 && level <= 9)
      return level;
  }

  // Helper lambda for calculating hierarchical levels by counting ancestor
  // nodes that match a target role.
  auto accumulateLevel = [&](int initial_level, ax::mojom::Role target_role) {
    int level = initial_level;
    for (AXObject* parent = ParentObject(); parent;
         parent = parent->ParentObject()) {
      if (parent->RoleValue() == target_role)
        level++;
    }
    return level;
  };

  switch (RoleValue()) {
    case ax::mojom::Role::kComment:
      // Comment: level is based on counting comment ancestors until the root.
      return accumulateLevel(1, ax::mojom::Role::kComment);
    case ax::mojom::Role::kListItem:
      level = accumulateLevel(0, ax::mojom::Role::kList);
      // When level count is 0 due to this list item not having an ancestor of
      // Role::kList, not nested in list groups, this list item has a level
      // of 1.
      return level == 0 ? 1 : level;
    case ax::mojom::Role::kTabList:
      return accumulateLevel(1, ax::mojom::Role::kTabList);
    case ax::mojom::Role::kTreeItem: {
      // Hierarchy leveling starts at 1, to match the aria-level spec.
      // We measure tree hierarchy by the number of groups that the item is
      // within.
      level = 1;
      for (AXObject* parent = ParentObject(); parent;
           parent = parent->ParentObject()) {
        ax::mojom::Role parent_role = parent->RoleValue();
        if (parent_role == ax::mojom::Role::kGroup)
          level++;
        else if (parent_role == ax::mojom::Role::kTree)
          break;
      }
      return level;
    }
    default:
      return 0;
  }

  return 0;
}

String AXNodeObject::AutoComplete() const {
  // Check cache for auto complete state.
  if (AXObjectCache().GetAutofillState(AXObjectID()) ==
      WebAXAutofillState::kAutocompleteAvailable)
    return "list";

  if (IsNativeTextControl() || IsARIATextControl()) {
    const AtomicString& aria_auto_complete =
        GetAOMPropertyOrARIAAttribute(AOMStringProperty::kAutocomplete)
            .LowerASCII();
    // Illegal values must be passed through, according to CORE-AAM.
    if (!aria_auto_complete.IsNull())
      return aria_auto_complete == "none" ? String() : aria_auto_complete;
  }

  if (auto* input = DynamicTo<HTMLInputElement>(GetNode())) {
    if (input->DataList())
      return "list";
  }

  return String();
}

namespace {

bool MarkerTypeIsUsedForAccessibility(DocumentMarker::MarkerType type) {
  return DocumentMarker::MarkerTypes(
             DocumentMarker::kSpelling | DocumentMarker::kGrammar |
             DocumentMarker::kTextMatch | DocumentMarker::kActiveSuggestion |
             DocumentMarker::kSuggestion | DocumentMarker::kTextFragment)
      .Contains(type);
}

base::Optional<DocumentMarker::MarkerType> GetAriaSpellingOrGrammarMarker(
    const AXObject& obj) {
  const AtomicString& attribute_value =
      obj.GetAOMPropertyOrARIAAttribute(AOMStringProperty::kInvalid);
  if (EqualIgnoringASCIICase(attribute_value, "spelling"))
    return DocumentMarker::kSpelling;
  else if (EqualIgnoringASCIICase(attribute_value, "grammar"))
    return DocumentMarker::kGrammar;
  return base::nullopt;
}

}  // namespace

void AXNodeObject::Markers(Vector<DocumentMarker::MarkerType>& marker_types,
                           Vector<AXRange>& marker_ranges) const {
  if (!GetNode() || !GetDocument() || !GetDocument()->View())
    return;

  auto* text_node = DynamicTo<Text>(GetNode());
  if (!text_node)
    return;

  // First use ARIA markers for spelling/grammar if available.
  // As an optimization, only checks until the nearest block-like ancestor.
  AXObject* ax_ancestor = ParentObjectUnignored();
  base::Optional<DocumentMarker::MarkerType> aria_marker;
  while (ax_ancestor) {
    aria_marker = GetAriaSpellingOrGrammarMarker(*ax_ancestor);
    if (aria_marker)
      break;  // Result obtained.
    if (ax_ancestor->GetNode()) {
      if (const ComputedStyle* style =
              ax_ancestor->GetNode()->GetComputedStyle()) {
        if (style->IsDisplayBlockContainer())
          break;  // Do not go higher than block container.
      }
    }
    ax_ancestor = ax_ancestor->ParentObjectUnignored();
  }
  if (aria_marker) {
    marker_types.push_back(aria_marker.value());
    marker_ranges.push_back(AXRange::RangeOfContents(*this));
  }

  DocumentMarkerController& marker_controller = GetDocument()->Markers();
  DocumentMarkerVector markers = marker_controller.MarkersFor(*text_node);
  for (DocumentMarker* marker : markers) {
    if (!MarkerTypeIsUsedForAccessibility(marker->GetType()) ||
        aria_marker == marker->GetType()) {
      continue;
    }

    const Position start_position(*GetNode(), marker->StartOffset());
    const Position end_position(*GetNode(), marker->EndOffset());
    if (!start_position.IsValidFor(*GetDocument()) ||
        !end_position.IsValidFor(*GetDocument())) {
      continue;
    }

    marker_types.push_back(marker->GetType());
    marker_ranges.emplace_back(
        AXPosition::FromPosition(start_position, TextAffinity::kDownstream,
                                 AXPositionAdjustmentBehavior::kMoveLeft),
        AXPosition::FromPosition(end_position, TextAffinity::kDownstream,
                                 AXPositionAdjustmentBehavior::kMoveRight));
  }
}

AXObject* AXNodeObject::InPageLinkTarget() const {
  if (!IsAnchor() || !GetDocument())
    return AXObject::InPageLinkTarget();

  const Element* anchor = AnchorElement();
  if (!anchor)
    return AXObject::InPageLinkTarget();

  KURL link_url = anchor->HrefURL();
  if (!link_url.IsValid())
    return AXObject::InPageLinkTarget();
  String fragment = link_url.FragmentIdentifier();
  if (fragment.IsEmpty())
    return AXObject::InPageLinkTarget();

  KURL document_url = GetDocument()->Url();
  if (!document_url.IsValid() ||
      !EqualIgnoringFragmentIdentifier(document_url, link_url)) {
    return AXObject::InPageLinkTarget();
  }

  TreeScope& tree_scope = anchor->GetTreeScope();
  Element* target = tree_scope.FindAnchor(fragment);
  if (!target)
    return AXObject::InPageLinkTarget();
  // If the target is not in the accessibility tree, get the first unignored
  // sibling.
  return AXObjectCache().FirstAccessibleObjectFromNode(target);
}

AccessibilityOrientation AXNodeObject::Orientation() const {
  const AtomicString& aria_orientation =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kOrientation);
  AccessibilityOrientation orientation = kAccessibilityOrientationUndefined;
  if (EqualIgnoringASCIICase(aria_orientation, "horizontal"))
    orientation = kAccessibilityOrientationHorizontal;
  else if (EqualIgnoringASCIICase(aria_orientation, "vertical"))
    orientation = kAccessibilityOrientationVertical;

  switch (RoleValue()) {
    case ax::mojom::Role::kListBox:
    case ax::mojom::Role::kMenu:
    case ax::mojom::Role::kScrollBar:
    case ax::mojom::Role::kTree:
      if (orientation == kAccessibilityOrientationUndefined)
        orientation = kAccessibilityOrientationVertical;

      return orientation;
    case ax::mojom::Role::kMenuBar:
    case ax::mojom::Role::kSlider:
    case ax::mojom::Role::kSplitter:
    case ax::mojom::Role::kTabList:
    case ax::mojom::Role::kToolbar:
      if (orientation == kAccessibilityOrientationUndefined)
        orientation = kAccessibilityOrientationHorizontal;

      return orientation;
    case ax::mojom::Role::kComboBoxGrouping:
    case ax::mojom::Role::kComboBoxMenuButton:
    case ax::mojom::Role::kRadioGroup:
    case ax::mojom::Role::kTreeGrid:
      return orientation;
    default:
      return AXObject::Orientation();
  }
}

AXObject::AXObjectVector AXNodeObject::RadioButtonsInGroup() const {
  AXObjectVector radio_buttons;
  if (!node_ || RoleValue() != ax::mojom::Role::kRadioButton)
    return radio_buttons;

  if (auto* node_radio_button = DynamicTo<HTMLInputElement>(node_.Get())) {
    HeapVector<Member<HTMLInputElement>> html_radio_buttons =
        FindAllRadioButtonsWithSameName(node_radio_button);
    for (HTMLInputElement* radio_button : html_radio_buttons) {
      AXObject* ax_radio_button = AXObjectCache().GetOrCreate(radio_button);
      if (ax_radio_button)
        radio_buttons.push_back(ax_radio_button);
    }
    return radio_buttons;
  }

  // If the immediate parent is a radio group, return all its children that are
  // radio buttons.
  AXObject* parent = ParentObject();
  if (parent && parent->RoleValue() == ax::mojom::Role::kRadioGroup) {
    for (AXObject* child : parent->Children()) {
      DCHECK(child);
      if (child->RoleValue() == ax::mojom::Role::kRadioButton &&
          child->AccessibilityIsIncludedInTree()) {
        radio_buttons.push_back(child);
      }
    }
  }

  return radio_buttons;
}

// static
HeapVector<Member<HTMLInputElement>>
AXNodeObject::FindAllRadioButtonsWithSameName(HTMLInputElement* radio_button) {
  HeapVector<Member<HTMLInputElement>> all_radio_buttons;
  if (!radio_button || radio_button->type() != input_type_names::kRadio)
    return all_radio_buttons;

  constexpr bool kTraverseForward = true;
  constexpr bool kTraverseBackward = false;
  HTMLInputElement* first_radio_button = radio_button;
  do {
    radio_button = RadioInputType::NextRadioButtonInGroup(first_radio_button,
                                                          kTraverseBackward);
    if (radio_button)
      first_radio_button = radio_button;
  } while (radio_button);

  HTMLInputElement* next_radio_button = first_radio_button;
  do {
    all_radio_buttons.push_back(next_radio_button);
    next_radio_button = RadioInputType::NextRadioButtonInGroup(
        next_radio_button, kTraverseForward);
  } while (next_radio_button);
  return all_radio_buttons;
}

String AXNodeObject::GetText() const {
  if (!IsTextControl())
    return String();

  Node* node = this->GetNode();
  if (!node)
    return String();

  if (IsNativeTextControl() &&
      (IsA<HTMLTextAreaElement>(*node) || IsA<HTMLInputElement>(*node))) {
    // We should not simply return the "value" attribute because it might be
    // sanitized in some input control types, e.g. email fields. If we do that,
    // then "selectionStart" and "selectionEnd" indices will not match with the
    // text in the sanitized value.
    return ToTextControl(*node).InnerEditorValue();
  }

  auto* element = DynamicTo<Element>(node);
  return element ? element->innerText() : String();
}

RGBA32 AXNodeObject::ColorValue() const {
  auto* input = DynamicTo<HTMLInputElement>(GetNode());
  if (!input || !IsColorWell())
    return AXObject::ColorValue();

  const AtomicString& type = input->getAttribute(kTypeAttr);
  if (!EqualIgnoringASCIICase(type, "color"))
    return AXObject::ColorValue();

  // HTMLInputElement::value always returns a string parseable by Color.
  Color color;
  bool success = color.SetFromString(input->value());
  DCHECK(success);
  return color.Rgb();
}

ax::mojom::AriaCurrentState AXNodeObject::GetAriaCurrentState() const {
  const AtomicString& attribute_value =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kCurrent);
  if (attribute_value.IsNull())
    return ax::mojom::AriaCurrentState::kNone;
  if (attribute_value.IsEmpty() ||
      EqualIgnoringASCIICase(attribute_value, "false"))
    return ax::mojom::AriaCurrentState::kFalse;
  if (EqualIgnoringASCIICase(attribute_value, "true"))
    return ax::mojom::AriaCurrentState::kTrue;
  if (EqualIgnoringASCIICase(attribute_value, "page"))
    return ax::mojom::AriaCurrentState::kPage;
  if (EqualIgnoringASCIICase(attribute_value, "step"))
    return ax::mojom::AriaCurrentState::kStep;
  if (EqualIgnoringASCIICase(attribute_value, "location"))
    return ax::mojom::AriaCurrentState::kLocation;
  if (EqualIgnoringASCIICase(attribute_value, "date"))
    return ax::mojom::AriaCurrentState::kDate;
  if (EqualIgnoringASCIICase(attribute_value, "time"))
    return ax::mojom::AriaCurrentState::kTime;
  // An unknown value should return true.
  if (!attribute_value.IsEmpty())
    return ax::mojom::AriaCurrentState::kTrue;

  return AXObject::GetAriaCurrentState();
}

ax::mojom::InvalidState AXNodeObject::GetInvalidState() const {
  const AtomicString& attribute_value =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kInvalid);
  if (EqualIgnoringASCIICase(attribute_value, "false"))
    return ax::mojom::InvalidState::kFalse;
  if (EqualIgnoringASCIICase(attribute_value, "true"))
    return ax::mojom::InvalidState::kTrue;
  // "spelling" and "grammar" are also invalid values: they are exposed via
  // Markers() as if they are native errors, but also use the invalid entry
  // state on the node itself, therefore they are treated like "true".
  // in terms of the node's invalid state
  // A yet unknown value.
  if (!attribute_value.IsEmpty())
    return ax::mojom::InvalidState::kOther;

  if (GetElement()) {
    ListedElement* form_control = ListedElement::From(*GetElement());
    if (form_control) {
      return form_control->IsNotCandidateOrValid()
                 ? ax::mojom::InvalidState::kFalse
                 : ax::mojom::InvalidState::kTrue;
    }
  }
  return AXObject::GetInvalidState();
}

int AXNodeObject::PosInSet() const {
  if (SupportsARIASetSizeAndPosInSet()) {
    uint32_t pos_in_set;
    if (HasAOMPropertyOrARIAAttribute(AOMUIntProperty::kPosInSet, pos_in_set))
      return pos_in_set;
  }
  return 0;
}

int AXNodeObject::SetSize() const {
  if (SupportsARIASetSizeAndPosInSet()) {
    int32_t set_size;
    if (HasAOMPropertyOrARIAAttribute(AOMIntProperty::kSetSize, set_size))
      return set_size;
  }
  return 0;
}

String AXNodeObject::AriaInvalidValue() const {
  if (GetInvalidState() == ax::mojom::InvalidState::kOther)
    return GetAOMPropertyOrARIAAttribute(AOMStringProperty::kInvalid);

  return String();
}

String AXNodeObject::ValueDescription() const {
  if (!IsRangeValueSupported())
    return String();

  return GetAOMPropertyOrARIAAttribute(AOMStringProperty::kValueText)
      .GetString();
}

bool AXNodeObject::ValueForRange(float* out_value) const {
  float value_now;
  if (HasAOMPropertyOrARIAAttribute(AOMFloatProperty::kValueNow, value_now)) {
    *out_value = value_now;
    return true;
  }

  if (IsNativeSlider() || IsNativeSpinButton()) {
    *out_value = To<HTMLInputElement>(*GetNode()).valueAsNumber();
    return std::isfinite(*out_value);
  }

  if (auto* meter = DynamicTo<HTMLMeterElement>(GetNode())) {
    *out_value = meter->value();
    return true;
  }

  // In ARIA 1.1, default values for aria-valuenow were changed as below.
  // - scrollbar, slider : half way between aria-valuemin and aria-valuemax
  // - separator : 50
  // - spinbutton : 0
  switch (AriaRoleAttribute()) {
    case ax::mojom::Role::kScrollBar:
    case ax::mojom::Role::kSlider: {
      float min_value, max_value;
      if (MinValueForRange(&min_value) && MaxValueForRange(&max_value)) {
        *out_value = (min_value + max_value) / 2.0f;
        return true;
      }
      FALLTHROUGH;
    }
    case ax::mojom::Role::kSplitter: {
      *out_value = 50.0f;
      return true;
    }
    case ax::mojom::Role::kSpinButton: {
      *out_value = 0.0f;
      return true;
    }
    default:
      break;
  }

  return false;
}

bool AXNodeObject::MaxValueForRange(float* out_value) const {
  float value_max;
  if (HasAOMPropertyOrARIAAttribute(AOMFloatProperty::kValueMax, value_max)) {
    *out_value = value_max;
    return true;
  }

  if (IsNativeSlider() || IsNativeSpinButton()) {
    *out_value = static_cast<float>(To<HTMLInputElement>(*GetNode()).Maximum());
    return std::isfinite(*out_value);
  }

  if (auto* meter = DynamicTo<HTMLMeterElement>(GetNode())) {
    *out_value = meter->max();
    return true;
  }

  // In ARIA 1.1, default value of scrollbar, separator and slider
  // for aria-valuemax were changed to 100.
  switch (AriaRoleAttribute()) {
    case ax::mojom::Role::kScrollBar:
    case ax::mojom::Role::kSplitter:
    case ax::mojom::Role::kSlider: {
      *out_value = 100.0f;
      return true;
    }
    default:
      break;
  }

  return false;
}

bool AXNodeObject::MinValueForRange(float* out_value) const {
  float value_min;
  if (HasAOMPropertyOrARIAAttribute(AOMFloatProperty::kValueMin, value_min)) {
    *out_value = value_min;
    return true;
  }

  if (IsNativeSlider() || IsNativeSpinButton()) {
    *out_value = static_cast<float>(To<HTMLInputElement>(*GetNode()).Minimum());
    return std::isfinite(*out_value);
  }

  if (auto* meter = DynamicTo<HTMLMeterElement>(GetNode())) {
    *out_value = meter->min();
    return true;
  }

  // In ARIA 1.1, default value of scrollbar, separator and slider
  // for aria-valuemin were changed to 0.
  switch (AriaRoleAttribute()) {
    case ax::mojom::Role::kScrollBar:
    case ax::mojom::Role::kSplitter:
    case ax::mojom::Role::kSlider: {
      *out_value = 0.0f;
      return true;
    }
    default:
      break;
  }

  return false;
}

bool AXNodeObject::StepValueForRange(float* out_value) const {
  if (IsNativeSlider() || IsNativeSpinButton()) {
    auto step =
        To<HTMLInputElement>(*GetNode()).CreateStepRange(kRejectAny).Step();
    *out_value = step.ToString().ToFloat();
    return std::isfinite(*out_value);
  }

  switch (AriaRoleAttribute()) {
    case ax::mojom::Role::kScrollBar:
    case ax::mojom::Role::kSplitter:
    case ax::mojom::Role::kSlider: {
      *out_value = 0.0f;
      return true;
    }
    default:
      break;
  }

  return false;
}

KURL AXNodeObject::Url() const {
  if (IsAnchor()) {
    const Element* anchor = AnchorElement();

    if (const auto* html_anchor = DynamicTo<HTMLAnchorElement>(anchor)) {
      return html_anchor->Href();
    }

    // Some non-HTML elements, most notably SVG <a> elements, can act as
    // links/anchors.
    if (anchor)
      return anchor->HrefURL();
  }

  if (IsWebArea() && GetDocument())
    return GetDocument()->Url();

  auto* html_image_element = DynamicTo<HTMLImageElement>(GetNode());
  if (IsImage() && html_image_element) {
    // Using ImageSourceURL handles both src and srcset.
    String source_url = html_image_element->ImageSourceURL();
    String stripped_image_source_url =
        StripLeadingAndTrailingHTMLSpaces(source_url);
    if (!stripped_image_source_url.IsEmpty())
      return GetDocument()->CompleteURL(stripped_image_source_url);
  }

  if (IsInputImage())
    return To<HTMLInputElement>(GetNode())->Src();

  return KURL();
}

AXObject* AXNodeObject::ChooserPopup() const {
  // When color & date chooser popups are visible, they can be found in the tree
  // as a WebArea child of the <input> control itself.
  switch (native_role_) {
    case ax::mojom::Role::kColorWell:
    case ax::mojom::Role::kDate:
    case ax::mojom::Role::kDateTime: {
      for (const auto& child : children_) {
        if (child->IsWebArea())
          return child;
      }
      return nullptr;
    }
    default:
      return nullptr;
  }
}

String AXNodeObject::StringValue() const {
  Node* node = this->GetNode();
  if (!node)
    return String();

  if (auto* select_element = DynamicTo<HTMLSelectElement>(*node)) {
    int selected_index = select_element->SelectedListIndex();
    const HeapVector<Member<HTMLElement>>& list_items =
        select_element->GetListItems();
    if (selected_index >= 0 &&
        static_cast<wtf_size_t>(selected_index) < list_items.size()) {
      const AtomicString& overridden_description =
          list_items[selected_index]->FastGetAttribute(
              html_names::kAriaLabelAttr);
      if (!overridden_description.IsNull())
        return overridden_description;
    }
    if (!select_element->IsMultiple())
      return select_element->value();
    return String();
  }

  if (IsNativeTextControl())
    return GetText();

  // Handle other HTML input elements that aren't text controls, like date and
  // time controls, by returning their value converted to text, with the
  // exception of checkboxes and radio buttons (which would return "on"), and
  // buttons which will return their name.
  // https://html.spec.whatwg.org/C/#dom-input-value
  if (const auto* input = DynamicTo<HTMLInputElement>(node)) {
    if (input->type() != input_type_names::kButton &&
        input->type() != input_type_names::kCheckbox &&
        input->type() != input_type_names::kImage &&
        input->type() != input_type_names::kRadio &&
        input->type() != input_type_names::kReset &&
        input->type() != input_type_names::kSubmit) {
      return input->value();
    }
  }

  // ARIA combobox can get value from inner contents.
  if (AriaRoleAttribute() == ax::mojom::Role::kComboBoxMenuButton) {
    AXObjectSet visited;
    return TextFromDescendants(visited, false);
  }

  return String();
}

ax::mojom::Role AXNodeObject::AriaRoleAttribute() const {
  return aria_role_;
}

bool AXNodeObject::HasAriaAttribute() const {
  Element* element = GetElement();
  if (!element)
    return false;

  // Explicit ARIA role should be considered an aria attribute.
  if (AriaRoleAttribute() != ax::mojom::Role::kUnknown)
    return true;

  AttributeCollection attributes = element->AttributesWithoutUpdate();
  for (const Attribute& attr : attributes) {
    // Attributes cache their uppercase names.
    if (attr.GetName().LocalNameUpper().StartsWith("ARIA-"))
      return true;
  }

  return false;
}

// Returns the nearest block-level LayoutBlockFlow ancestor
static LayoutBlockFlow* NonInlineBlockFlow(LayoutObject* object) {
  LayoutObject* current = object;
  while (current) {
    auto* block_flow = DynamicTo<LayoutBlockFlow>(current);
    if (block_flow && !block_flow->IsAtomicInlineLevel())
      return block_flow;
    current = current->Parent();
  }

  NOTREACHED();
  return nullptr;
}

// Returns true if |r1| and |r2| are both non-null, both inline, and are
// contained within the same non-inline LayoutBlockFlow.
static bool IsInSameNonInlineBlockFlow(LayoutObject* r1, LayoutObject* r2) {
  if (!r1 || !r2)
    return false;
  if (!r1->IsInline() || !r2->IsInline())
    return false;
  LayoutBlockFlow* b1 = NonInlineBlockFlow(r1);
  LayoutBlockFlow* b2 = NonInlineBlockFlow(r2);
  return b1 && b2 && b1 == b2;
}

//
// New AX name calculation.
//

String AXNodeObject::GetName(ax::mojom::NameFrom& name_from,
                             AXObjectVector* name_objects) const {
  String name = AXObject::GetName(name_from, name_objects);
  if (RoleValue() == ax::mojom::Role::kSpinButton && DatetimeAncestor()) {
    // Fields inside a datetime control need to merge the field name with
    // the name of the <input> element.
    name_objects->clear();
    String input_name = DatetimeAncestor()->GetName(name_from, name_objects);
    if (!input_name.IsEmpty())
      return name + " " + input_name;
  }

  return name;
}

String AXNodeObject::TextAlternative(bool recursive,
                                     bool in_aria_labelled_by_traversal,
                                     AXObjectSet& visited,
                                     ax::mojom::NameFrom& name_from,
                                     AXRelatedObjectVector* related_objects,
                                     NameSources* name_sources) const {
  // If nameSources is non-null, relatedObjects is used in filling it in, so it
  // must be non-null as well.
  if (name_sources)
    DCHECK(related_objects);

  bool found_text_alternative = false;

  if (!GetNode() && !GetLayoutObject())
    return String();

  // Exclude offscreen objects inside a portal.
  // NOTE: If an object is found to be offscreen, this also omits its children,
  // which may not be offscreen in some cases.
  Page* page = GetNode() ? GetNode()->GetDocument().GetPage() : nullptr;
  if (page && page->InsidePortal()) {
    LayoutRect bounds = GetBoundsInFrameCoordinates();
    IntSize document_size =
        GetNode()->GetDocument().GetLayoutView()->GetLayoutSize();
    bool is_visible = bounds.Intersects(LayoutRect(IntPoint(), document_size));
    if (!is_visible)
      return String();
  }

  String text_alternative = AriaTextAlternative(
      recursive, in_aria_labelled_by_traversal, visited, name_from,
      related_objects, name_sources, &found_text_alternative);
  if (found_text_alternative && !name_sources)
    return text_alternative;

  // Step 2E from: http://www.w3.org/TR/accname-aam-1.1 -- value from control
  if (recursive && !in_aria_labelled_by_traversal && CanSetValueAttribute()) {
    // No need to set any name source info in a recursive call.
    if (IsTextControl())
      return GetText();

    if (IsRangeValueSupported()) {
      const AtomicString& aria_valuetext =
          GetAOMPropertyOrARIAAttribute(AOMStringProperty::kValueText);
      if (!aria_valuetext.IsNull())
        return aria_valuetext.GetString();
      float value;
      if (ValueForRange(&value))
        return String::Number(value);
    }

    return StringValue();
  }

  // Step 2E from: http://www.w3.org/TR/accname-aam-1.1
  // "If the embedded control has role combobox or listbox, return the text
  // alternative of the chosen option."
  if (NameFromSelectedOption(recursive)) {
    StringBuilder accumulated_text;
    AXObjectVector selected_options;
    SelectedOptions(selected_options);
    for (const auto& child : selected_options) {
      if (accumulated_text.length())
        accumulated_text.Append(" ");
      accumulated_text.Append(child->ComputedName());
    }
    return accumulated_text.ToString();
  }

  // Step 2D from: http://www.w3.org/TR/accname-aam-1.1
  text_alternative =
      NativeTextAlternative(visited, name_from, related_objects, name_sources,
                            &found_text_alternative);
  const bool has_text_alternative =
      !text_alternative.IsEmpty() ||
      name_from == ax::mojom::NameFrom::kAttributeExplicitlyEmpty;
  if (has_text_alternative && !name_sources)
    return text_alternative;

  // Step 2F / 2G from: http://www.w3.org/TR/accname-aam-1.1
  if (in_aria_labelled_by_traversal || NameFromContents(recursive)) {
    Node* node = GetNode();
    if (!IsA<HTMLSelectElement>(node)) {  // Avoid option descendant text
      name_from = ax::mojom::NameFrom::kContents;
      if (name_sources) {
        name_sources->push_back(NameSource(found_text_alternative));
        name_sources->back().type = name_from;
      }

      if (auto* text_node = DynamicTo<Text>(node))
        text_alternative = text_node->data();
      else if (IsA<HTMLBRElement>(node))
        text_alternative = String("\n");
      else
        text_alternative = TextFromDescendants(visited, false);

      if (!text_alternative.IsEmpty()) {
        if (name_sources) {
          found_text_alternative = true;
          name_sources->back().text = text_alternative;
        } else {
          return text_alternative;
        }
      }
    }
  }

  // Step 2H from: http://www.w3.org/TR/accname-aam-1.1
  name_from = ax::mojom::NameFrom::kTitle;
  if (name_sources) {
    name_sources->push_back(NameSource(found_text_alternative, kTitleAttr));
    name_sources->back().type = name_from;
  }
  const AtomicString& title = GetAttribute(kTitleAttr);
  if (!title.IsEmpty()) {
    text_alternative = title;
    name_from = ax::mojom::NameFrom::kTitle;
    if (name_sources) {
      found_text_alternative = true;
      name_sources->back().text = text_alternative;
    } else {
      return text_alternative;
    }
  }

  name_from = ax::mojom::NameFrom::kUninitialized;

  if (name_sources && found_text_alternative) {
    for (NameSource& name_source : *name_sources) {
      if (!name_source.text.IsNull() && !name_source.superseded) {
        name_from = name_source.type;
        if (!name_source.related_objects.IsEmpty())
          *related_objects = name_source.related_objects;
        return name_source.text;
      }
    }
  }

  return String();
}

static bool ShouldInsertSpaceBetweenObjectsIfNeeded(
    AXObject* previous,
    AXObject* next,
    ax::mojom::NameFrom last_used_name_from,
    ax::mojom::NameFrom name_from) {
  // If we're going between two layoutObjects that are in separate
  // LayoutBoxes, add whitespace if it wasn't there already. Intuitively if
  // you have <span>Hello</span><span>World</span>, those are part of the same
  // LayoutBox so we should return "HelloWorld", but given
  // <div>Hello</div><div>World</div> the strings are in separate boxes so we
  // should return "Hello World".
  if (!IsInSameNonInlineBlockFlow(next->GetLayoutObject(),
                                  previous->GetLayoutObject()))
    return true;

  // Even if it is in the same inline block flow, if we are using a text
  // alternative such as an ARIA label or HTML title, we should separate
  // the strings. Doing so is consistent with what is stated in the AccName
  // spec and with what is done in other user agents.
  switch (last_used_name_from) {
    case ax::mojom::NameFrom::kNone:
    case ax::mojom::NameFrom::kUninitialized:
    case ax::mojom::NameFrom::kAttributeExplicitlyEmpty:
    case ax::mojom::NameFrom::kContents:
      break;
    case ax::mojom::NameFrom::kAttribute:
    case ax::mojom::NameFrom::kCaption:
    case ax::mojom::NameFrom::kPlaceholder:
    case ax::mojom::NameFrom::kRelatedElement:
    case ax::mojom::NameFrom::kTitle:
    case ax::mojom::NameFrom::kValue:
      return true;
  }
  switch (name_from) {
    case ax::mojom::NameFrom::kNone:
    case ax::mojom::NameFrom::kUninitialized:
    case ax::mojom::NameFrom::kAttributeExplicitlyEmpty:
    case ax::mojom::NameFrom::kContents:
      break;
    case ax::mojom::NameFrom::kAttribute:
    case ax::mojom::NameFrom::kCaption:
    case ax::mojom::NameFrom::kPlaceholder:
    case ax::mojom::NameFrom::kRelatedElement:
    case ax::mojom::NameFrom::kTitle:
    case ax::mojom::NameFrom::kValue:
      return true;
  }

  // According to the AccName spec, we need to separate controls from text nodes
  // using a space.
  return previous->IsControl() || next->IsControl();
}

String AXNodeObject::TextFromDescendants(AXObjectSet& visited,
                                         bool recursive) const {
  if (!CanHaveChildren() && recursive)
    return String();

  StringBuilder accumulated_text;
  AXObject* previous = nullptr;
  ax::mojom::NameFrom last_used_name_from = ax::mojom::NameFrom::kUninitialized;

  AXObjectVector children;

  HeapVector<Member<AXObject>> owned_children;
  ComputeAriaOwnsChildren(owned_children);

  for (Node* child = LayoutTreeBuilderTraversal::FirstChild(*node_); child;
       child = LayoutTreeBuilderTraversal::NextSibling(*child)) {
    auto* child_text_node = DynamicTo<Text>(child);
    if (child_text_node &&
        child_text_node->wholeText().ContainsOnlyWhitespaceOrEmpty()) {
      // skip over empty text nodes
      continue;
    }
    AXObject* child_obj = AXObjectCache().GetOrCreate(child);
    if (child_obj && !AXObjectCache().IsAriaOwned(child_obj))
      children.push_back(child_obj);
  }
  for (const auto& owned_child : owned_children)
    children.push_back(owned_child);

  for (AXObject* child : children) {
    constexpr size_t kMaxDescendantsForTextAlternativeComputation = 100;
    if (visited.size() > kMaxDescendantsForTextAlternativeComputation + 1)
      break;  // Need to add 1 because the root naming node is in the list.
    // If a child is a continuation, we should ignore attributes like
    // hidden and presentational. See LAYOUT TREE WALKING ALGORITHM in
    // ax_layout_object.cc for more information on continuations.
    bool is_continuation = child->GetLayoutObject() &&
                           child->GetLayoutObject()->IsElementContinuation();

    // Don't recurse into children that are explicitly marked as aria-hidden.
    // Note that we don't call isInertOrAriaHidden because that would return
    // true if any ancestor is hidden, but we need to be able to compute the
    // accessible name of object inside hidden subtrees (for example, if
    // aria-labelledby points to an object that's hidden).
    if (!is_continuation &&
        child->AOMPropertyOrARIAAttributeIsTrue(AOMBooleanProperty::kHidden))
      continue;

    ax::mojom::NameFrom child_name_from = ax::mojom::NameFrom::kUninitialized;
    String result;
    if (!is_continuation && child->IsPresentational()) {
      if (child->IsVisible())
        result = child->TextFromDescendants(visited, true);
    } else {
      result =
          RecursiveTextAlternative(*child, false, visited, child_name_from);
    }

    if (!result.IsEmpty() && previous && accumulated_text.length() &&
        !IsHTMLSpace(accumulated_text[accumulated_text.length() - 1]) &&
        !IsHTMLSpace(result[0])) {
      if (ShouldInsertSpaceBetweenObjectsIfNeeded(
              previous, child, last_used_name_from, child_name_from)) {
        accumulated_text.Append(' ');
      }
    }

    accumulated_text.Append(result);

    // We keep track of all non-hidden children, even those whose content is
    // not included, because all rendered children impact whether or not a
    // space should be inserted between objects. Example: A label which has
    // a single, nameless input surrounded by CSS-generated content should
    // have a space separating the before and after content.
    previous = child;

    // We only keep track of the source of children whose content is included.
    // Example: Three spans, the first with an aria-label, the second with no
    // content, and the third whose name comes from content. There should be a
    // space between the first and third because of the aria-label in the first.
    if (!result.IsEmpty())
      last_used_name_from = child_name_from;
  }

  return accumulated_text.ToString();
}

bool AXNodeObject::NameFromLabelElement() const {
  // This unfortunately duplicates a bit of logic from textAlternative and
  // nativeTextAlternative, but it's necessary because nameFromLabelElement
  // needs to be called from computeAccessibilityIsIgnored, which isn't allowed
  // to call axObjectCache->getOrCreate.

  if (!GetNode() && !GetLayoutObject())
    return false;

  // Step 2A from: http://www.w3.org/TR/accname-aam-1.1
  if (IsHiddenForTextAlternativeCalculation())
    return false;

  // Step 2B from: http://www.w3.org/TR/accname-aam-1.1
  // Try both spellings, but prefer aria-labelledby, which is the official spec.
  const QualifiedName& attr =
      HasAttribute(html_names::kAriaLabeledbyAttr) &&
              !HasAttribute(html_names::kAriaLabelledbyAttr)
          ? html_names::kAriaLabeledbyAttr
          : html_names::kAriaLabelledbyAttr;
  HeapVector<Member<Element>> elements_from_attribute;
  Vector<String> ids;
  ElementsFromAttribute(elements_from_attribute, attr, ids);
  if (elements_from_attribute.size() > 0)
    return false;

  // Step 2C from: http://www.w3.org/TR/accname-aam-1.1
  const AtomicString& aria_label =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kLabel);
  if (!aria_label.IsEmpty())
    return false;

  // Based on
  // http://rawgit.com/w3c/aria/master/html-aam/html-aam.html#accessible-name-and-description-calculation
  // 5.1/5.5 Text inputs, Other labelable Elements
  auto* html_element = DynamicTo<HTMLElement>(GetNode());
  if (html_element && html_element->IsLabelable()) {
    if (html_element->labels() && html_element->labels()->length() > 0)
      return true;
  }

  return false;
}

void AXNodeObject::GetRelativeBounds(AXObject** out_container,
                                     FloatRect& out_bounds_in_container,
                                     SkMatrix44& out_container_transform,
                                     bool* clips_children) const {
  if (LayoutObjectForRelativeBounds()) {
    AXObject::GetRelativeBounds(out_container, out_bounds_in_container,
                                out_container_transform, clips_children);
    return;
  }

  *out_container = nullptr;
  out_bounds_in_container = FloatRect();
  out_container_transform.setIdentity();

  // First check if it has explicit bounds, for example if this element is tied
  // to a canvas path. When explicit coordinates are provided, the ID of the
  // explicit container element that the coordinates are relative to must be
  // provided too.
  if (!explicit_element_rect_.IsEmpty()) {
    *out_container = AXObjectCache().ObjectFromAXID(explicit_container_id_);
    if (*out_container) {
      out_bounds_in_container = FloatRect(explicit_element_rect_);
      return;
    }
  }

  Element* element = GetElement();
  // If it's in a canvas but doesn't have an explicit rect, or has display:
  // contents set, get the bounding rect of its children.
  if ((GetNode()->parentElement() &&
       GetNode()->parentElement()->IsInCanvasSubtree()) ||
      (element && element->HasDisplayContentsStyle())) {
    Vector<FloatRect> rects;
    for (Node& child : NodeTraversal::ChildrenOf(*GetNode())) {
      if (child.IsHTMLElement()) {
        if (AXObject* obj = AXObjectCache().Get(&child)) {
          AXObject* container;
          FloatRect bounds;
          obj->GetRelativeBounds(&container, bounds, out_container_transform,
                                 clips_children);
          if (container) {
            *out_container = container;
            rects.push_back(bounds);
          }
        }
      }
    }

    if (*out_container) {
      out_bounds_in_container = UnionRect(rects);
      return;
    }
  }

  // If this object doesn't have an explicit element rect or computable from its
  // children, for now, let's return the position of the ancestor that does have
  // a position, and make it the width of that parent, and about the height of a
  // line of text, so that it's clear the object is a child of the parent.
  for (AXObject* position_provider = ParentObject(); position_provider;
       position_provider = position_provider->ParentObject()) {
    if (IsA<AXLayoutObject>(position_provider)) {
      position_provider->GetRelativeBounds(
          out_container, out_bounds_in_container, out_container_transform,
          clips_children);
      if (*out_container)
        out_bounds_in_container.SetSize(
            FloatSize(out_bounds_in_container.Width(),
                      std::min(10.0f, out_bounds_in_container.Height())));
      break;
    }
  }
}

static Node* GetParentNodeForComputeParent(Node* node) {
  if (!node)
    return nullptr;

  return LayoutTreeBuilderTraversal::Parent(*node);
}

AXObject* AXNodeObject::ComputeParent() const {
  DCHECK(!IsDetached());
  if (Node* parent_node = GetParentNodeForComputeParent(GetNode()))
    return AXObjectCache().GetOrCreate(parent_node);

  return nullptr;
}

AXObject* AXNodeObject::ComputeParentIfExists() const {
  if (Node* parent_node = GetParentNodeForComputeParent(GetNode()))
    return AXObjectCache().Get(parent_node);

  return nullptr;
}

AXObject* AXNodeObject::RawFirstChild() const {
  if (!GetNode())
    return nullptr;

  Node* first_child = LayoutTreeBuilderTraversal::FirstChild(*GetNode());

  if (!first_child)
    return nullptr;

  return AXObjectCache().GetOrCreate(first_child);
}

AXObject* AXNodeObject::RawNextSibling() const {
  if (!GetNode())
    return nullptr;

  Node* next_sibling = LayoutTreeBuilderTraversal::NextSibling(*GetNode());
  if (!next_sibling)
    return nullptr;

  return AXObjectCache().GetOrCreate(next_sibling);
}

void AXNodeObject::AddTableChildren() {
  if (!IsTableLikeRole() || !GetLayoutObject() || !GetLayoutObject()->IsTable())
    return;

  AXObjectCacheImpl& ax_cache = AXObjectCache();
  LayoutNGTableInterface* table =
      ToInterface<LayoutNGTableInterface>(GetLayoutObject());
  if (table)
    table->RecalcSectionsIfNeeded();
  Node* table_node = GetNode();
  if (auto* html_table_element = DynamicTo<HTMLTableElement>(table_node)) {
    if (HTMLTableCaptionElement* caption = html_table_element->caption()) {
      AXObject* caption_object = ax_cache.GetOrCreate(caption);
      if (caption_object && caption_object->AccessibilityIsIncludedInTree())
        children_.push_front(caption_object);
    }
  }
}

//
// Inline text boxes.
//

void AXNodeObject::LoadInlineTextBoxes() {
  if (!GetLayoutObject())
    return;

  if (GetLayoutObject()->IsText()) {
    ClearChildren();
    AddInlineTextBoxChildren(true);
    return;
  }

  for (const auto& child : children_) {
    child->LoadInlineTextBoxes();
  }
}

void AXNodeObject::AddInlineTextBoxChildren(bool force) {
  Document* document = GetDocument();
  if (!document)
    return;

  Settings* settings = document->GetSettings();
  if (!force &&
      (!settings || !settings->GetInlineTextBoxAccessibilityEnabled()))
    return;

  if (!GetLayoutObject() || !GetLayoutObject()->IsText())
    return;

  if (GetLayoutObject()->NeedsLayout()) {
    // If a LayoutText needs layout, its inline text boxes are either
    // nonexistent or invalid, so defer until the layout happens and
    // the layoutObject calls AXObjectCacheImpl::inlineTextBoxesUpdated.
    return;
  }

  LayoutText* layout_text = ToLayoutText(GetLayoutObject());
  for (scoped_refptr<AbstractInlineTextBox> box =
           layout_text->FirstAbstractInlineTextBox();
       box.get(); box = box->NextInlineTextBox()) {
    AXObject* ax_object = AXObjectCache().GetOrCreate(box.get());
    if (ax_object->AccessibilityIsIncludedInTree())
      children_.push_back(ax_object);
  }
}

void AXNodeObject::AddValidationMessageChild() {
  if (!IsWebArea())
    return;
  AXObject* ax_object = AXObjectCache().ValidationMessageObjectIfInvalid();
  if (ax_object)
    children_.push_back(ax_object);
}

// Hidden children are those that are not laid out or visible, but are
// specifically marked as aria-hidden=false,
// meaning that they should be exposed to the AX hierarchy.
void AXNodeObject::AddHiddenChildren() {
  Node* node = this->GetNode();
  if (!node)
    return;

  // First do a quick run through to determine if we have any hidden nodes (most
  // often we will not).  If we do have hidden nodes, we need to determine where
  // to insert them so they match DOM order as close as possible.
  bool should_insert_hidden_nodes = false;
  for (Node& child : NodeTraversal::ChildrenOf(*node)) {
    if (!child.GetLayoutObject() && IsNodeAriaVisible(&child)) {
      should_insert_hidden_nodes = true;
      break;
    }
  }

  if (!should_insert_hidden_nodes)
    return;

  // Iterate through all of the children, including those that may have already
  // been added, and try to insert hidden nodes in the correct place in the DOM
  // order.
  unsigned insertion_index = 0;
  for (Node& child : NodeTraversal::ChildrenOf(*node)) {
    if (child.GetLayoutObject()) {
      // Find out where the last layout sibling is located within children_.
      if (AXObject* child_object =
              AXObjectCache().Get(child.GetLayoutObject())) {
        if (!child_object->AccessibilityIsIncludedInTree()) {
          const auto& children = child_object->Children();
          child_object = children.size() ? children.back().Get() : nullptr;
        }
        if (child_object)
          insertion_index = children_.Find(child_object) + 1;
        continue;
      }
    }

    if (!IsNodeAriaVisible(&child))
      continue;

    unsigned previous_size = children_.size();
    if (insertion_index > previous_size)
      insertion_index = previous_size;

    InsertChild(AXObjectCache().GetOrCreate(&child), insertion_index);
    insertion_index += (children_.size() - previous_size);
  }
}

void AXNodeObject::AddImageMapChildren() {
  LayoutBoxModelObject* css_box = GetLayoutBoxModelObject();
  if (!css_box || !css_box->IsLayoutImage())
    return;

  HTMLMapElement* map = ToLayoutImage(css_box)->ImageMap();
  if (!map)
    return;

  for (HTMLAreaElement& area :
       Traversal<HTMLAreaElement>::DescendantsOf(*map)) {
    // add an <area> element for this child if it has a link
    AXObject* obj = AXObjectCache().GetOrCreate(&area);
    if (obj) {
      auto* area_object = To<AXImageMapLink>(obj);
      area_object->SetParent(this);
      DCHECK_NE(area_object->AXObjectID(), 0U);
      if (area_object->AccessibilityIsIncludedInTree())
        children_.push_back(area_object);
      else
        AXObjectCache().Remove(area_object->AXObjectID());
    }
  }
}

void AXNodeObject::AddPopupChildren() {
  auto* html_input_element = DynamicTo<HTMLInputElement>(GetNode());
  if (!html_input_element)
    return;
  if (AXObject* ax_popup = html_input_element->PopupRootAXObject())
    children_.push_back(ax_popup);
}

AXSVGRoot* AXNodeObject::RemoteSVGRootElement() const {
  // FIXME(dmazzoni): none of this code properly handled multiple references to
  // the same remote SVG document. I'm disabling this support until it can be
  // fixed properly.
  return nullptr;
}

void AXNodeObject::AddRemoteSVGChildren() {
  AXSVGRoot* root = RemoteSVGRootElement();
  if (!root)
    return;

  root->SetParent(this);

  if (!root->AccessibilityIsIncludedInTree()) {
    for (const auto& child : root->Children())
      children_.push_back(child);
  } else {
    children_.push_back(root);
  }
}

bool AXNodeObject::ShouldUseLayoutBuilderTraversal() const {
  // TODO(accessibility) Look into having one method of traversal, otherwise
  // it's possible for the same object to become a child of 2 different nodes,
  // e.g. if it has a different layout parent and DOM parent.

  // Avoid calling AXNodeObject logic for continuations.
  if (GetLayoutObject() && GetLayoutObject()->IsElementContinuation())
    return false;

  Node* node = GetNode();
  if (!node)
    return false;

  // <ruby>: special layout handling
  if (IsA<HTMLRubyElement>(*node))
    return false;

  // <table>: a thead/tfoot in the middle are bumped to the top/bottom in
  // the layout representation.
  if (IsA<HTMLTableElement>(*node))
    return false;

  // Pseudo elements often have text children that are not
  // visited by the LayoutTreeBuilderTraversal class used in DOM traversal.
  // Without this condition, list bullets would not have static text children.
  Element* element = GetElement();
  if (element && element->IsPseudoElement())
    return false;

  return true;
}

void AXNodeObject::AddChildren() {
  if (IsDetached())
    return;

  // If the need to add more children in addition to existing children arises,
  // childrenChanged should have been called, leaving the object with no
  // children.
  DCHECK(!have_children_);
  have_children_ = true;

  AXObjectVector owned_children;
  ComputeAriaOwnsChildren(owned_children);

  if (ShouldUseLayoutBuilderTraversal()) {
    for (Node* child = LayoutTreeBuilderTraversal::FirstChild(*node_); child;
         child = LayoutTreeBuilderTraversal::NextSibling(*child)) {
      if (child->IsMarkerPseudoElement() && AccessibilityIsIgnored())
        continue;
      AXObject* child_obj = AXObjectCache().GetOrCreate(child);
      if (child_obj && !AXObjectCache().IsAriaOwned(child_obj))
        AddChild(child_obj);
    }
  } else {
    for (AXObject* obj = RawFirstChild(); obj; obj = obj->RawNextSibling()) {
      if (!AXObjectCache().IsAriaOwned(obj))
        AddChild(obj);
    }
  }

  AddHiddenChildren();
  AddPopupChildren();
  AddRemoteSVGChildren();
  AddImageMapChildren();
  AddTableChildren();
  AddInlineTextBoxChildren(false);
  AddValidationMessageChild();
  AddAccessibleNodeChildren();

  for (const auto& owned_child : owned_children)
    AddChild(owned_child);

  bool is_continuation =
      GetLayoutObject() && GetLayoutObject()->IsElementContinuation();
  for (const auto& child : children_) {
    if (!is_continuation && !child->CachedParentObject()) {
      // Never set continuations as a parent object. The first layout object
      // in the chain must be used instead.
      child->SetParent(this);
    }
  }
}

void AXNodeObject::AddChild(AXObject* child) {
  unsigned index = children_.size();
  InsertChild(child, index);
}

void AXNodeObject::InsertChild(AXObject* child, unsigned index) {
  if (!child || !CanHaveChildren())
    return;

  // If the parent is asking for this child's children, then either it's the
  // first time (and clearing is a no-op), or its visibility has changed. In the
  // latter case, this child may have a stale child cached.  This can prevent
  // aria-hidden changes from working correctly. Hence, whenever a parent is
  // getting children, ensure data is not stale.
  child->ClearChildren();

  if (!child->AccessibilityIsIncludedInTree()) {
    const auto& children = child->Children();
    wtf_size_t length = children.size();
    for (wtf_size_t i = 0; i < length; ++i)
      children_.insert(index + i, children[i]);
  } else if (!child->IsMenuListOption()) {
    // MenuListOptions must only added in AXMenuListPopup::AddChildren
    children_.insert(index, child);
  }
}

void AXNodeObject::ClearChildren() {
  AXObject::ClearChildren();
  children_dirty_ = false;
}

bool AXNodeObject::CanHaveChildren() const {
  // If this is an AXLayoutObject, then it's okay if this object
  // doesn't have a node - there are some layoutObjects that don't have
  // associated nodes, like scroll areas and css-generated text.
  if (!GetNode() && !IsAXLayoutObject())
    return false;

  if (GetNode() && IsA<HTMLMapElement>(GetNode()))
    return false;  // Does not have a role, so check here

  switch (native_role_) {
    case ax::mojom::Role::kCheckBox:
    case ax::mojom::Role::kImage:
    case ax::mojom::Role::kListBoxOption:
    case ax::mojom::Role::kMenuButton:
    case ax::mojom::Role::kMenuListOption:
    case ax::mojom::Role::kMenuItem:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kMenuItemRadio:
    case ax::mojom::Role::kProgressIndicator:
    case ax::mojom::Role::kRadioButton:
    case ax::mojom::Role::kScrollBar:
    // case ax::mojom::Role::kSearchBox:
    case ax::mojom::Role::kSlider:
    case ax::mojom::Role::kSplitter:
    case ax::mojom::Role::kSwitch:
    case ax::mojom::Role::kTab:
    // case ax::mojom::Role::kTextField:
    case ax::mojom::Role::kToggleButton:
      return false;
    case ax::mojom::Role::kPopUpButton:
      return true;
    case ax::mojom::Role::kStaticText:
      return AXObjectCache().InlineTextBoxAccessibilityEnabled();
    default:
      break;
  }

  switch (AriaRoleAttribute()) {
    case ax::mojom::Role::kImage:
      return false;
    case ax::mojom::Role::kCheckBox:
    case ax::mojom::Role::kListBoxOption:
    case ax::mojom::Role::kMath:  // role="math" is flat, unlike <math>
    case ax::mojom::Role::kMenuButton:
    case ax::mojom::Role::kMenuListOption:
    case ax::mojom::Role::kMenuItem:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kMenuItemRadio:
    case ax::mojom::Role::kPopUpButton:
    case ax::mojom::Role::kProgressIndicator:
    case ax::mojom::Role::kRadioButton:
    case ax::mojom::Role::kScrollBar:
    case ax::mojom::Role::kSlider:
    case ax::mojom::Role::kSplitter:
    case ax::mojom::Role::kSwitch:
    case ax::mojom::Role::kTab:
    case ax::mojom::Role::kToggleButton: {
      // These roles have ChildrenPresentational: true in the ARIA spec.
      // We used to remove/prune all descendants of them, but that removed
      // useful content if the author didn't follow the spec perfectly, for
      // example if they wanted a complex radio button with a textfield child.
      // We are now only pruning these if there is a single text child,
      // otherwise the subtree is exposed. The ChildrenPresentational rule
      // is thus useful for authoring/verification tools but does not break
      // complex widget implementations.
      Element* element = GetElement();
      return element && !element->HasOneTextChild();
    }
    default:
      break;
  }
  return true;
}

Element* AXNodeObject::ActionElement() const {
  Node* node = this->GetNode();
  if (!node)
    return nullptr;

  auto* element = DynamicTo<Element>(node);
  if (element && IsClickable())
    return element;

  Element* anchor = AnchorElement();
  Element* click_element = MouseButtonListener();
  if (!anchor || (click_element && click_element->IsDescendantOf(anchor)))
    return click_element;
  return anchor;
}

Element* AXNodeObject::AnchorElement() const {
  Node* node = this->GetNode();
  if (!node)
    return nullptr;

  AXObjectCacheImpl& cache = AXObjectCache();

  // search up the DOM tree for an anchor element
  // NOTE: this assumes that any non-image with an anchor is an
  // HTMLAnchorElement
  for (; node; node = node->parentNode()) {
    if (IsA<HTMLAnchorElement>(*node))
      return To<Element>(node);

    if (LayoutObject* layout_object = node->GetLayoutObject()) {
      AXObject* ax_object = cache.GetOrCreate(layout_object);
      if (ax_object && ax_object->IsAnchor())
        return To<Element>(node);
    }
  }

  return nullptr;
}

Document* AXNodeObject::GetDocument() const {
  if (!GetNode())
    return nullptr;
  return &GetNode()->GetDocument();
}

void AXNodeObject::SetNode(Node* node) {
  node_ = node;
}

AXObject* AXNodeObject::CorrespondingControlAXObjectForLabelElement() const {
  HTMLLabelElement* label_element = LabelElementContainer();
  if (!label_element)
    return nullptr;

  HTMLElement* corresponding_control = label_element->control();
  if (!corresponding_control)
    return nullptr;

  // Make sure the corresponding control isn't a descendant of this label
  // that's in the middle of being destroyed.
  if (corresponding_control->GetLayoutObject() &&
      !corresponding_control->GetLayoutObject()->Parent())
    return nullptr;

  return AXObjectCache().GetOrCreate(corresponding_control);
}

AXObject* AXNodeObject::CorrespondingLabelAXObject() const {
  HTMLLabelElement* label_element = LabelElementContainer();
  if (!label_element)
    return nullptr;

  return AXObjectCache().GetOrCreate(label_element);
}

HTMLLabelElement* AXNodeObject::LabelElementContainer() const {
  if (!GetNode())
    return nullptr;

  // the control element should not be considered part of the label
  if (IsControl())
    return nullptr;

  // the link element should not be considered part of the label
  if (IsLink())
    return nullptr;

  // find if this has a ancestor that is a label
  return Traversal<HTMLLabelElement>::FirstAncestorOrSelf(*GetNode());
}

bool AXNodeObject::OnNativeFocusAction() {
  // Checking if node is focusable in a native focus action requires that we
  // have updated style and layout tree, since the focus check relies on the
  // existence of layout objects to determine the result. However, these layout
  // objects may have been deferred by display-locking.
  Document* document = GetDocument();
  Node* node = GetNode();
  if (document && node)
    document->UpdateStyleAndLayoutTreeForNode(node);

  if (!CanSetFocusAttribute())
    return false;

  if (IsWebArea()) {
    // If another Frame has focused content (e.g. nested iframe), then we
    // need to clear focus for the other Document Frame.
    // Here we set the focused element via the FocusController so that the
    // other Frame loses focus, and the target Document Element gains focus.
    // This fixes a scenario with Narrator Item Navigation when the user
    // navigates from the outer UI to the document when the last focused
    // element was within a nested iframe before leaving the document frame.
    Page* page = document->GetPage();
    // Elements inside a portal should not be focusable.
    if (page && !page->InsidePortal()) {
      page->GetFocusController().SetFocusedElement(document->documentElement(),
                                                   document->GetFrame());
    } else {
      document->ClearFocusedElement();
    }
    return true;
  }

  Element* element = GetElement();
  if (!element) {
    document->ClearFocusedElement();
    return true;
  }

  // If this node is already the currently focused node, then calling
  // focus() won't do anything.  That is a problem when focus is removed
  // from the webpage to chrome, and then returns.  In these cases, we need
  // to do what keyboard and mouse focus do, which is reset focus first.
  if (document->FocusedElement() == element)
    document->ClearFocusedElement();

  // If the object is not natively focusable but can be focused using an ARIA
  // active descendant, perform a native click instead. This will enable Web
  // apps that set accessibility focus using an active descendant to capture and
  // act on the click event. Otherwise, there is no other way to inform the app
  // that an AT has requested the focus to be changed, except if the app is
  // using AOM. To be extra safe, exclude objects that are clickable themselves.
  // This won't prevent anyone from having a click handler on the object's
  // container.
  if (!IsClickable() && CanBeActiveDescendant()) {
    return OnNativeClickAction();
  }

  element->focus();
  return true;
}

bool AXNodeObject::OnNativeIncrementAction() {
  LocalFrame* frame = GetDocument() ? GetDocument()->GetFrame() : nullptr;
  LocalFrame::NotifyUserActivation(frame);
  AlterSliderOrSpinButtonValue(true);
  return true;
}

bool AXNodeObject::OnNativeDecrementAction() {
  LocalFrame* frame = GetDocument() ? GetDocument()->GetFrame() : nullptr;
  LocalFrame::NotifyUserActivation(frame);
  AlterSliderOrSpinButtonValue(false);
  return true;
}

bool AXNodeObject::OnNativeSetSequentialFocusNavigationStartingPointAction() {
  if (!GetNode())
    return false;

  Document* document = GetDocument();
  document->ClearFocusedElement();
  document->SetSequentialFocusNavigationStartingPoint(GetNode());
  return true;
}

void AXNodeObject::ChildrenChanged() {
  if (!GetNode() && !GetLayoutObject())
    return;

  // Call SetNeedsToUpdateChildren on this node, and if this node is
  // ignored, call it on each existing parent until reaching an unignored node,
  // because unignored nodes recursively include all children of ignored
  // nodes. This method is called during layout, so we need to be careful to
  // only explore existing objects.
  AXObject* node_to_update = this;
  while (node_to_update) {
    node_to_update->SetNeedsToUpdateChildren();
    if (!node_to_update->LastKnownIsIgnoredValue())
      break;
    node_to_update = node_to_update->ParentObjectIfExists();
  }

  // If this node's children are not part of the accessibility tree then
  // skip notification and walking up the ancestors.
  // Cases where this happens:
  // - an ancestor has only presentational children, or
  // - this or an ancestor is a leaf node
  // Uses |cached_is_descendant_of_leaf_node_| to avoid updating cached
  // attributes for eachc change via | UpdateCachedAttributeValuesIfNeeded()|.
  if (!CanHaveChildren() || cached_is_descendant_of_leaf_node_)
    return;

  // Calling CanHaveChildren(), above, can occasionally detach |this|.
  if (IsDetached())
    return;

  AXObjectCache().PostNotification(this, ax::mojom::Event::kChildrenChanged);

  // Go up the accessibility parent chain, but only if the element already
  // exists. This method is called during layout, minimal work should be done.
  // If AX elements are created now, they could interrogate the layout tree
  // while it's in a funky state.  At the same time, process ARIA live region
  // changes.
  for (AXObject* parent = this; parent;
       parent = parent->ParentObjectIfExists()) {
    // These notifications always need to be sent because screenreaders are
    // reliant on them to perform.  In other words, they need to be sent even
    // when the screen reader has not accessed this live region since the last
    // update.

    // If this element supports ARIA live regions, then notify the AT of
    // changes. Do not fire live region changed events if aria-live="off".
    if (parent->IsLiveRegionRoot()) {
      if (parent->IsActiveLiveRegionRoot()) {
        AXObjectCache().PostNotification(parent,
                                         ax::mojom::Event::kLiveRegionChanged);
      }
      break;
    }
  }

  for (AXObject* parent = this; parent;
       parent = parent->ParentObjectIfExists()) {
    // If this element is an ARIA text box or content editable, post a "value
    // changed" notification on it so that it behaves just like a native input
    // element or textarea.
    if (IsNonNativeTextControl()) {
      AXObjectCache().PostNotification(parent, ax::mojom::Event::kValueChanged);
      break;
    }
  }
}

void AXNodeObject::UpdateChildrenIfNecessary() {
  if (NeedsToUpdateChildren())
    ClearChildren();

  AXObject::UpdateChildrenIfNecessary();
}

void AXNodeObject::SelectedOptions(AXObjectVector& options) const {
  if (auto* select = DynamicTo<HTMLSelectElement>(GetNode())) {
    for (auto* const option : *select->selectedOptions()) {
      AXObject* ax_option = AXObjectCache().GetOrCreate(option);
      if (ax_option)
        options.push_back(ax_option);
    }
    return;
  }

  // If the combobox or listbox is a descendant of a label element for another
  // widget, it may be ignored and Children() won't return all its children.
  // As a result, we need to use RawFirstChild and RawNextSibling to iterate
  // over the children in search of the selected option(s).

  if (RoleValue() == ax::mojom::Role::kComboBoxGrouping ||
      RoleValue() == ax::mojom::Role::kComboBoxMenuButton) {
    for (AXObject* obj = RawFirstChild(); obj; obj = obj->RawNextSibling()) {
      if (obj->RoleValue() == ax::mojom::Role::kListBox) {
        obj->SelectedOptions(options);
        return;
      }
    }
  }

  for (AXObject* obj = RawFirstChild(); obj; obj = obj->RawNextSibling()) {
    if (obj->IsSelected() == kSelectedStateTrue)
      options.push_back(obj);
  }
}

void AXNodeObject::SelectionChanged() {
  // Post the selected text changed event on the first ancestor that's
  // focused (to handle form controls, ARIA text boxes and contentEditable),
  // or the web area if the selection is just in the document somewhere.
  if (IsFocused() || IsWebArea()) {
    AXObjectCache().PostNotification(this,
                                     ax::mojom::Event::kTextSelectionChanged);
    if (GetDocument()) {
      AXObject* document_object = AXObjectCache().GetOrCreate(GetDocument());
      AXObjectCache().PostNotification(
          document_object, ax::mojom::Event::kDocumentSelectionChanged);
    }
  } else {
    AXObject::SelectionChanged();  // Calls selectionChanged on parent.
  }
}

void AXNodeObject::TextChanged() {
  // If this element supports ARIA live regions, or is part of a region with an
  // ARIA editable role, then notify the AT of changes.
  AXObjectCacheImpl& cache = AXObjectCache();
  for (Node* parent_node = GetNode(); parent_node;
       parent_node = parent_node->parentNode()) {
    AXObject* parent = cache.Get(parent_node);
    if (!parent)
      continue;

    if (parent->IsLiveRegionRoot()) {
      if (parent->IsActiveLiveRegionRoot()) {
        cache.PostNotification(parent_node,
                               ax::mojom::Event::kLiveRegionChanged);
      }
      break;
    }
  }

  // If this element is an ARIA text box or content editable, post a "value
  // changed" notification on it so that it behaves just like a native input
  // element or textarea.
  for (Node* parent_node = GetNode(); parent_node;
       parent_node = parent_node->parentNode()) {
    AXObject* parent = cache.Get(parent_node);
    if (!parent)
      continue;
    if (parent->IsNonNativeTextControl()) {
      cache.PostNotification(parent_node, ax::mojom::Event::kValueChanged);
      break;
    }
  }
}

void AXNodeObject::ComputeAriaOwnsChildren(
    HeapVector<Member<AXObject>>& owned_children) const {
  Vector<String> id_vector;
  // Case 1: owned children not allowed
  if (!CanHaveChildren() || IsNativeTextControl() ||
      HasContentEditableAttributeSet()) {
    if (GetNode())
      AXObjectCache().UpdateAriaOwns(this, id_vector, owned_children);
    return;
  }

  // We first check if the element has an explicitly set aria-owns association.
  // Explicitly set elements are validated on setting time (that they are in a
  // valid scope etc). The content attribute can contain ids that are not
  // legally ownable.
  Element* element = GetElement();
  if (element && element->HasExplicitlySetAttrAssociatedElements(
                     html_names::kAriaOwnsAttr)) {
    AXObjectCache().UpdateAriaOwnsFromAttrAssociatedElements(
        this,
        element->GetElementArrayAttribute(html_names::kAriaOwnsAttr).value(),
        owned_children);
    return;
  }

  // Case 2: aria-owns attribute
  TokenVectorFromAttribute(id_vector, html_names::kAriaOwnsAttr);
  AXObjectCache().UpdateAriaOwns(this, id_vector, owned_children);
}

// According to the standard, the figcaption should only be the first or
// last child: https://html.spec.whatwg.org/#the-figcaption-element
static Element* GetChildFigcaption(const Node& node) {
  Element* element = ElementTraversal::FirstChild(node);
  if (!element)
    return nullptr;
  if (element->HasTagName(html_names::kFigcaptionTag))
    return element;

  element = ElementTraversal::LastChild(node);
  if (element->HasTagName(html_names::kFigcaptionTag))
    return element;

  return nullptr;
}

// Based on
// http://rawgit.com/w3c/aria/master/html-aam/html-aam.html#accessible-name-and-description-calculation
String AXNodeObject::NativeTextAlternative(
    AXObjectSet& visited,
    ax::mojom::NameFrom& name_from,
    AXRelatedObjectVector* related_objects,
    NameSources* name_sources,
    bool* found_text_alternative) const {
  if (!GetNode())
    return String();

  // If nameSources is non-null, relatedObjects is used in filling it in, so it
  // must be non-null as well.
  if (name_sources)
    DCHECK(related_objects);

  String text_alternative;
  AXRelatedObjectVector local_related_objects;

  const auto* input_element = DynamicTo<HTMLInputElement>(GetNode());

  // 5.1/5.5 Text inputs, Other labelable Elements
  // If you change this logic, update AXNodeObject::nameFromLabelElement, too.
  auto* html_element = DynamicTo<HTMLElement>(GetNode());
  if (html_element && html_element->IsLabelable()) {
    name_from = ax::mojom::NameFrom::kRelatedElement;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative));
      name_sources->back().type = name_from;
      name_sources->back().native_source = kAXTextFromNativeHTMLLabel;
    }

    LabelsNodeList* labels = nullptr;
    if (AXObjectCache().MayHaveHTMLLabel(*html_element))
      labels = html_element->labels();
    if (labels && labels->length() > 0) {
      HeapVector<Member<Element>> label_elements;
      for (unsigned label_index = 0; label_index < labels->length();
           ++label_index) {
        Element* label = labels->item(label_index);
        if (name_sources) {
          if (!label->FastGetAttribute(html_names::kForAttr).IsEmpty() &&
              label->FastGetAttribute(html_names::kForAttr) ==
                  html_element->GetIdAttribute()) {
            name_sources->back().native_source = kAXTextFromNativeHTMLLabelFor;
          } else {
            name_sources->back().native_source =
                kAXTextFromNativeHTMLLabelWrapped;
          }
        }
        label_elements.push_back(label);
      }

      text_alternative =
          TextFromElements(false, visited, label_elements, related_objects);
      if (!text_alternative.IsNull()) {
        *found_text_alternative = true;
        if (name_sources) {
          NameSource& source = name_sources->back();
          source.related_objects = *related_objects;
          source.text = text_alternative;
        } else {
          return text_alternative.StripWhiteSpace();
        }
      } else if (name_sources) {
        name_sources->back().invalid = true;
      }
    }
  }

  // 5.2 input type="button", input type="submit" and input type="reset"
  if (input_element && input_element->IsTextButton()) {
    // value attribute.
    name_from = ax::mojom::NameFrom::kValue;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative, kValueAttr));
      name_sources->back().type = name_from;
    }
    String value = input_element->value();
    if (!value.IsNull()) {
      text_alternative = value;
      if (name_sources) {
        NameSource& source = name_sources->back();
        source.text = text_alternative;
        *found_text_alternative = true;
      } else {
        return text_alternative;
      }
    }

    // Get default value if object is not laid out.
    // If object is laid out, it will have a layout object for the label.
    if (!GetLayoutObject()) {
      String default_label = input_element->ValueOrDefaultLabel();
      if (value.IsNull() && !default_label.IsNull()) {
        // default label
        name_from = ax::mojom::NameFrom::kContents;
        if (name_sources) {
          name_sources->push_back(NameSource(*found_text_alternative));
          name_sources->back().type = name_from;
        }
        text_alternative = default_label;
        if (name_sources) {
          NameSource& source = name_sources->back();
          source.text = text_alternative;
          *found_text_alternative = true;
        } else {
          return text_alternative;
        }
      }
    }
    return text_alternative;
  }

  // 5.3 input type="image"
  if (input_element &&
      input_element->getAttribute(kTypeAttr) == input_type_names::kImage) {
    // alt attr
    const AtomicString& alt = input_element->getAttribute(kAltAttr);
    const bool is_empty = alt.IsEmpty() && !alt.IsNull();
    name_from = is_empty ? ax::mojom::NameFrom::kAttributeExplicitlyEmpty
                         : ax::mojom::NameFrom::kAttribute;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative, kAltAttr));
      name_sources->back().type = name_from;
    }
    if (!alt.IsNull()) {
      text_alternative = alt;
      if (name_sources) {
        NameSource& source = name_sources->back();
        source.attribute_value = alt;
        source.text = text_alternative;
        *found_text_alternative = true;
      } else {
        return text_alternative;
      }
    }

    // value attribute.
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative, kValueAttr));
      name_sources->back().type = name_from;
    }
    name_from = ax::mojom::NameFrom::kAttribute;
    String value = input_element->value();
    if (!value.IsNull()) {
      text_alternative = value;
      if (name_sources) {
        NameSource& source = name_sources->back();
        source.text = text_alternative;
        *found_text_alternative = true;
      } else {
        return text_alternative;
      }
    }

    // title attr
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative, kTitleAttr));
      name_sources->back().type = name_from;
    }
    name_from = ax::mojom::NameFrom::kTitle;
    const AtomicString& title = input_element->getAttribute(kTitleAttr);
    if (!title.IsNull()) {
      text_alternative = title;
      if (name_sources) {
        NameSource& source = name_sources->back();
        source.attribute_value = title;
        source.text = text_alternative;
        *found_text_alternative = true;
      } else {
        return text_alternative;
      }
    }

    // localised default value ("Submit")
    name_from = ax::mojom::NameFrom::kValue;
    text_alternative =
        input_element->GetLocale().QueryString(IDS_FORM_SUBMIT_LABEL);
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative, kTypeAttr));
      NameSource& source = name_sources->back();
      source.attribute_value = input_element->getAttribute(kTypeAttr);
      source.type = name_from;
      source.text = text_alternative;
      *found_text_alternative = true;
    } else {
      return text_alternative;
    }
    return text_alternative;
  }

  // 5.1 Text inputs - step 3 (placeholder attribute)
  if (html_element && html_element->IsTextControl()) {
    name_from = ax::mojom::NameFrom::kPlaceholder;
    if (name_sources) {
      name_sources->push_back(
          NameSource(*found_text_alternative, html_names::kPlaceholderAttr));
      NameSource& source = name_sources->back();
      source.type = name_from;
    }
    const String placeholder = PlaceholderFromNativeAttribute();
    if (!placeholder.IsEmpty()) {
      text_alternative = placeholder;
      if (name_sources) {
        NameSource& source = name_sources->back();
        source.text = text_alternative;
        source.attribute_value =
            html_element->FastGetAttribute(html_names::kPlaceholderAttr);
        *found_text_alternative = true;
      } else {
        return text_alternative;
      }
    }
  }

  // Also check for aria-placeholder.
  if (IsTextControl()) {
    name_from = ax::mojom::NameFrom::kPlaceholder;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative,
                                         html_names::kAriaPlaceholderAttr));
      NameSource& source = name_sources->back();
      source.type = name_from;
    }
    const AtomicString& aria_placeholder =
        GetAOMPropertyOrARIAAttribute(AOMStringProperty::kPlaceholder);
    if (!aria_placeholder.IsEmpty()) {
      text_alternative = aria_placeholder;
      if (name_sources) {
        NameSource& source = name_sources->back();
        source.text = text_alternative;
        source.attribute_value = aria_placeholder;
        *found_text_alternative = true;
      } else {
        return text_alternative;
      }
    }

    return text_alternative;
  }

  // 5.7 figure and figcaption Elements
  if (GetNode()->HasTagName(html_names::kFigureTag)) {
    // figcaption
    name_from = ax::mojom::NameFrom::kRelatedElement;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative));
      name_sources->back().type = name_from;
      name_sources->back().native_source = kAXTextFromNativeHTMLFigcaption;
    }
    Element* figcaption = GetChildFigcaption(*(GetNode()));
    if (figcaption) {
      AXObject* figcaption_ax_object = AXObjectCache().GetOrCreate(figcaption);
      if (figcaption_ax_object) {
        text_alternative =
            RecursiveTextAlternative(*figcaption_ax_object, false, visited);

        if (related_objects) {
          local_related_objects.push_back(
              MakeGarbageCollected<NameSourceRelatedObject>(
                  figcaption_ax_object, text_alternative));
          *related_objects = local_related_objects;
          local_related_objects.clear();
        }

        if (name_sources) {
          NameSource& source = name_sources->back();
          source.related_objects = *related_objects;
          source.text = text_alternative;
          *found_text_alternative = true;
        } else {
          return text_alternative;
        }
      }
    }
    return text_alternative;
  }

  // 5.8 img or area Element
  if (IsA<HTMLImageElement>(GetNode()) || IsA<HTMLAreaElement>(GetNode()) ||
      (GetLayoutObject() && GetLayoutObject()->IsSVGImage())) {
    // alt
    const AtomicString& alt = GetAttribute(kAltAttr);
    const bool is_empty = alt.IsEmpty() && !alt.IsNull();
    name_from = is_empty ? ax::mojom::NameFrom::kAttributeExplicitlyEmpty
                         : ax::mojom::NameFrom::kAttribute;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative, kAltAttr));
      name_sources->back().type = name_from;
    }
    if (!alt.IsNull()) {
      text_alternative = alt;
      if (name_sources) {
        NameSource& source = name_sources->back();
        source.attribute_value = alt;
        source.text = text_alternative;
        *found_text_alternative = true;
      } else {
        return text_alternative;
      }
    }
    return text_alternative;
  }

  // 5.9 table Element
  if (auto* table_element = DynamicTo<HTMLTableElement>(GetNode())) {
    // caption
    name_from = ax::mojom::NameFrom::kCaption;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative));
      name_sources->back().type = name_from;
      name_sources->back().native_source = kAXTextFromNativeHTMLTableCaption;
    }
    HTMLTableCaptionElement* caption = table_element->caption();
    if (caption) {
      AXObject* caption_ax_object = AXObjectCache().GetOrCreate(caption);
      if (caption_ax_object) {
        text_alternative =
            RecursiveTextAlternative(*caption_ax_object, false, visited);
        if (related_objects) {
          local_related_objects.push_back(
              MakeGarbageCollected<NameSourceRelatedObject>(caption_ax_object,
                                                            text_alternative));
          *related_objects = local_related_objects;
          local_related_objects.clear();
        }

        if (name_sources) {
          NameSource& source = name_sources->back();
          source.related_objects = *related_objects;
          source.text = text_alternative;
          *found_text_alternative = true;
        } else {
          return text_alternative;
        }
      }
    }

    // summary
    name_from = ax::mojom::NameFrom::kAttribute;
    if (name_sources) {
      name_sources->push_back(
          NameSource(*found_text_alternative, html_names::kSummaryAttr));
      name_sources->back().type = name_from;
    }
    const AtomicString& summary = GetAttribute(html_names::kSummaryAttr);
    if (!summary.IsNull()) {
      text_alternative = summary;
      if (name_sources) {
        NameSource& source = name_sources->back();
        source.attribute_value = summary;
        source.text = text_alternative;
        *found_text_alternative = true;
      } else {
        return text_alternative;
      }
    }

    return text_alternative;
  }

  // Per SVG AAM 1.0's modifications to 2D of this algorithm.
  if (GetNode()->IsSVGElement()) {
    name_from = ax::mojom::NameFrom::kRelatedElement;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative));
      name_sources->back().type = name_from;
      name_sources->back().native_source = kAXTextFromNativeHTMLTitleElement;
    }
    auto* container_node = To<ContainerNode>(GetNode());
    Element* title = ElementTraversal::FirstChild(
        *container_node, HasTagName(svg_names::kTitleTag));

    if (title) {
      AXObject* title_ax_object = AXObjectCache().GetOrCreate(title);
      if (title_ax_object && !visited.Contains(title_ax_object)) {
        text_alternative =
            RecursiveTextAlternative(*title_ax_object, false, visited);
        if (related_objects) {
          local_related_objects.push_back(
              MakeGarbageCollected<NameSourceRelatedObject>(title_ax_object,
                                                            text_alternative));
          *related_objects = local_related_objects;
          local_related_objects.clear();
        }
      }
      if (name_sources) {
        NameSource& source = name_sources->back();
        source.text = text_alternative;
        source.related_objects = *related_objects;
        *found_text_alternative = true;
      } else {
        return text_alternative;
      }
    }
  }

  // Fieldset / legend.
  if (auto* html_field_set_element =
          DynamicTo<HTMLFieldSetElement>(GetNode())) {
    name_from = ax::mojom::NameFrom::kRelatedElement;
    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative));
      name_sources->back().type = name_from;
      name_sources->back().native_source = kAXTextFromNativeHTMLLegend;
    }
    HTMLElement* legend = html_field_set_element->Legend();
    if (legend) {
      AXObject* legend_ax_object = AXObjectCache().GetOrCreate(legend);
      // Avoid an infinite loop
      if (legend_ax_object && !visited.Contains(legend_ax_object)) {
        text_alternative =
            RecursiveTextAlternative(*legend_ax_object, false, visited);

        if (related_objects) {
          local_related_objects.push_back(
              MakeGarbageCollected<NameSourceRelatedObject>(legend_ax_object,
                                                            text_alternative));
          *related_objects = local_related_objects;
          local_related_objects.clear();
        }

        if (name_sources) {
          NameSource& source = name_sources->back();
          source.related_objects = *related_objects;
          source.text = text_alternative;
          *found_text_alternative = true;
        } else {
          return text_alternative;
        }
      }
    }
  }

  // Document.
  if (IsWebArea()) {
    Document* document = this->GetDocument();
    if (document) {
      name_from = ax::mojom::NameFrom::kAttribute;
      if (name_sources) {
        name_sources->push_back(
            NameSource(found_text_alternative, html_names::kAriaLabelAttr));
        name_sources->back().type = name_from;
      }
      if (Element* document_element = document->documentElement()) {
        const AtomicString& aria_label =
            AccessibleNode::GetPropertyOrARIAAttribute(
                document_element, AOMStringProperty::kLabel);
        if (!aria_label.IsEmpty()) {
          text_alternative = aria_label;

          if (name_sources) {
            NameSource& source = name_sources->back();
            source.text = text_alternative;
            source.attribute_value = aria_label;
            *found_text_alternative = true;
          } else {
            return text_alternative;
          }
        }
      }

      name_from = ax::mojom::NameFrom::kRelatedElement;
      if (name_sources) {
        name_sources->push_back(NameSource(*found_text_alternative));
        name_sources->back().type = name_from;
        name_sources->back().native_source = kAXTextFromNativeHTMLTitleElement;
      }

      text_alternative = document->title();

      Element* title_element = document->TitleElement();
      AXObject* title_ax_object = AXObjectCache().GetOrCreate(title_element);
      if (title_ax_object) {
        if (related_objects) {
          local_related_objects.push_back(
              MakeGarbageCollected<NameSourceRelatedObject>(title_ax_object,
                                                            text_alternative));
          *related_objects = local_related_objects;
          local_related_objects.clear();
        }

        if (name_sources) {
          NameSource& source = name_sources->back();
          source.related_objects = *related_objects;
          source.text = text_alternative;
          *found_text_alternative = true;
        } else {
          return text_alternative;
        }
      }
    }
  }

  return text_alternative;
}

String AXNodeObject::Description(ax::mojom::NameFrom name_from,
                                 ax::mojom::DescriptionFrom& description_from,
                                 AXObjectVector* description_objects) const {
  AXRelatedObjectVector related_objects;
  String result =
      Description(name_from, description_from, nullptr, &related_objects);
  if (description_objects) {
    description_objects->clear();
    for (NameSourceRelatedObject* related_object : related_objects)
      description_objects->push_back(related_object->object);
  }

  result = CollapseWhitespace(result);

  if (RoleValue() == ax::mojom::Role::kSpinButton && DatetimeAncestor()) {
    // Fields inside a datetime control need to merge the field description
    // with the description of the <input> element.
    const AXObject* datetime_ancestor = DatetimeAncestor();
    ax::mojom::NameFrom datetime_ancestor_name_from;
    datetime_ancestor->GetName(datetime_ancestor_name_from, nullptr);
    description_objects->clear();
    String ancestor_description = DatetimeAncestor()->Description(
        datetime_ancestor_name_from, description_from, description_objects);
    if (!result.IsEmpty() && !ancestor_description.IsEmpty())
      return result + " " + ancestor_description;
    if (!ancestor_description.IsEmpty())
      return ancestor_description;
  }

  return result;
}

// Based on
// http://rawgit.com/w3c/aria/master/html-aam/html-aam.html#accessible-name-and-description-calculation
String AXNodeObject::Description(ax::mojom::NameFrom name_from,
                                 ax::mojom::DescriptionFrom& description_from,
                                 DescriptionSources* description_sources,
                                 AXRelatedObjectVector* related_objects) const {
  // If descriptionSources is non-null, relatedObjects is used in filling it in,
  // so it must be non-null as well.
  if (description_sources)
    DCHECK(related_objects);

  if (!GetNode())
    return String();

  String description;
  bool found_description = false;

  description_from = ax::mojom::DescriptionFrom::kRelatedElement;
  if (description_sources) {
    description_sources->push_back(
        DescriptionSource(found_description, html_names::kAriaDescribedbyAttr));
    description_sources->back().type = description_from;
  }

  // aria-describedby overrides any other accessible description, from:
  // http://rawgit.com/w3c/aria/master/html-aam/html-aam.html
  Element* element = GetElement();
  if (!element)
    return String();

  Vector<String> ids;
  HeapVector<Member<Element>> elements_from_attribute;
  ElementsFromAttribute(elements_from_attribute,
                        html_names::kAriaDescribedbyAttr, ids);
  if (!elements_from_attribute.IsEmpty()) {
    // TODO(meredithl): Determine description sources when |aria_describedby| is
    // the empty string, in order to make devtools work with attr-associated
    // elements.
    if (description_sources) {
      description_sources->back().attribute_value =
          GetAttribute(html_names::kAriaDescribedbyAttr);
    }
    AXObjectSet visited;
    description = TextFromElements(true, visited, elements_from_attribute,
                                   related_objects);

    for (auto& element : elements_from_attribute)
      ids.push_back(element->GetIdAttribute());

    TokenVectorFromAttribute(ids, html_names::kAriaDescribedbyAttr);
    AXObjectCache().UpdateReverseRelations(this, ids);

    if (!description.IsNull()) {
      if (description_sources) {
        DescriptionSource& source = description_sources->back();
        source.type = description_from;
        source.related_objects = *related_objects;
        source.text = description;
        found_description = true;
      } else {
        return description;
      }
    } else if (description_sources) {
      description_sources->back().invalid = true;
    }
  }

  // aria-description overrides any HTML-based accessible description,
  // but not aria-describedby.
  if (RuntimeEnabledFeatures::AccessibilityExposeARIAAnnotationsEnabled(
          GetDocument())) {
    const AtomicString& aria_desc =
        GetAOMPropertyOrARIAAttribute(AOMStringProperty::kDescription);
    if (!aria_desc.IsNull()) {
      description_from = ax::mojom::DescriptionFrom::kAttribute;
      description = aria_desc;
      if (description_sources) {
        found_description = true;
        description_sources->back().text = description;
      } else {
        return description;
      }
    }
  }

  const auto* input_element = DynamicTo<HTMLInputElement>(GetNode());

  // value, 5.2.2 from: http://rawgit.com/w3c/aria/master/html-aam/html-aam.html
  if (name_from != ax::mojom::NameFrom::kValue && input_element &&
      input_element->IsTextButton()) {
    description_from = ax::mojom::DescriptionFrom::kAttribute;
    if (description_sources) {
      description_sources->push_back(
          DescriptionSource(found_description, kValueAttr));
      description_sources->back().type = description_from;
    }
    String value = input_element->value();
    if (!value.IsNull()) {
      description = value;
      if (description_sources) {
        DescriptionSource& source = description_sources->back();
        source.text = description;
        found_description = true;
      } else {
        return description;
      }
    }
  }

  // table caption, 5.9.2 from:
  // http://rawgit.com/w3c/aria/master/html-aam/html-aam.html
  auto* table_element = DynamicTo<HTMLTableElement>(element);
  if (name_from != ax::mojom::NameFrom::kCaption && table_element) {
    description_from = ax::mojom::DescriptionFrom::kRelatedElement;
    if (description_sources) {
      description_sources->push_back(DescriptionSource(found_description));
      description_sources->back().type = description_from;
      description_sources->back().native_source =
          kAXTextFromNativeHTMLTableCaption;
    }
    HTMLTableCaptionElement* caption = table_element->caption();
    if (caption) {
      AXObject* caption_ax_object = AXObjectCache().GetOrCreate(caption);
      if (caption_ax_object) {
        AXObjectSet visited;
        description =
            RecursiveTextAlternative(*caption_ax_object, false, visited);
        if (related_objects) {
          related_objects->push_back(
              MakeGarbageCollected<NameSourceRelatedObject>(caption_ax_object,
                                                            description));
        }

        if (description_sources) {
          DescriptionSource& source = description_sources->back();
          source.related_objects = *related_objects;
          source.text = description;
          found_description = true;
        } else {
          return description;
        }
      }
    }
  }

  // summary, 5.6.2 from:
  // http://rawgit.com/w3c/aria/master/html-aam/html-aam.html
  if (name_from != ax::mojom::NameFrom::kContents &&
      IsA<HTMLSummaryElement>(GetNode())) {
    description_from = ax::mojom::DescriptionFrom::kContents;
    if (description_sources) {
      description_sources->push_back(DescriptionSource(found_description));
      description_sources->back().type = description_from;
    }

    AXObjectSet visited;
    description = TextFromDescendants(visited, false);

    if (!description.IsEmpty()) {
      if (description_sources) {
        found_description = true;
        description_sources->back().text = description;
      } else {
        return description;
      }
    }
  }

  // title attribute, from:
  // http://rawgit.com/w3c/aria/master/html-aam/html-aam.html
  if (name_from != ax::mojom::NameFrom::kTitle) {
    description_from = ax::mojom::DescriptionFrom::kTitle;
    if (description_sources) {
      description_sources->push_back(
          DescriptionSource(found_description, kTitleAttr));
      description_sources->back().type = description_from;
    }
    const AtomicString& title = GetAttribute(kTitleAttr);
    if (!title.IsEmpty()) {
      description = title;
      if (description_sources) {
        found_description = true;
        description_sources->back().text = description;
      } else {
        return description;
      }
    }
  }

  description_from = ax::mojom::DescriptionFrom::kUninitialized;

  if (found_description) {
    for (DescriptionSource& description_source : *description_sources) {
      if (!description_source.text.IsNull() && !description_source.superseded) {
        description_from = description_source.type;
        if (!description_source.related_objects.IsEmpty())
          *related_objects = description_source.related_objects;
        return description_source.text;
      }
    }
  }

  return String();
}

String AXNodeObject::Placeholder(ax::mojom::NameFrom name_from) const {
  if (name_from == ax::mojom::NameFrom::kPlaceholder)
    return String();

  Node* node = GetNode();
  if (!node || !node->IsHTMLElement())
    return String();

  String native_placeholder = PlaceholderFromNativeAttribute();
  if (!native_placeholder.IsEmpty())
    return native_placeholder;

  const AtomicString& aria_placeholder =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kPlaceholder);
  if (!aria_placeholder.IsEmpty())
    return aria_placeholder;

  return String();
}

String AXNodeObject::Title(ax::mojom::NameFrom name_from) const {
  if (name_from == ax::mojom::NameFrom::kTitle)
    return String();

  if (const auto* element = GetElement()) {
    String title = element->title();
    if (!title.IsEmpty())
      return title;
  }

  return String();
}

String AXNodeObject::PlaceholderFromNativeAttribute() const {
  Node* node = GetNode();
  if (!node || !blink::IsTextControl(*node))
    return String();
  return ToTextControl(node)->StrippedPlaceholder();
}

void AXNodeObject::Trace(Visitor* visitor) {
  visitor->Trace(node_);
  AXObject::Trace(visitor);
}

}  // namespace blink
