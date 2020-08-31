/*
 * Copyright (C) 2014, Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/accessibility/scoped_blink_ax_event_intent.h"
#include "third_party/blink/renderer/core/aom/accessible_node.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/listed_element.h"
#include "third_party/blink/renderer/core/html/html_area_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_table_cell_element.h"
#include "third_party/blink/renderer/core/html/html_table_element.h"
#include "third_party/blink/renderer/core/html/html_table_row_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_api_shim.h"
#include "third_party/blink/renderer/core/layout/layout_progress.h"
#include "third_party/blink/renderer/core/layout/layout_slider.h"
#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/layout_table_row.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/line/abstract_inline_text_box.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/accessibility/ax_image_map_link.h"
#include "third_party/blink/renderer/modules/accessibility/ax_inline_text_box.h"
#include "third_party/blink/renderer/modules/accessibility/ax_layout_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_list.h"
#include "third_party/blink/renderer/modules/accessibility/ax_list_box.h"
#include "third_party/blink/renderer/modules/accessibility/ax_list_box_option.h"
#include "third_party/blink/renderer/modules/accessibility/ax_media_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_menu_list.h"
#include "third_party/blink/renderer/modules/accessibility/ax_menu_list_option.h"
#include "third_party/blink/renderer/modules/accessibility/ax_menu_list_popup.h"
#include "third_party/blink/renderer/modules/accessibility/ax_progress_indicator.h"
#include "third_party/blink/renderer/modules/accessibility/ax_radio_input.h"
#include "third_party/blink/renderer/modules/accessibility/ax_relation_cache.h"
#include "third_party/blink/renderer/modules/accessibility/ax_slider.h"
#include "third_party/blink/renderer/modules/accessibility/ax_svg_root.h"
#include "third_party/blink/renderer/modules/accessibility/ax_validation_message.h"
#include "third_party/blink/renderer/modules/accessibility/ax_virtual_object.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "ui/accessibility/ax_enums.mojom-blink.h"
#include "ui/accessibility/ax_event.h"

// Prevent code that runs during the lifetime of the stack from altering the
// document lifecycle. Usually doc is the same as document_, but it can be
// different when it is a popup document. Because it's harmless to test both
// documents, even if they are the same, the scoped check is initialized for
// both documents.
// clang-format off
#if DCHECK_IS_ON()
#define SCOPED_DISALLOW_LIFECYCLE_TRANSITION(document)                        \
  DocumentLifecycle::DisallowTransitionScope scoped1((document).Lifecycle()); \
  DocumentLifecycle::DisallowTransitionScope scoped2(document_->Lifecycle())
#else
#define SCOPED_DISALLOW_LIFECYCLE_TRANSITION(document)
#endif  // DCHECK_IS_ON()
// clang-format on

namespace blink {

namespace {

// Return a node for the current layout object or ancestor layout object.
Node* GetClosestNodeForLayoutObject(LayoutObject* layout_object) {
  if (!layout_object)
    return nullptr;
  Node* node = layout_object->GetNode();
  return node ? node : GetClosestNodeForLayoutObject(layout_object->Parent());
}

bool IsActive(Document& document) {
  return document.IsActive() && !document.IsDetached();
}

}  // namespace

// static
AXObjectCache* AXObjectCacheImpl::Create(Document& document) {
  return MakeGarbageCollected<AXObjectCacheImpl>(document);
}

AXObjectCacheImpl::AXObjectCacheImpl(Document& document)
    : document_(document),
      modification_count_(0),
      validation_message_axid_(0),
      relation_cache_(std::make_unique<AXRelationCache>(this)),
      accessibility_event_permission_(mojom::blink::PermissionStatus::ASK),
      permission_service_(document.GetExecutionContext()),
      permission_observer_receiver_(this, document.GetExecutionContext()) {
  if (document_->LoadEventFinished())
    AddPermissionStatusListener();
  documents_.insert(&document);
  relation_cache_->Init();
}

AXObjectCacheImpl::~AXObjectCacheImpl() {
#if DCHECK_IS_ON()
  DCHECK(has_been_disposed_);
#endif
}

void AXObjectCacheImpl::Dispose() {
  for (auto& entry : objects_) {
    AXObject* obj = entry.value;
    obj->Detach();
    RemoveAXID(obj);
  }

  permission_observer_receiver_.reset();

#if DCHECK_IS_ON()
  has_been_disposed_ = true;
#endif
}

AXObject* AXObjectCacheImpl::Root() {
  return GetOrCreate(document_);
}

void AXObjectCacheImpl::InitializePopup(Document* document) {
  if (!document || documents_.Contains(document) || !document->View())
    return;

  documents_.insert(document);
}

void AXObjectCacheImpl::DisposePopup(Document* document) {
  if (!documents_.Contains(document) || !document->View())
    return;

  documents_.erase(document);
}

Node* AXObjectCacheImpl::FocusedElement() {
  Node* focused_node = document_->FocusedElement();
  if (!focused_node)
    focused_node = document_;

  // If it's an image map, get the focused link within the image map.
  if (IsA<HTMLAreaElement>(focused_node))
    return focused_node;

  // See if there's a page popup, for example a calendar picker.
  Element* adjusted_focused_element = document_->AdjustedFocusedElement();
  if (auto* input = DynamicTo<HTMLInputElement>(adjusted_focused_element)) {
    if (AXObject* ax_popup = input->PopupRootAXObject()) {
      if (Element* focused_element_in_popup =
              ax_popup->GetDocument()->FocusedElement())
        focused_node = focused_element_in_popup;
    }
  }

  return focused_node;
}

AXObject* AXObjectCacheImpl::GetOrCreateFocusedObjectFromNode(Node* node) {
  // If it's an image map, get the focused link within the image map.
  if (auto* area = DynamicTo<HTMLAreaElement>(node))
    return FocusedImageMapUIElement(area);

  AXObject* obj = GetOrCreate(node);
  if (!obj)
    return nullptr;

  // the HTML element, for example, is focusable but has an AX object that is
  // ignored
  if (!obj->AccessibilityIsIncludedInTree())
    obj = obj->ParentObjectIncludedInTree();

  return obj;
}

AXObject* AXObjectCacheImpl::FocusedImageMapUIElement(
    HTMLAreaElement* area_element) {
  // Find the corresponding accessibility object for the HTMLAreaElement. This
  // should be in the list of children for its corresponding image.
  if (!area_element)
    return nullptr;

  HTMLImageElement* image_element = area_element->ImageElement();
  if (!image_element)
    return nullptr;

  AXObject* ax_layout_image = GetOrCreate(image_element);
  if (!ax_layout_image)
    return nullptr;

  const AXObject::AXObjectVector& image_children = ax_layout_image->Children();
  unsigned count = image_children.size();
  for (unsigned k = 0; k < count; ++k) {
    AXObject* child = image_children[k];
    auto* ax_object = DynamicTo<AXImageMapLink>(child);
    if (!ax_object)
      continue;

    if (ax_object->AreaElement() == area_element)
      return child;
  }

  return nullptr;
}

AXObject* AXObjectCacheImpl::FocusedObject() {
  return GetOrCreateFocusedObjectFromNode(this->FocusedElement());
}

AXObject* AXObjectCacheImpl::Get(LayoutObject* layout_object) {
  if (!layout_object)
    return nullptr;

  AXID ax_id = layout_object_mapping_.at(layout_object);
  DCHECK(!HashTraits<AXID>::IsDeletedValue(ax_id));

  Node* node = layout_object->GetNode();
  if (node && DisplayLockUtilities::NearestLockedExclusiveAncestor(*node)) {
    // It's in a locked subtree so we need to search by node instead of by
    // layout object.
    if (ax_id) {
      // We previously saved the node in the cache with its layout object,
      // but now it's in a locked subtree so we should remove the entry with its
      // layout object and replace it with an AXNodeObject created from the node
      // instead.
      Remove(ax_id);
      return GetOrCreate(node);
    }
    return Get(node);
  }

  if (!ax_id)
    return nullptr;

  return objects_.at(ax_id);
}

// Returns true if |node| is an <option> element and its parent <select>
// is a menu list (not a list box).
static bool IsMenuListOption(const Node* node) {
  auto* option_element = DynamicTo<HTMLOptionElement>(node);
  if (!option_element)
    return false;
  const HTMLSelectElement* select = option_element->OwnerSelectElement();
  if (!select || !select->UsesMenuList())
    return false;
  return select->GetLayoutObject();
}

AXObject* AXObjectCacheImpl::Get(const Node* node) {
  if (!node)
    return nullptr;

  // Menu list option and HTML area elements are indexed by DOM node, never by
  // layout object.
  LayoutObject* layout_object = node->GetLayoutObject();
  if (IsMenuListOption(node) || IsA<HTMLAreaElement>(node))
    layout_object = nullptr;

  AXID layout_id = layout_object ? layout_object_mapping_.at(layout_object) : 0;
  DCHECK(!HashTraits<AXID>::IsDeletedValue(layout_id));

  AXID node_id = node_object_mapping_.at(node);
  DCHECK(!HashTraits<AXID>::IsDeletedValue(node_id));

  if (layout_object &&
      DisplayLockUtilities::NearestLockedExclusiveAncestor(*node)) {
    // The node is in a display locked subtree, but we've previously put it in
    // the cache with its layout object.
    if (layout_id) {
      Remove(layout_id);
      layout_id = 0;
    }
    layout_object = nullptr;
  }

  if (layout_object && node_id && !layout_id && !IsMenuListOption(node) &&
      !IsA<HTMLAreaElement>(node)) {
    // This can happen if an AXNodeObject is created for a node that's not laid
    // out, but later something changes and it gets a layoutObject (like if it's
    // reparented). It's also possible the layout object changed.
    // In any case, reuse the ax_id since the node didn't change.
    Remove(node_id);

    // Note that this codepath can be reached when |layout_object| is about to
    // be destroyed.

    // This potentially misses root LayoutObject re-creation, but we have no way
    // of knowing whether the |layout_object| in those cases is still valid.
    if (!layout_object->Parent())
      return nullptr;

    layout_object_mapping_.Set(layout_object, node_id);
    AXObject* new_obj = CreateFromRenderer(layout_object);
    ids_in_use_.insert(node_id);
    new_obj->SetAXObjectID(node_id);
    objects_.Set(node_id, new_obj);
    new_obj->Init();
    new_obj->SetLastKnownIsIgnoredValue(new_obj->AccessibilityIsIgnored());
    new_obj->SetLastKnownIsIgnoredButIncludedInTreeValue(
        new_obj->AccessibilityIsIgnoredButIncludedInTree());
    return new_obj;
  }

  if (layout_id)
    return objects_.at(layout_id);

  if (!node_id)
    return nullptr;

  return objects_.at(node_id);
}

AXObject* AXObjectCacheImpl::Get(AbstractInlineTextBox* inline_text_box) {
  if (!inline_text_box)
    return nullptr;

  AXID ax_id = inline_text_box_object_mapping_.at(inline_text_box);
  DCHECK(!HashTraits<AXID>::IsDeletedValue(ax_id));
  if (!ax_id)
    return nullptr;

  return objects_.at(ax_id);
}

AXID AXObjectCacheImpl::GetAXID(Node* node) {
  AXObject* ax_object = GetOrCreate(node);
  if (!ax_object)
    return 0;
  return ax_object->AXObjectID();
}

Element* AXObjectCacheImpl::GetElementFromAXID(AXID axid) {
  AXObject* ax_object = ObjectFromAXID(axid);
  if (!ax_object || !ax_object->GetElement())
    return nullptr;
  return ax_object->GetElement();
}

AXObject* AXObjectCacheImpl::Get(AccessibleNode* accessible_node) {
  if (!accessible_node)
    return nullptr;

  AXID ax_id = accessible_node_mapping_.at(accessible_node);
  DCHECK(!HashTraits<AXID>::IsDeletedValue(ax_id));
  if (!ax_id)
    return nullptr;

  return objects_.at(ax_id);
}

// FIXME: This probably belongs on Node.
// FIXME: This should take a const char*, but one caller passes g_null_atom.
static bool NodeHasRole(Node* node, const String& role) {
  auto* element = DynamicTo<Element>(node);
  if (!element)
    return false;

  // TODO(accessibility) support role strings with multiple roles.
  return EqualIgnoringASCIICase(
      element->FastGetAttribute(html_names::kRoleAttr), role);
}

AXObject* AXObjectCacheImpl::CreateFromRenderer(LayoutObject* layout_object) {
  // FIXME: How could layoutObject->node() ever not be an Element?
  Node* node = layout_object->GetNode();

  // If the node is aria role="list" or the aria role is empty and its a
  // ul/ol/dl type (it shouldn't be a list if aria says otherwise).
  if (NodeHasRole(node, "list") || NodeHasRole(node, "directory") ||
      (NodeHasRole(node, g_null_atom) &&
       (IsA<HTMLUListElement>(node) || IsA<HTMLOListElement>(node) ||
        IsA<HTMLDListElement>(node))))
    return MakeGarbageCollected<AXList>(layout_object, *this);

  // media element
  if (node && node->IsMediaElement())
    return AccessibilityMediaElement::Create(layout_object, *this);

  if (IsA<HTMLOptionElement>(node))
    return MakeGarbageCollected<AXListBoxOption>(layout_object, *this);

  auto* html_input_element = DynamicTo<HTMLInputElement>(node);
  if (html_input_element &&
      html_input_element->type() == input_type_names::kRadio)
    return MakeGarbageCollected<AXRadioInput>(layout_object, *this);

  if (layout_object->IsSVGRoot())
    return MakeGarbageCollected<AXSVGRoot>(layout_object, *this);

  if (layout_object->IsBoxModelObject()) {
    LayoutBoxModelObject* css_box = ToLayoutBoxModelObject(layout_object);
    if (auto* select_element = DynamicTo<HTMLSelectElement>(node)) {
      if (select_element->UsesMenuList())
        return MakeGarbageCollected<AXMenuList>(css_box, *this);
      return MakeGarbageCollected<AXListBox>(css_box, *this);
    }

    // progress bar
    if (css_box->IsProgress()) {
      return MakeGarbageCollected<AXProgressIndicator>(
          ToLayoutProgress(css_box), *this);
    }

    // input type=range
    if (auto* slider = DynamicTo<LayoutSlider>(css_box))
      return MakeGarbageCollected<AXSlider>(slider, *this);
  }

  return MakeGarbageCollected<AXLayoutObject>(layout_object, *this);
}

AXObject* AXObjectCacheImpl::CreateFromNode(Node* node) {
  if (IsMenuListOption(node)) {
    return MakeGarbageCollected<AXMenuListOption>(To<HTMLOptionElement>(node),
                                                  *this);
  }

  if (auto* area = DynamicTo<HTMLAreaElement>(node))
    return MakeGarbageCollected<AXImageMapLink>(area, *this);

  return MakeGarbageCollected<AXNodeObject>(node, *this);
}

AXObject* AXObjectCacheImpl::CreateFromInlineTextBox(
    AbstractInlineTextBox* inline_text_box) {
  return MakeGarbageCollected<AXInlineTextBox>(inline_text_box, *this);
}

AXObject* AXObjectCacheImpl::GetOrCreate(AccessibleNode* accessible_node) {
  if (AXObject* obj = Get(accessible_node))
    return obj;

  AXObject* new_obj =
      MakeGarbageCollected<AXVirtualObject>(*this, accessible_node);
  const AXID ax_id = GetOrCreateAXID(new_obj);
  accessible_node_mapping_.Set(accessible_node, ax_id);

  new_obj->Init();
  return new_obj;
}

AXObject* AXObjectCacheImpl::GetOrCreate(const Node* node) {
  return GetOrCreate(const_cast<Node*>(node));
}

AXObject* AXObjectCacheImpl::GetOrCreate(Node* node) {
  if (!node)
    return nullptr;

  if (!node->IsElementNode() && !node->IsTextNode() && !node->IsDocumentNode())
    return nullptr;  // Only documents, elements and text nodes get a11y objects

  if (AXObject* obj = Get(node))
    return obj;

  // If the node has a layout object, prefer using that as the primary key for
  // the AXObject, with the exception of the HTMLAreaElement and nodes within
  // a locked subtree, which are created based on its node.
  if (node->GetLayoutObject() && !IsA<HTMLAreaElement>(node) &&
      !DisplayLockUtilities::NearestLockedExclusiveAncestor(*node)) {
    return GetOrCreate(node->GetLayoutObject());
  }

  if (!LayoutTreeBuilderTraversal::Parent(*node))
    return nullptr;

  if (IsA<HTMLHeadElement>(node))
    return nullptr;

  AXObject* new_obj = CreateFromNode(node);

  // Will crash later if we have two objects for the same node.
  DCHECK(!Get(node));

  const AXID ax_id = GetOrCreateAXID(new_obj);
  DCHECK(!HashTraits<AXID>::IsDeletedValue(ax_id));
  node_object_mapping_.Set(node, ax_id);
  new_obj->Init();
  new_obj->SetLastKnownIsIgnoredValue(new_obj->AccessibilityIsIgnored());
  new_obj->SetLastKnownIsIgnoredButIncludedInTreeValue(
      new_obj->AccessibilityIsIgnoredButIncludedInTree());
  MaybeNewRelationTarget(node, new_obj);

  return new_obj;
}

AXObject* AXObjectCacheImpl::GetOrCreate(LayoutObject* layout_object) {
  if (!layout_object)
    return nullptr;

  if (AXObject* obj = Get(layout_object))
    return obj;

  // Area elements are never created based on layout objects (see |Get|), so we
  // really should never get here.
  Node* node = layout_object->GetNode();
  if (node && (IsMenuListOption(node) || IsA<HTMLAreaElement>(node)))
    return nullptr;

  // Prefer creating AXNodeObjects over AXLayoutObjects in locked subtrees
  // (e.g. content-visibility: auto), even if a LayoutObject is available,
  // because the LayoutObject is not guaranteed to be up-to-date (it might come
  // from a previous layout update), or even it is up-to-date, it may not remain
  // up-to-date. Blink doesn't update style/layout for nodes in locked
  // subtrees, so creating a matching AXLayoutObjects could lead to the use of
  // old information. Note that Blink will recreate the AX objects as
  // AXLayoutObjects when a locked element is activated, aka it becomes visible.
  // Visit https://wicg.github.io/display-locking/#accessibility for more info.
  if (DisplayLockUtilities::NearestLockedExclusiveAncestor(*layout_object)) {
    if (!node) {
      // Nodeless objects such as anonymous blocks do not get accessible objects
      // in a locked subtree. Anonymous blocks are added to help layout when
      // a block and inline are siblings.
      // This prevents an odd mixture of ax objects in a locked subtree, e.g.
      // AXNodeObjects when there is a node, and AXLayoutObjects
      // when there isn't. The locked subtree should not have AXLayoutObjects.
      return nullptr;
    }
    return GetOrCreate(layout_object->GetNode());
  }

  AXObject* new_obj = CreateFromRenderer(layout_object);

  // Will crash later if we have two objects for the same layoutObject.
  DCHECK(!Get(layout_object));

  const AXID axid = GetOrCreateAXID(new_obj);

  layout_object_mapping_.Set(layout_object, axid);
  new_obj->Init();
  new_obj->SetLastKnownIsIgnoredValue(new_obj->AccessibilityIsIgnored());
  new_obj->SetLastKnownIsIgnoredButIncludedInTreeValue(
      new_obj->AccessibilityIsIgnoredButIncludedInTree());
  if (node && node->GetLayoutObject() == layout_object) {
    AXID prev_axid = node_object_mapping_.at(node);
    if (prev_axid != 0 && prev_axid != axid) {
      Remove(prev_axid);
      node_object_mapping_.Set(node, axid);
    }
    MaybeNewRelationTarget(node, new_obj);
  }

  return new_obj;
}

AXObject* AXObjectCacheImpl::GetOrCreate(
    AbstractInlineTextBox* inline_text_box) {
  if (!inline_text_box)
    return nullptr;

  if (AXObject* obj = Get(inline_text_box))
    return obj;

  AXObject* new_obj = CreateFromInlineTextBox(inline_text_box);

  // Will crash later if we have two objects for the same inlineTextBox.
  DCHECK(!Get(inline_text_box));

  const AXID axid = GetOrCreateAXID(new_obj);

  inline_text_box_object_mapping_.Set(inline_text_box, axid);
  new_obj->Init();
  new_obj->SetLastKnownIsIgnoredValue(new_obj->AccessibilityIsIgnored());
  new_obj->SetLastKnownIsIgnoredButIncludedInTreeValue(
      new_obj->AccessibilityIsIgnoredButIncludedInTree());
  return new_obj;
}

AXObject* AXObjectCacheImpl::GetOrCreate(ax::mojom::blink::Role role) {
  AXObject* obj = nullptr;

  switch (role) {
    case ax::mojom::Role::kSliderThumb:
      obj = MakeGarbageCollected<AXSliderThumb>(*this);
      break;
    case ax::mojom::Role::kMenuListPopup:
      obj = MakeGarbageCollected<AXMenuListPopup>(*this);
      break;
    default:
      obj = nullptr;
  }

  if (!obj)
    return nullptr;

  GetOrCreateAXID(obj);

  obj->Init();
  return obj;
}

ContainerNode* FindParentTable(Node* node) {
  ContainerNode* parent = node->parentNode();
  while (parent && !IsA<HTMLTableElement>(*parent))
    parent = parent->parentNode();
  return parent;
}

void AXObjectCacheImpl::ContainingTableRowsOrColsMaybeChanged(Node* node) {
  // Any containing table must recompute its rows and columns on insertion or
  // removal of a <tr> or <td>.
  // Get parent table from DOM, because AXObject/layout tree are incomplete.
  ContainerNode* containing_table = nullptr;
  if (IsA<HTMLTableCellElement>(node) || IsA<HTMLTableRowElement>(node))
    containing_table = FindParentTable(node);

  if (containing_table) {
    AXObject* ax_table = Get(containing_table);
    if (ax_table)
      ax_table->SetNeedsToUpdateChildren();
  }
}

void AXObjectCacheImpl::InvalidateTableSubtree(AXObject* subtree) {
  if (!subtree)
    return;

  LayoutObject* layout_object = subtree->GetLayoutObject();
  if (layout_object) {
    LayoutObject* layout_child = layout_object->SlowFirstChild();
    while (layout_child) {
      InvalidateTableSubtree(Get(layout_child));
      layout_child = layout_child->NextSibling();
    }
  }

  AXID ax_id = subtree->AXObjectID();
  Remove(ax_id);
}

void AXObjectCacheImpl::Remove(AXID ax_id) {
  if (!ax_id)
    return;

  // First, fetch object to operate some cleanup functions on it.
  AXObject* obj = objects_.at(ax_id);
  if (!obj)
    return;

  obj->Detach();
  RemoveAXID(obj);

  // Finally, remove the object.
  if (!objects_.Take(ax_id))
    return;

  DCHECK_GE(objects_.size(), ids_in_use_.size());
}

void AXObjectCacheImpl::Remove(AccessibleNode* accessible_node) {
  if (!accessible_node)
    return;

  AXID ax_id = accessible_node_mapping_.at(accessible_node);
  Remove(ax_id);
  accessible_node_mapping_.erase(accessible_node);
}

void AXObjectCacheImpl::Remove(LayoutObject* layout_object) {
  if (!layout_object)
    return;

  AXID ax_id = layout_object_mapping_.at(layout_object);
  Remove(ax_id);
  layout_object_mapping_.erase(layout_object);
}

void AXObjectCacheImpl::Remove(Node* node) {
  if (!node)
    return;

  // This is all safe even if we didn't have a mapping.
  AXID ax_id = node_object_mapping_.at(node);
  Remove(ax_id);
  node_object_mapping_.erase(node);

  if (node->GetLayoutObject())
    Remove(node->GetLayoutObject());
}

void AXObjectCacheImpl::Remove(AbstractInlineTextBox* inline_text_box) {
  if (!inline_text_box)
    return;

  AXID ax_id = inline_text_box_object_mapping_.at(inline_text_box);
  Remove(ax_id);
  inline_text_box_object_mapping_.erase(inline_text_box);
}

AXID AXObjectCacheImpl::GenerateAXID() const {
  static AXID last_used_id = 0;

  // Generate a new ID.
  AXID obj_id = last_used_id;
  do {
    ++obj_id;
  } while (!obj_id || HashTraits<AXID>::IsDeletedValue(obj_id) ||
           ids_in_use_.Contains(obj_id));

  last_used_id = obj_id;

  return obj_id;
}

AXID AXObjectCacheImpl::GetOrCreateAXID(AXObject* obj) {
  // check for already-assigned ID
  const AXID existing_axid = obj->AXObjectID();
  if (existing_axid) {
    DCHECK(ids_in_use_.Contains(existing_axid));
    return existing_axid;
  }

  const AXID new_axid = GenerateAXID();

  ids_in_use_.insert(new_axid);
  obj->SetAXObjectID(new_axid);
  objects_.Set(new_axid, obj);

  return new_axid;
}

void AXObjectCacheImpl::RemoveAXID(AXObject* object) {
  if (!object)
    return;

  AXID obj_id = object->AXObjectID();
  if (!obj_id)
    return;
  DCHECK(!HashTraits<AXID>::IsDeletedValue(obj_id));
  DCHECK(ids_in_use_.Contains(obj_id));
  object->SetAXObjectID(0);
  ids_in_use_.erase(obj_id);
  autofill_state_map_.erase(obj_id);

  relation_cache_->RemoveAXID(obj_id);
}

AXObject* AXObjectCacheImpl::NearestExistingAncestor(Node* node) {
  // Find the nearest ancestor that already has an accessibility object, since
  // we might be in the middle of a layout.
  while (node) {
    if (AXObject* obj = Get(node))
      return obj;
    node = node->parentNode();
  }
  return nullptr;
}

AXObject::InOrderTraversalIterator AXObjectCacheImpl::InOrderTraversalBegin() {
  AXObject* root = Root();
  if (root)
    return AXObject::InOrderTraversalIterator(*root);
  return InOrderTraversalEnd();
}

AXObject::InOrderTraversalIterator AXObjectCacheImpl::InOrderTraversalEnd() {
  return AXObject::InOrderTraversalIterator();
}

void AXObjectCacheImpl::DeferTreeUpdateInternal(Node* node,
                                                base::OnceClosure callback) {
  // The node's document can be different from the main document_ when the node
  // is inside a popup. Check to ensure both documents are in a good state.
  if (!node || !IsActive(node->GetDocument()) || !IsActive(GetDocument()))
    return;

  DCHECK(!node->GetDocument().GetPage()->Animator().IsServicingAnimations() ||
         (node->GetDocument().Lifecycle().GetState() <
              DocumentLifecycle::kInAccessibility ||
          node->GetDocument().Lifecycle().StateAllowsDetach()))
      << "DeferTreeUpdateInternal should only be outside of the lifecycle or "
         "before the accessibility state.";
  tree_update_callback_queue_.push_back(MakeGarbageCollected<TreeUpdateParams>(
      node, ComputeEventFrom(), ActiveEventIntents(), std::move(callback)));

  // These events are fired during DocumentLifecycle::kInAccessibility,
  // ensure there is a document lifecycle update scheduled.
  ScheduleVisualUpdate();
}

void AXObjectCacheImpl::DeferTreeUpdate(
    void (AXObjectCacheImpl::*method)(Node*),
    Node* node) {
  base::OnceClosure callback =
      WTF::Bind(method, WrapWeakPersistent(this), WrapWeakPersistent(node));
  DeferTreeUpdateInternal(node, std::move(callback));
}

void AXObjectCacheImpl::DeferTreeUpdate(
    void (AXObjectCacheImpl::*method)(const QualifiedName&, Element* element),
    const QualifiedName& attr_name,
    Element* element) {
  base::OnceClosure callback = WTF::Bind(
      method, WrapWeakPersistent(this), attr_name, WrapWeakPersistent(element));
  DeferTreeUpdateInternal(element, std::move(callback));
}

void AXObjectCacheImpl::DeferTreeUpdate(
    void (AXObjectCacheImpl::*method)(Node*, AXObject*),
    Node* node,
    AXObject* obj) {
  base::OnceClosure callback =
      WTF::Bind(method, WrapWeakPersistent(this), WrapWeakPersistent(node),
                WrapWeakPersistent(obj));
  DeferTreeUpdateInternal(node, std::move(callback));
}

void AXObjectCacheImpl::SelectionChanged(Node* node) {
  if (!node)
    return;

  DeferTreeUpdate(&AXObjectCacheImpl::SelectionChangedWithCleanLayout, node);
}

void AXObjectCacheImpl::SelectionChangedWithCleanLayout(Node* node) {
  if (!node)
    return;

  // Something about the call chain for this method seems to leave distribution
  // in a dirty state - update it before we call GetOrCreate so that we don't
  // crash.
  node->UpdateDistributionForFlatTreeTraversal();
  AXObject* ax_object = GetOrCreate(node);
  if (ax_object)
    ax_object->SelectionChanged();
}

void AXObjectCacheImpl::UpdateReverseRelations(
    const AXObject* relation_source,
    const Vector<String>& target_ids) {
  relation_cache_->UpdateReverseRelations(relation_source, target_ids);
}

void AXObjectCacheImpl::TextChanged(Node* node) {
  if (!node)
    return;

  DeferTreeUpdate(&AXObjectCacheImpl::TextChangedWithCleanLayout, node);
}

void AXObjectCacheImpl::TextChanged(LayoutObject* layout_object) {
  if (!layout_object)
    return;

  // TODO(aboxhall): audit calls to this and figure out when this is called
  // when node might be null
  Node* node = GetClosestNodeForLayoutObject(layout_object);
  if (node) {
    DeferTreeUpdate(&AXObjectCacheImpl::TextChangedWithCleanLayout, node);
    return;
  }

  TextChanged(Get(layout_object), layout_object->GetNode());
}

void AXObjectCacheImpl::TextChanged(AXObject* obj,
                                    Node* node_for_relation_update) {
  // TODO(aboxhall): Figure out when this may be called with dirty layout
  if (obj)
    obj->TextChanged();

  if (node_for_relation_update)
    relation_cache_->UpdateRelatedTree(node_for_relation_update);

  PostNotification(obj, ax::mojom::Event::kTextChanged);
}

void AXObjectCacheImpl::TextChangedWithCleanLayout(Node* node) {
  if (!node)
    return;

  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));
  TextChanged(Get(node), node);
}

void AXObjectCacheImpl::FocusableChangedWithCleanLayout(Element* element) {
  DCHECK(element);
  DCHECK(!element->GetDocument().NeedsLayoutTreeUpdateForNode(*element));
  AXObject* obj = GetOrCreate(element);
  if (!obj)
    return;

  if (obj->AriaHiddenRoot()) {
    // Elements that are hidden but focusable are not ignored. Therefore, if a
    // hidden element's focusable state changes, it's ignored state must be
    // recomputed.
    ChildrenChangedWithCleanLayout(element->parentNode());
  }

  // Refresh the focusable state and State::kIgnored on the exposed object.
  MarkAXObjectDirty(obj, false);
}

void AXObjectCacheImpl::DocumentTitleChanged() {
  DocumentLifecycle::DisallowTransitionScope disallow(document_->Lifecycle());

  PostNotification(Root(), ax::mojom::Event::kDocumentTitleChanged);
}

void AXObjectCacheImpl::UpdateCacheAfterNodeIsAttached(Node* node) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(node->GetDocument());
  // Calling get() will update the AX object if we had an AXNodeObject but now
  // we need an AXLayoutObject, because it was reparented to a location outside
  // of a canvas.
  AXObject* obj = Get(node);

  // Process any relation attributes that can affect ax objects already created.

  // Force computation of aria-owns, so that original parents that already
  // computed their children get the aria-owned children removed.
  Element* element = DynamicTo<Element>(node);
  if (!element)
    return;

  if (element->FastHasAttribute(html_names::kAriaOwnsAttr) ||
      element->HasExplicitlySetAttrAssociatedElements(
          html_names::kAriaOwnsAttr)) {
    HandleAttributeChanged(html_names::kAriaOwnsAttr, element);
  }

  // Process cases where relationships are pointing to this node.
  MaybeNewRelationTarget(node, obj);
}

void AXObjectCacheImpl::DidInsertChildrenOfNode(Node* node) {
  // If a node is inserted that is a descendant of a leaf node in the
  // accessibility tree, notify the root of that subtree that its children have
  // changed.
  if (!node)
    return;

  if (AXObject* obj = Get(node)) {
    TextChanged(obj, node);
  } else {
    DidInsertChildrenOfNode(NodeTraversal::Parent(*node));
  }
}

void AXObjectCacheImpl::ChildrenChanged(Node* node) {
  if (!node)
    return;

  DeferTreeUpdate(&AXObjectCacheImpl::ChildrenChangedWithCleanLayout, node);
}

void AXObjectCacheImpl::ChildrenChanged(LayoutObject* layout_object) {
  if (!layout_object)
    return;

  Node* node = GetClosestNodeForLayoutObject(layout_object);

  if (node) {
    DeferTreeUpdate(&AXObjectCacheImpl::ChildrenChangedWithCleanLayout, node);
    return;
  }

  AXObject* object = Get(layout_object);
  ChildrenChanged(object, layout_object->GetNode());
}

void AXObjectCacheImpl::ChildrenChanged(AccessibleNode* accessible_node) {
  if (!accessible_node)
    return;

  AXObject* object = Get(accessible_node);
  ChildrenChanged(object, object ? object->GetNode() : nullptr);
}

void AXObjectCacheImpl::ChildrenChangedWithCleanLayout(Node* node) {
  if (!node)
    return;

  DCHECK(node->GetDocument().Lifecycle().GetState() >=
         DocumentLifecycle::kLayoutClean);
#ifndef NDEBUG
  if (node->GetDocument().NeedsLayoutTreeUpdateForNode(*node)) {
    LOG(ERROR) << "Node needs layout tree update: " << node;
    node->ShowTreeForThisAcrossFrame();
  }
#endif
  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));

  ChildrenChanged(Get(node), node);
}

void AXObjectCacheImpl::ChildrenChanged(AXObject* obj, Node* optional_node) {
  // TODO(aboxhall): Figure out when this may be called with dirty layout
  if (obj)
    obj->ChildrenChanged();

  if (optional_node) {
    ContainingTableRowsOrColsMaybeChanged(optional_node);
    relation_cache_->UpdateRelatedTree(optional_node);
  }
}

void AXObjectCacheImpl::ProcessDeferredAccessibilityEvents(Document& document) {
  if (document.Lifecycle().GetState() != DocumentLifecycle::kInAccessibility) {
    DCHECK(false) << "Deferred events should only be processed during the "
                     "accessibility document lifecycle";
    return;
  }

  ProcessUpdates(document);
  PostNotifications(document);
}

void AXObjectCacheImpl::ProcessUpdates(Document& document) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(document);

  TreeUpdateCallbackQueue old_tree_update_callback_queue;
  tree_update_callback_queue_.swap(old_tree_update_callback_queue);
  for (auto& tree_update : old_tree_update_callback_queue) {
    Node* node = tree_update->node;
    if (!node)
      continue;

    base::OnceClosure& callback = tree_update->callback;
    if (node->GetDocument() != document) {
      tree_update_callback_queue_.push_back(
          MakeGarbageCollected<TreeUpdateParams>(node, tree_update->event_from,
                                                 tree_update->event_intents,
                                                 std::move(callback)));
      continue;
    }

    FireTreeUpdatedEventImmediately(node, tree_update->event_from,
                                    tree_update->event_intents,
                                    std::move(callback));
  }
}

void AXObjectCacheImpl::PostNotifications(Document& document) {
  HeapVector<Member<AXEventParams>> old_notifications_to_post;
  notifications_to_post_.swap(old_notifications_to_post);
  for (auto& params : old_notifications_to_post) {
    AXObject* obj = params->target;

    if (!obj || !obj->AXObjectID())
      continue;

    if (obj->IsDetached())
      continue;

    ax::mojom::blink::Event event_type = params->event_type;
    ax::mojom::blink::EventFrom event_from = params->event_from;
    const BlinkAXEventIntentsSet& event_intents = params->event_intents;
    if (obj->GetDocument() != &document) {
      notifications_to_post_.push_back(MakeGarbageCollected<AXEventParams>(
          obj, event_type, event_from, event_intents));
      continue;
    }

    FireAXEventImmediately(obj, event_type, event_from, event_intents);
  }
}

void AXObjectCacheImpl::PostNotification(LayoutObject* layout_object,
                                         ax::mojom::blink::Event notification) {
  if (!layout_object)
    return;
  PostNotification(Get(layout_object), notification);
}

void AXObjectCacheImpl::PostNotification(Node* node,
                                         ax::mojom::blink::Event notification) {
  if (!node)
    return;
  PostNotification(Get(node), notification);
}

void AXObjectCacheImpl::PostNotification(AXObject* object,
                                         ax::mojom::blink::Event event_type) {
  if (!object || !object->AXObjectID() || object->IsDetached())
    return;

  modification_count_++;

  // It's possible for FireAXEventImmediately to post another notification.
  // If we're still in the accessibility document lifecycle, fire these events
  // immediately rather than deferring them.
  if (object->GetDocument()->Lifecycle().GetState() ==
      DocumentLifecycle::kInAccessibility) {
    FireAXEventImmediately(object, event_type, ComputeEventFrom(),
                           ActiveEventIntents());
    return;
  }

  notifications_to_post_.push_back(MakeGarbageCollected<AXEventParams>(
      object, event_type, ComputeEventFrom(), ActiveEventIntents()));

  // These events are fired during DocumentLifecycle::kInAccessibility,
  // ensure there is a visual update scheduled.
  ScheduleVisualUpdate();
}

void AXObjectCacheImpl::ScheduleVisualUpdate() {
  // Scheduling visual updates before the document is finished loading can
  // interfere with event ordering.
  if (!GetDocument().IsLoadCompleted())
    return;

  // If there was a document change that doesn't trigger a lifecycle update on
  // its own, (e.g. because it doesn't make layout dirty), make sure we run
  // lifecycle phases to update the computed accessibility tree.
  LocalFrameView* frame_view = GetDocument().View();
  Page* page = GetDocument().GetPage();
  if (!frame_view || !page)
    return;

  if (!frame_view->CanThrottleRendering() &&
      (!GetDocument().GetPage()->Animator().IsServicingAnimations() ||
       GetDocument().Lifecycle().GetState() >=
           DocumentLifecycle::kInAccessibility)) {
    page->Animator().ScheduleVisualUpdate(GetDocument().GetFrame());
  }
}

void AXObjectCacheImpl::FireTreeUpdatedEventImmediately(
    Node* node,
    ax::mojom::blink::EventFrom event_from,
    const BlinkAXEventIntentsSet& event_intents,
    base::OnceClosure callback) {
  DCHECK_EQ(node->GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kInAccessibility);

  base::AutoReset<ax::mojom::blink::EventFrom> event_from_resetter(
      &active_event_from_, event_from);
  ScopedBlinkAXEventIntent defered_event_intents(event_intents.AsVector(),
                                                 &node->GetDocument());
  std::move(callback).Run();
}

void AXObjectCacheImpl::FireAXEventImmediately(
    AXObject* obj,
    ax::mojom::blink::Event event_type,
    ax::mojom::blink::EventFrom event_from,
    const BlinkAXEventIntentsSet& event_intents) {
  DCHECK_EQ(obj->GetDocument()->Lifecycle().GetState(),
            DocumentLifecycle::kInAccessibility);

#if DCHECK_IS_ON()
  // Make sure none of the layout views are in the process of being laid out.
  // Notifications should only be sent after the layoutObject has finished
  auto* ax_layout_object = DynamicTo<AXLayoutObject>(obj);
  if (ax_layout_object) {
    LayoutObject* layout_object = ax_layout_object->GetLayoutObject();
    if (layout_object && layout_object->View())
      DCHECK(!layout_object->View()->GetLayoutState());
  }

  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(*obj->GetDocument());
#endif  // DCHECK_IS_ON()

  PostPlatformNotification(obj, event_type, event_from, event_intents);

  if (event_type == ax::mojom::blink::Event::kChildrenChanged &&
      obj->CachedParentObject()) {
    const bool was_ignored = obj->LastKnownIsIgnoredValue();
    const bool was_ignored_but_included_in_tree =
        obj->LastKnownIsIgnoredButIncludedInTreeValue();
    bool is_ignored_changed =
        was_ignored != obj->AccessibilityIsIgnored() ||
        was_ignored_but_included_in_tree !=
            obj->AccessibilityIsIgnoredButIncludedInTree();
    if (is_ignored_changed)
      ChildrenChanged(obj->CachedParentObject());
  }
}

bool AXObjectCacheImpl::IsAriaOwned(const AXObject* object) const {
  return relation_cache_->IsAriaOwned(object);
}

AXObject* AXObjectCacheImpl::GetAriaOwnedParent(const AXObject* object) const {
  return relation_cache_->GetAriaOwnedParent(object);
}

void AXObjectCacheImpl::UpdateAriaOwns(
    const AXObject* owner,
    const Vector<String>& id_vector,
    HeapVector<Member<AXObject>>& owned_children) {
  relation_cache_->UpdateAriaOwns(owner, id_vector, owned_children);
}

void AXObjectCacheImpl::UpdateAriaOwnsFromAttrAssociatedElements(
    const AXObject* owner,
    const HeapVector<Member<Element>>& attr_associated_elements,
    HeapVector<Member<AXObject>>& owned_children) {
  relation_cache_->UpdateAriaOwnsFromAttrAssociatedElements(
      owner, attr_associated_elements, owned_children);
}

bool AXObjectCacheImpl::MayHaveHTMLLabel(const HTMLElement& elem) {
  // Return false if this type of element will not accept a <label for> label.
  if (!elem.IsLabelable())
    return false;

  // Return true if a <label for> pointed to this element at some point.
  if (relation_cache_->MayHaveHTMLLabelViaForAttribute(elem))
    return true;

  // Return true if any amcestor is a label, as in <label><input></label>.
  return Traversal<HTMLLabelElement>::FirstAncestor(elem);
}

void AXObjectCacheImpl::CheckedStateChanged(Node* node) {
  PostNotification(node, ax::mojom::Event::kCheckedStateChanged);
}

void AXObjectCacheImpl::ListboxOptionStateChanged(HTMLOptionElement* option) {
  PostNotification(option, ax::mojom::Event::kCheckedStateChanged);
}

void AXObjectCacheImpl::ListboxSelectedChildrenChanged(
    HTMLSelectElement* select) {
  PostNotification(select, ax::mojom::Event::kSelectedChildrenChanged);
}

void AXObjectCacheImpl::ListboxActiveIndexChanged(HTMLSelectElement* select) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(select->GetDocument());

  auto* ax_object = DynamicTo<AXListBox>(Get(select));
  if (!ax_object)
    return;

  ax_object->ActiveIndexChanged();
}

void AXObjectCacheImpl::LocationChanged(LayoutObject* layout_object) {
  // No need to send this notification if the object is aria-hidden.
  // Note that if the node is ignored for other reasons, it still might
  // be important to send this notification if any of its children are
  // visible - but in the case of aria-hidden we can safely ignore it.
  AXObject* obj = Get(layout_object);
  if (obj && obj->AriaHiddenRoot())
    return;

  PostNotification(layout_object, ax::mojom::Event::kLocationChanged);
}

void AXObjectCacheImpl::RadiobuttonRemovedFromGroup(
    HTMLInputElement* group_member) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(group_member->GetDocument());

  auto* ax_object = DynamicTo<AXRadioInput>(Get(group_member));
  if (!ax_object)
    return;

  // The 'posInSet' and 'setSize' attributes should be updated from the first
  // node, as the removed node is already detached from tree.
  auto* first_radio = ax_object->FindFirstRadioButtonInGroup(group_member);
  AXObject* first_obj = Get(first_radio);
  auto* ax_first_obj = DynamicTo<AXRadioInput>(first_obj);
  if (!ax_first_obj)
    return;

  ax_first_obj->UpdatePosAndSetSize(1);
  PostNotification(first_obj, ax::mojom::Event::kAriaAttributeChanged);
  ax_first_obj->RequestUpdateToNextNode(true);
}

void AXObjectCacheImpl::ImageLoaded(LayoutObject* layout_object) {
  AXObject* obj = Get(layout_object);
  MarkAXObjectDirty(obj, false);
}

void AXObjectCacheImpl::HandleLayoutComplete(LayoutObject* layout_object) {
  if (!layout_object)
    return;

  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(layout_object->GetDocument());

  modification_count_++;

  // Create the AXObject if it didn't yet exist - that's always safe at the
  // end of a layout, and it allows an AX notification to be sent when a page
  // has its first layout, rather than when the document first loads.
  if (AXObject* obj = GetOrCreate(layout_object))
    PostNotification(obj, ax::mojom::Event::kLayoutComplete);
}

void AXObjectCacheImpl::HandleClicked(Node* node) {
  if (AXObject* obj = Get(node))
    PostNotification(obj, ax::mojom::Event::kClicked);
}

void AXObjectCacheImpl::HandleAttributeChanged(
    const QualifiedName& attr_name,
    AccessibleNode* accessible_node) {
  if (!accessible_node)
    return;
  modification_count_++;
  if (AXObject* obj = Get(accessible_node))
    PostNotification(obj, ax::mojom::Event::kAriaAttributeChanged);
}

void AXObjectCacheImpl::HandleAriaExpandedChangeWithCleanLayout(Node* node) {
  if (!node)
    return;

  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(node->GetDocument());

  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));
  if (AXObject* obj = GetOrCreate(node))
    obj->HandleAriaExpandedChanged();
}

void AXObjectCacheImpl::HandleAriaSelectedChangedWithCleanLayout(Node* node) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(node->GetDocument());

  DCHECK(node);
  DCHECK(!document_->NeedsLayoutTreeUpdateForNode(*node));
  AXObject* obj = Get(node);
  if (!obj)
    return;

  PostNotification(obj, ax::mojom::Event::kCheckedStateChanged);

  AXObject* listbox = obj->ParentObjectUnignored();
  if (listbox && listbox->RoleValue() == ax::mojom::Role::kListBox) {
    // Ensure listbox options are in sync as selection status may have changed
    MarkAXObjectDirty(listbox, true);
    PostNotification(listbox, ax::mojom::Event::kSelectedChildrenChanged);
  }
}

void AXObjectCacheImpl::HandleNodeLostFocusWithCleanLayout(Node* node) {
  DCHECK(node);
  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));
  AXObject* obj = Get(node);
  if (!obj)
    return;

  PostNotification(obj, ax::mojom::Event::kBlur);
}

void AXObjectCacheImpl::HandleNodeGainedFocusWithCleanLayout(Node* node) {
  DCHECK(node);
  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));
  // Something about the call chain for this method seems to leave distribution
  // in a dirty state - update it before we call GetOrCreate so that we don't
  // crash.
  node->UpdateDistributionForFlatTreeTraversal();
  AXObject* obj = GetOrCreateFocusedObjectFromNode(node);
  if (!obj)
    return;

  PostNotification(obj, ax::mojom::Event::kFocus);
}

// This might be the new target of a relation. Handle all possible cases.
void AXObjectCacheImpl::MaybeNewRelationTarget(Node* node, AXObject* obj) {
  // Track reverse relations
  relation_cache_->UpdateRelatedTree(node);

  if (!obj)
    return;

  // Check whether aria-activedescendant on a focused object points to |obj|.
  // If so, fire activedescendantchanged event now.
  // This is only for ARIA active descendants, not in a native control like a
  // listbox, which has its own initial active descendant handling.
  Node* focused_node = document_->FocusedElement();
  if (focused_node) {
    AXObject* focus = Get(focused_node);
    if (focus && focus->ActiveDescendant() == obj &&
        obj->CanBeActiveDescendant())
      focus->HandleActiveDescendantChanged();
  }
}

void AXObjectCacheImpl::HandleActiveDescendantChangedWithCleanLayout(
    Node* node) {
  DCHECK(node);
  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));
  // Changing the active descendant should trigger recomputing all
  // cached values even if it doesn't result in a notification, because
  // it can affect what's focusable or not.
  modification_count_++;

  if (AXObject* obj = GetOrCreate(node))
    obj->HandleActiveDescendantChanged();
}

// Be as safe as possible about changes that could alter the accessibility role,
// as this may require a different subclass of AXObject.
// Role changes are disallowed by the spec but we must handle it gracefully, see
// https://www.w3.org/TR/wai-aria-1.1/#h-roles for more information.
void AXObjectCacheImpl::HandleRoleChangeWithCleanLayout(Node* node) {
  if (!node)
    return;  // Virtual AOM node.

  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));

  // Invalidate the current object and make the parent reconsider its children.
  if (AXObject* obj = GetOrCreate(node)) {
    // Save parent for later use.
    AXObject* parent = obj->ParentObject();

    // If role changes on a table, invalidate the entire table subtree as many
    // objects may suddenly need to change, because presentation is inherited
    // from the table to rows and cells.
    // TODO(aleventhal) A size change on a select means the children may need to
    // switch between AXMenuListOption and AXListBoxOption.
    // For some reason we don't get attribute changes for @size, though.
    LayoutObject* layout_object = node->GetLayoutObject();
    if (layout_object && layout_object->IsTable())
      InvalidateTableSubtree(obj);
    else
      Remove(node);

    // Parent object changed children, as the previous AXObject for this node
    // was destroyed and a different one was created in its place.
    if (parent)
      ChildrenChanged(parent, parent->GetNode());
    modification_count_++;
  }
}

void AXObjectCacheImpl::HandleRoleChangeIfNotEditableWithCleanLayout(
    Node* node) {
  if (!node)
    return;

  DCHECK(!node->GetDocument().NeedsLayoutTreeUpdateForNode(*node));

  // Do not invalidate object if the role doesn't actually change when it's a
  // text control, otherwise unique id will change on platform side, and confuse
  // some screen readers as user edits.
  // TODO(aleventhal) Ideally the text control check would be removed, and
  // HandleRoleChangeWithCleanLayout() and only ever invalidate when the role
  // actually changes. For example:
  // if (obj->RoleValue() == obj->ComputeAccessibilityRole())
  //   return;
  // However, doing that would require
  // waiting for layout to complete, as ComputeAccessibilityRole() looks at
  // layout objects.
  if (AXObject* obj = Get(node)) {
    if (!obj->IsTextControl())
      HandleRoleChangeWithCleanLayout(node);
  }
}

void AXObjectCacheImpl::HandleAttributeChanged(const QualifiedName& attr_name,
                                               Element* element) {
  DCHECK(element);
  DeferTreeUpdate(&AXObjectCacheImpl::HandleAttributeChangedWithCleanLayout,
                  attr_name, element);
}

void AXObjectCacheImpl::HandleAttributeChangedWithCleanLayout(
    const QualifiedName& attr_name,
    Element* element) {
  DCHECK(element);
  DCHECK(!element->GetDocument().NeedsLayoutTreeUpdateForNode(*element));
  if (attr_name == html_names::kRoleAttr ||
      attr_name == html_names::kTypeAttr) {
    HandleRoleChangeWithCleanLayout(element);
  } else if (attr_name == html_names::kSizeAttr ||
             attr_name == html_names::kAriaHaspopupAttr) {
    // Role won't change on edits.
    HandleRoleChangeIfNotEditableWithCleanLayout(element);
  } else if (attr_name == html_names::kAltAttr ||
             attr_name == html_names::kTitleAttr) {
    TextChangedWithCleanLayout(element);
  } else if (attr_name == html_names::kForAttr &&
             IsA<HTMLLabelElement>(*element)) {
    LabelChangedWithCleanLayout(element);
  } else if (attr_name == html_names::kIdAttr) {
    MaybeNewRelationTarget(element, Get(element));
  } else if (attr_name == html_names::kTabindexAttr) {
    FocusableChangedWithCleanLayout(element);
  } else if (attr_name == html_names::kDisabledAttr ||
             attr_name == html_names::kReadonlyAttr) {
    MarkElementDirty(element, false);
  } else if (attr_name == html_names::kValueAttr) {
    HandleValueChanged(element);
  } else if (attr_name == html_names::kMinAttr ||
             attr_name == html_names::kMaxAttr) {
    MarkElementDirty(element, false);
  } else if (attr_name == html_names::kStepAttr) {
    MarkElementDirty(element, false);
  }

  if (!attr_name.LocalName().StartsWith("aria-"))
    return;

  // Perform updates specific to each attribute.
  if (attr_name == html_names::kAriaActivedescendantAttr) {
    HandleActiveDescendantChangedWithCleanLayout(element);
  } else if (attr_name == html_names::kAriaValuenowAttr ||
             attr_name == html_names::kAriaValuetextAttr) {
    HandleValueChanged(element);
  } else if (attr_name == html_names::kAriaLabelAttr ||
             attr_name == html_names::kAriaLabeledbyAttr ||
             attr_name == html_names::kAriaLabelledbyAttr) {
    TextChangedWithCleanLayout(element);
  } else if (attr_name == html_names::kAriaDescriptionAttr ||
             attr_name == html_names::kAriaDescribedbyAttr) {
    // TODO do we need a DescriptionChanged() ?
    TextChangedWithCleanLayout(element);
  } else if (attr_name == html_names::kAriaCheckedAttr ||
             attr_name == html_names::kAriaPressedAttr) {
    CheckedStateChanged(element);
  } else if (attr_name == html_names::kAriaSelectedAttr) {
    HandleAriaSelectedChangedWithCleanLayout(element);
  } else if (attr_name == html_names::kAriaExpandedAttr) {
    HandleAriaExpandedChangeWithCleanLayout(element);
  } else if (attr_name == html_names::kAriaHiddenAttr) {
    ChildrenChangedWithCleanLayout(element->parentNode());
  } else if (attr_name == html_names::kAriaInvalidAttr) {
    PostNotification(element, ax::mojom::Event::kInvalidStatusChanged);
  } else if (attr_name == html_names::kAriaErrormessageAttr) {
    MarkElementDirty(element, false);
  } else if (attr_name == html_names::kAriaOwnsAttr) {
    ChildrenChangedWithCleanLayout(element);
    // Ensure aria-owns update fires on original parent as well
    if (AXObject* obj = GetOrCreate(element)) {
      obj->ClearChildren();
      obj->AddChildren();
    }
  } else {
    PostNotification(element, ax::mojom::Event::kAriaAttributeChanged);
  }
}

AXObject* AXObjectCacheImpl::GetOrCreateValidationMessageObject() {
  AXObject* message_ax_object = nullptr;
  // Create only if it does not already exist.
  if (validation_message_axid_) {
    message_ax_object = ObjectFromAXID(validation_message_axid_);
  }
  if (!message_ax_object) {
    message_ax_object = MakeGarbageCollected<AXValidationMessage>(*this);
    DCHECK(message_ax_object);
    // Cache the validation message container for reuse.
    validation_message_axid_ = GetOrCreateAXID(message_ax_object);
    message_ax_object->Init();
    // Validation message alert object is a child of the document, as not all
    // form controls can have a child. Also, there are form controls such as
    // listbox that technically can have children, but they are probably not
    // expected to have alerts within AT client code.
    ChildrenChanged(document_);
  }
  return message_ax_object;
}

AXObject* AXObjectCacheImpl::ValidationMessageObjectIfInvalid() {
  Element* focused_element = document_->FocusedElement();
  if (focused_element) {
    ListedElement* form_control = ListedElement::From(*focused_element);
    if (form_control && !form_control->IsNotCandidateOrValid()) {
      // These must both be true:
      // * Focused control is currently invalid.
      // * Validation message was previously created but hidden
      // from timeout or currently visible.
      bool was_validation_message_already_created = validation_message_axid_;
      if (was_validation_message_already_created ||
          form_control->IsValidationMessageVisible()) {
        AXObject* focused_object = FocusedObject();
        if (focused_object) {
          // Return as long as the focused form control isn't overriding with a
          // different message via aria-errormessage.
          bool override_native_validation_message =
              focused_object->GetAOMPropertyOrARIAAttribute(
                  AOMRelationProperty::kErrorMessage);
          if (!override_native_validation_message) {
            AXObject* message = GetOrCreateValidationMessageObject();
            if (message && !was_validation_message_already_created)
              ChildrenChanged(document_);
            return message;
          }
        }
      }
    }
  }

  // No focused, invalid form control.
  RemoveValidationMessageObject();
  return nullptr;
}

void AXObjectCacheImpl::RemoveValidationMessageObject() {
  if (validation_message_axid_) {
    // Remove when it becomes hidden, so that a new object is created the next
    // time the message becomes visible. It's not possible to reuse the same
    // alert, because the event generator will not generate an alert event if
    // the same object is hidden and made visible quickly, which occurs if the
    // user submits the form when an alert is already visible.
    Remove(validation_message_axid_);
    validation_message_axid_ = 0;
    ChildrenChanged(document_);
  }
}

// Native validation error popup for focused form control in current document.
void AXObjectCacheImpl::HandleValidationMessageVisibilityChanged(
    const Element* form_control) {
  AXObject* message_ax_object = ValidationMessageObjectIfInvalid();
  if (message_ax_object)
    MarkAXObjectDirty(message_ax_object, false);  // May be invisible now.

  // If the form control is invalid, it will now have an error message relation
  // to the message container.
  MarkElementDirty(form_control, false);
}

void AXObjectCacheImpl::LabelChangedWithCleanLayout(Element* element) {
  // Will call back to TextChanged() when done updating relation cache.
  relation_cache_->LabelChanged(element);
}

void AXObjectCacheImpl::InlineTextBoxesUpdated(
    LineLayoutItem line_layout_item) {
  if (!InlineTextBoxAccessibilityEnabled())
    return;

  LayoutObject* layout_object =
      LineLayoutAPIShim::LayoutObjectFrom(line_layout_item);

  // Only update if the accessibility object already exists and it's
  // not already marked as dirty.
  if (AXObject* obj = Get(layout_object)) {
    if (!obj->NeedsToUpdateChildren()) {
      obj->SetNeedsToUpdateChildren();
      PostNotification(layout_object, ax::mojom::Event::kChildrenChanged);
    }
  }
}

Settings* AXObjectCacheImpl::GetSettings() {
  return document_->GetSettings();
}

bool AXObjectCacheImpl::InlineTextBoxAccessibilityEnabled() {
  Settings* settings = this->GetSettings();
  if (!settings)
    return false;
  return settings->GetInlineTextBoxAccessibilityEnabled();
}

const Element* AXObjectCacheImpl::RootAXEditableElement(const Node* node) {
  const Element* result = RootEditableElement(*node);
  const auto* element = DynamicTo<Element>(node);
  if (!element)
    element = node->parentElement();

  for (; element; element = element->parentElement()) {
    if (NodeIsTextControl(element))
      result = element;
  }

  return result;
}

AXObject* AXObjectCacheImpl::FirstAccessibleObjectFromNode(const Node* node) {
  if (!node)
    return nullptr;

  AXObject* accessible_object = GetOrCreate(node->GetLayoutObject());
  while (accessible_object &&
         !accessible_object->AccessibilityIsIncludedInTree()) {
    node = NodeTraversal::Next(*node);

    while (node && !node->GetLayoutObject())
      node = NodeTraversal::NextSkippingChildren(*node);

    if (!node)
      return nullptr;

    accessible_object = GetOrCreate(node->GetLayoutObject());
  }

  return accessible_object;
}

bool AXObjectCacheImpl::NodeIsTextControl(const Node* node) {
  if (!node)
    return false;

  const AXObject* ax_object = GetOrCreate(const_cast<Node*>(node));
  return ax_object && ax_object->IsTextControl();
}

bool IsNodeAriaVisible(Node* node) {
  auto* element = DynamicTo<Element>(node);
  if (!element)
    return false;

  bool is_null = true;
  bool hidden = AccessibleNode::GetPropertyOrARIAAttribute(
      element, AOMBooleanProperty::kHidden, is_null);
  return !is_null && !hidden;
}

void AXObjectCacheImpl::PostPlatformNotification(
    AXObject* obj,
    ax::mojom::blink::Event event_type,
    ax::mojom::blink::EventFrom event_from,
    const BlinkAXEventIntentsSet& event_intents) {
  if (!document_ || !document_->View() ||
      !document_->View()->GetFrame().GetPage()) {
    return;
  }

  WebLocalFrameImpl* web_frame =
      WebLocalFrameImpl::FromFrame(document_->AXObjectCacheOwner().GetFrame());
  if (web_frame && web_frame->Client()) {
    ui::AXEvent event;
    event.id = obj->AXObjectID();
    event.event_type = event_type;
    event.event_from = event_from;
    event.event_intents.resize(event_intents.size());
    // We need to filter out the counts from every intent.
    std::transform(event_intents.begin(), event_intents.end(),
                   event.event_intents.begin(),
                   [](const auto& intent) { return intent.key.intent(); });

    web_frame->Client()->PostAccessibilityEvent(event);
  }
}

void AXObjectCacheImpl::MarkAXObjectDirty(AXObject* obj, bool subtree) {
  if (!obj || !document_ || !document_->View() ||
      !document_->View()->GetFrame().GetPage())
    return;

  WebLocalFrameImpl* webframe =
      WebLocalFrameImpl::FromFrame(document_->AXObjectCacheOwner().GetFrame());
  if (webframe && webframe->Client())
    webframe->Client()->MarkWebAXObjectDirty(WebAXObject(obj), subtree);
}

void AXObjectCacheImpl::MarkElementDirty(const Element* element, bool subtree) {
  // Warning, if no AXObject exists for element, nothing is marked dirty,
  // including descendant objects when subtree == true.
  MarkAXObjectDirty(Get(element), subtree);
}

void AXObjectCacheImpl::HandleFocusedUIElementChanged(
    Element* old_focused_element,
    Element* new_focused_element) {
#if DCHECK_IS_ON()
  // The focus can be in a different document when a popup is open.
  Document& focused_doc =
      new_focused_element ? new_focused_element->GetDocument() : *document_;
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(focused_doc);
#endif  // DCHECK_IS_ON()

  RemoveValidationMessageObject();

  if (!new_focused_element) {
    // When focus is cleared, implicitly focus the document by sending a blur.
    DeferTreeUpdate(&AXObjectCacheImpl::HandleNodeLostFocusWithCleanLayout,
                    GetDocument().documentElement());
    return;
  }

  Page* page = new_focused_element->GetDocument().GetPage();
  if (!page)
    return;

  if (old_focused_element) {
    DeferTreeUpdate(&AXObjectCacheImpl::HandleNodeLostFocusWithCleanLayout,
                    old_focused_element);
  }

  DeferTreeUpdate(&AXObjectCacheImpl::HandleNodeGainedFocusWithCleanLayout,
                  this->FocusedElement());
}

void AXObjectCacheImpl::HandleInitialFocus() {
  PostNotification(document_, ax::mojom::Event::kFocus);
}

void AXObjectCacheImpl::HandleEditableTextContentChanged(Node* node) {
  if (!node)
    return;

  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(node->GetDocument());

  AXObject* obj = nullptr;
  // We shouldn't create a new AX object here because we might be in the middle
  // of a layout.
  do {
    obj = Get(node);
  } while (!obj && (node = node->parentNode()));
  if (!obj)
    return;

  while (obj && !obj->IsNativeTextControl() && !obj->IsNonNativeTextControl())
    obj = obj->ParentObject();
  PostNotification(obj, ax::mojom::Event::kValueChanged);
}

void AXObjectCacheImpl::HandleScaleAndLocationChanged(Document* document) {
  if (!document)
    return;
  PostNotification(document, ax::mojom::Event::kLocationChanged);
}

void AXObjectCacheImpl::HandleTextFormControlChanged(Node* node) {
  HandleEditableTextContentChanged(node);
}

void AXObjectCacheImpl::HandleTextMarkerDataAdded(Node* start, Node* end) {
  if (!start || !end)
    return;

  // Notify the client of new text marker data.
  ChildrenChanged(start);
  if (start != end) {
    ChildrenChanged(end);
  }
}

void AXObjectCacheImpl::HandleValueChanged(Node* node) {
  PostNotification(node, ax::mojom::Event::kValueChanged);
}

void AXObjectCacheImpl::HandleUpdateActiveMenuOption(LayoutObject* menu_list,
                                                     int option_index) {
  auto* ax_object = DynamicTo<AXMenuList>(Get(menu_list));
  if (!ax_object)
    return;

  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(*ax_object->GetDocument());

  ax_object->DidUpdateActiveOption(option_index);
}

void AXObjectCacheImpl::DidShowMenuListPopup(LayoutObject* menu_list) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(menu_list->GetDocument());

  auto* ax_object = DynamicTo<AXMenuList>(Get(menu_list));
  if (ax_object)
    ax_object->DidShowPopup();
}

void AXObjectCacheImpl::DidHideMenuListPopup(LayoutObject* menu_list) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(menu_list->GetDocument());

  auto* ax_object = DynamicTo<AXMenuList>(Get(menu_list));
  if (ax_object)
    ax_object->DidHidePopup();
}

void AXObjectCacheImpl::HandleLoadComplete(Document* document) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(*document);

  PostNotification(GetOrCreate(document), ax::mojom::Event::kLoadComplete);
  AddPermissionStatusListener();
}

void AXObjectCacheImpl::HandleLayoutComplete(Document* document) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(*document);

  PostNotification(GetOrCreate(document), ax::mojom::Event::kLayoutComplete);
}

void AXObjectCacheImpl::HandleScrolledToAnchor(const Node* anchor_node) {
  if (!anchor_node)
    return;

  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(anchor_node->GetDocument());

  AXObject* obj = GetOrCreate(anchor_node->GetLayoutObject());
  if (!obj)
    return;
  if (!obj->AccessibilityIsIncludedInTree())
    obj = obj->ParentObjectUnignored();
  PostNotification(obj, ax::mojom::Event::kScrolledToAnchor);
}

void AXObjectCacheImpl::HandleFrameRectsChanged(Document& document) {
  MarkAXObjectDirty(Get(&document), false);
}

void AXObjectCacheImpl::HandleScrollPositionChanged(
    LocalFrameView* frame_view) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(*frame_view->GetFrame().GetDocument());

  AXObject* target_ax_object = GetOrCreate(document_);
  PostNotification(target_ax_object, ax::mojom::Event::kScrollPositionChanged);
}

void AXObjectCacheImpl::HandleScrollPositionChanged(
    LayoutObject* layout_object) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(layout_object->GetDocument());
  PostNotification(GetOrCreate(layout_object),
                   ax::mojom::Event::kScrollPositionChanged);
}

const AtomicString& AXObjectCacheImpl::ComputedRoleForNode(Node* node) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(node->GetDocument());

  AXObject* obj = GetOrCreate(node);
  if (!obj)
    return AXObject::RoleName(ax::mojom::Role::kUnknown);
  return AXObject::RoleName(obj->RoleValue());
}

String AXObjectCacheImpl::ComputedNameForNode(Node* node) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(node->GetDocument());

  AXObject* obj = GetOrCreate(node);
  if (!obj)
    return "";

  return obj->ComputedName();
}

void AXObjectCacheImpl::OnTouchAccessibilityHover(const IntPoint& location) {
  DocumentLifecycle::DisallowTransitionScope disallow(document_->Lifecycle());
  AXObject* hit = Root()->AccessibilityHitTest(location);
  if (hit) {
    // Ignore events on a frame or plug-in, because the touch events
    // will be re-targeted there and we don't want to fire duplicate
    // accessibility events.
    if (hit->GetLayoutObject() &&
        hit->GetLayoutObject()->IsLayoutEmbeddedContent())
      return;

    PostNotification(hit, ax::mojom::Event::kHover);
  }
}

void AXObjectCacheImpl::SetCanvasObjectBounds(HTMLCanvasElement* canvas,
                                              Element* element,
                                              const LayoutRect& rect) {
  SCOPED_DISALLOW_LIFECYCLE_TRANSITION(element->GetDocument());

  AXObject* obj = GetOrCreate(element);
  if (!obj)
    return;

  AXObject* ax_canvas = GetOrCreate(canvas);
  if (!ax_canvas)
    return;

  obj->SetElementRect(rect, ax_canvas);
}

void AXObjectCacheImpl::AddPermissionStatusListener() {
  if (!document_->GetExecutionContext())
    return;

  // Passing an Origin to Mojo crashes if the host is empty because
  // blink::SecurityOrigin sets unique to false, but url::Origin sets
  // unique to true. This only happens for some obscure corner cases
  // like on Android where the system registers unusual protocol handlers,
  // and we don't need any special permissions in those cases.
  //
  // http://crbug.com/759528 and http://crbug.com/762716
  if (document_->Url().Protocol() != "file" &&
      document_->Url().Host().IsEmpty()) {
    return;
  }

  if (permission_service_.is_bound())
    permission_service_.reset();

  ConnectToPermissionService(
      document_->GetExecutionContext(),
      permission_service_.BindNewPipeAndPassReceiver(
          document_->GetTaskRunner(TaskType::kUserInteraction)));

  if (permission_observer_receiver_.is_bound())
    permission_observer_receiver_.reset();

  mojo::PendingRemote<mojom::blink::PermissionObserver> observer;
  permission_observer_receiver_.Bind(
      observer.InitWithNewPipeAndPassReceiver(),
      document_->GetTaskRunner(TaskType::kUserInteraction));
  permission_service_->AddPermissionObserver(
      CreatePermissionDescriptor(
          mojom::blink::PermissionName::ACCESSIBILITY_EVENTS),
      accessibility_event_permission_, std::move(observer));
}

void AXObjectCacheImpl::OnPermissionStatusChange(
    mojom::PermissionStatus status) {
  accessibility_event_permission_ = status;
}

bool AXObjectCacheImpl::CanCallAOMEventListeners() const {
  return accessibility_event_permission_ == mojom::PermissionStatus::GRANTED;
}

void AXObjectCacheImpl::RequestAOMEventListenerPermission() {
  if (accessibility_event_permission_ != mojom::PermissionStatus::ASK)
    return;

  if (!permission_service_.is_bound())
    return;

  permission_service_->RequestPermission(
      CreatePermissionDescriptor(
          mojom::blink::PermissionName::ACCESSIBILITY_EVENTS),
      LocalFrame::HasTransientUserActivation(document_->GetFrame()),
      WTF::Bind(&AXObjectCacheImpl::OnPermissionStatusChange,
                WrapPersistent(this)));
}

void AXObjectCacheImpl::Trace(Visitor* visitor) {
  visitor->Trace(document_);
  visitor->Trace(accessible_node_mapping_);
  visitor->Trace(node_object_mapping_);

  visitor->Trace(objects_);
  visitor->Trace(notifications_to_post_);
  visitor->Trace(permission_service_);
  visitor->Trace(permission_observer_receiver_);
  visitor->Trace(documents_);
  visitor->Trace(tree_update_callback_queue_);
  AXObjectCache::Trace(visitor);
}

ax::mojom::blink::EventFrom AXObjectCacheImpl::ComputeEventFrom() {
  if (active_event_from_ != ax::mojom::blink::EventFrom::kNone)
    return active_event_from_;

  if (document_ && document_->View() &&
      LocalFrame::HasTransientUserActivation(
          &(document_->View()->GetFrame()))) {
    return ax::mojom::blink::EventFrom::kUser;
  }

  return ax::mojom::blink::EventFrom::kPage;
}

WebAXAutofillState AXObjectCacheImpl::GetAutofillState(AXID id) const {
  auto iter = autofill_state_map_.find(id);
  if (iter == autofill_state_map_.end())
    return WebAXAutofillState::kNoSuggestions;
  return iter->value;
}

void AXObjectCacheImpl::SetAutofillState(AXID id, WebAXAutofillState state) {
  WebAXAutofillState previous_state = GetAutofillState(id);
  if (state != previous_state) {
    autofill_state_map_.Set(id, state);
    MarkAXObjectDirty(ObjectFromAXID(id), false);
  }
}

}  // namespace blink
