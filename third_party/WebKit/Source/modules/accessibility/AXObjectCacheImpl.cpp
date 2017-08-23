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

#include "modules/accessibility/AXObjectCacheImpl.h"

#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/dom/AccessibleNode.h"
#include "core/dom/Document.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/editing/EditingUtilities.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/html/HTMLAreaElement.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLLabelElement.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/HTMLSelectElement.h"
#include "core/layout/LayoutListBox.h"
#include "core/layout/LayoutMenuList.h"
#include "core/layout/LayoutProgress.h"
#include "core/layout/LayoutSlider.h"
#include "core/layout/LayoutTable.h"
#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTableRow.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/layout/line/AbstractInlineTextBox.h"
#include "core/page/ChromeClient.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "modules/accessibility/AXARIAGrid.h"
#include "modules/accessibility/AXARIAGridCell.h"
#include "modules/accessibility/AXARIAGridRow.h"
#include "modules/accessibility/AXImageMapLink.h"
#include "modules/accessibility/AXInlineTextBox.h"
#include "modules/accessibility/AXLayoutObject.h"
#include "modules/accessibility/AXList.h"
#include "modules/accessibility/AXListBox.h"
#include "modules/accessibility/AXListBoxOption.h"
#include "modules/accessibility/AXMediaControls.h"
#include "modules/accessibility/AXMenuList.h"
#include "modules/accessibility/AXMenuListOption.h"
#include "modules/accessibility/AXMenuListPopup.h"
#include "modules/accessibility/AXProgressIndicator.h"
#include "modules/accessibility/AXRadioInput.h"
#include "modules/accessibility/AXSVGRoot.h"
#include "modules/accessibility/AXSlider.h"
#include "modules/accessibility/AXSpinButton.h"
#include "modules/accessibility/AXTable.h"
#include "modules/accessibility/AXTableCell.h"
#include "modules/accessibility/AXTableColumn.h"
#include "modules/accessibility/AXTableHeaderContainer.h"
#include "modules/accessibility/AXTableRow.h"
#include "modules/permissions/PermissionUtils.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/modules/permissions/permission.mojom-blink.h"
#include "public/platform/modules/permissions/permission_status.mojom-blink.h"
#include "public/web/WebFrameClient.h"

namespace blink {

using namespace HTMLNames;

// static
AXObjectCache* AXObjectCacheImpl::Create(Document& document) {
  return new AXObjectCacheImpl(document);
}

AXObjectCacheImpl::AXObjectCacheImpl(Document& document)
    : AXObjectCacheBase(document),
      document_(document),
      modification_count_(0),
      notification_post_timer_(
          TaskRunnerHelper::Get(TaskType::kUnspecedTimer, &document),
          this,
          &AXObjectCacheImpl::NotificationPostTimerFired),
      accessibility_event_permission_(mojom::PermissionStatus::ASK),
      permission_observer_binding_(this) {
  if (document_->LoadEventFinished())
    AddPermissionStatusListener();
}

AXObjectCacheImpl::~AXObjectCacheImpl() {
#if DCHECK_IS_ON()
  DCHECK(has_been_disposed_);
#endif
}

void AXObjectCacheImpl::Dispose() {
  notification_post_timer_.Stop();

  for (auto& entry : objects_) {
    AXObject* obj = entry.value;
    obj->Detach();
    RemoveAXID(obj);
  }

#if DCHECK_IS_ON()
  has_been_disposed_ = true;
#endif
}

AXObject* AXObjectCacheImpl::Root() {
  return GetOrCreate(document_);
}

AXObject* AXObjectCacheImpl::FocusedImageMapUIElement(
    HTMLAreaElement* area_element) {
  // Find the corresponding accessibility object for the HTMLAreaElement. This
  // should be in the list of children for its corresponding image.
  if (!area_element)
    return 0;

  HTMLImageElement* image_element = area_element->ImageElement();
  if (!image_element)
    return 0;

  AXObject* ax_layout_image = GetOrCreate(image_element);
  if (!ax_layout_image)
    return 0;

  const AXObject::AXObjectVector& image_children = ax_layout_image->Children();
  unsigned count = image_children.size();
  for (unsigned k = 0; k < count; ++k) {
    AXObject* child = image_children[k];
    if (!child->IsImageMapLink())
      continue;

    if (ToAXImageMapLink(child)->AreaElement() == area_element)
      return child;
  }

  return 0;
}

AXObject* AXObjectCacheImpl::FocusedObject() {
  if (!AccessibilityEnabled())
    return nullptr;

  Node* focused_node = document_->FocusedElement();
  if (!focused_node)
    focused_node = document_;

  // If it's an image map, get the focused link within the image map.
  if (isHTMLAreaElement(focused_node))
    return FocusedImageMapUIElement(toHTMLAreaElement(focused_node));

  // See if there's a page popup, for example a calendar picker.
  Element* adjusted_focused_element = document_->AdjustedFocusedElement();
  if (isHTMLInputElement(adjusted_focused_element)) {
    if (AXObject* ax_popup =
            toHTMLInputElement(adjusted_focused_element)->PopupRootAXObject()) {
      if (Element* focused_element_in_popup =
              ax_popup->GetDocument()->FocusedElement())
        focused_node = focused_element_in_popup;
    }
  }

  AXObject* obj = GetOrCreate(focused_node);
  if (!obj)
    return nullptr;

  // the HTML element, for example, is focusable but has an AX object that is
  // ignored
  if (obj->AccessibilityIsIgnored())
    obj = obj->ParentObjectUnignored();

  return obj;
}

AXObject* AXObjectCacheImpl::Get(LayoutObject* layout_object) {
  if (!layout_object)
    return 0;

  AXID ax_id = layout_object_mapping_.at(layout_object);
  DCHECK(!HashTraits<AXID>::IsDeletedValue(ax_id));
  if (!ax_id)
    return 0;

  return objects_.at(ax_id);
}

// Returns true if |node| is an <option> element and its parent <select>
// is a menu list (not a list box).
static bool IsMenuListOption(const Node* node) {
  if (!isHTMLOptionElement(node))
    return false;
  const HTMLSelectElement* select =
      toHTMLOptionElement(node)->OwnerSelectElement();
  if (!select)
    return false;
  const LayoutObject* layout_object = select->GetLayoutObject();
  return layout_object && layout_object->IsMenuList();
}

AXObject* AXObjectCacheImpl::Get(const Node* node) {
  if (!node)
    return 0;

  // Menu list option and HTML area elements are indexed by DOM node, never by
  // layout object.
  LayoutObject* layout_object = node->GetLayoutObject();
  if (IsMenuListOption(node) || isHTMLAreaElement(node))
    layout_object = nullptr;

  AXID layout_id = layout_object ? layout_object_mapping_.at(layout_object) : 0;
  DCHECK(!HashTraits<AXID>::IsDeletedValue(layout_id));

  AXID node_id = node_object_mapping_.at(node);
  DCHECK(!HashTraits<AXID>::IsDeletedValue(node_id));

  if (layout_object && node_id && !layout_id) {
    // This can happen if an AXNodeObject is created for a node that's not
    // laid out, but later something changes and it gets a layoutObject (like if
    // it's reparented).
    Remove(node_id);
    return 0;
  }

  if (layout_id)
    return objects_.at(layout_id);

  if (!node_id)
    return 0;

  return objects_.at(node_id);
}

AXObject* AXObjectCacheImpl::Get(AbstractInlineTextBox* inline_text_box) {
  if (!inline_text_box)
    return 0;

  AXID ax_id = inline_text_box_object_mapping_.at(inline_text_box);
  DCHECK(!HashTraits<AXID>::IsDeletedValue(ax_id));
  if (!ax_id)
    return 0;

  return objects_.at(ax_id);
}

// FIXME: This probably belongs on Node.
// FIXME: This should take a const char*, but one caller passes nullAtom.
bool NodeHasRole(Node* node, const String& role) {
  if (!node || !node->IsElementNode())
    return false;

  return EqualIgnoringASCIICase(ToElement(node)->getAttribute(roleAttr), role);
}

AXObject* AXObjectCacheImpl::CreateFromRenderer(LayoutObject* layout_object) {
  // FIXME: How could layoutObject->node() ever not be an Element?
  Node* node = layout_object->GetNode();

  // If the node is aria role="list" or the aria role is empty and its a
  // ul/ol/dl type (it shouldn't be a list if aria says otherwise).
  if (NodeHasRole(node, "list") || NodeHasRole(node, "directory") ||
      (NodeHasRole(node, g_null_atom) &&
       (isHTMLUListElement(node) || isHTMLOListElement(node) ||
        isHTMLDListElement(node))))
    return AXList::Create(layout_object, *this);

  // aria tables
  if (NodeHasRole(node, "grid") || NodeHasRole(node, "treegrid"))
    return AXARIAGrid::Create(layout_object, *this);
  if (NodeHasRole(node, "row"))
    return AXARIAGridRow::Create(layout_object, *this);
  if (NodeHasRole(node, "gridcell") || NodeHasRole(node, "columnheader") ||
      NodeHasRole(node, "rowheader"))
    return AXARIAGridCell::Create(layout_object, *this);

  // media controls
  if (node && node->IsMediaControlElement())
    return AccessibilityMediaControl::Create(layout_object, *this);

  if (isHTMLOptionElement(node))
    return AXListBoxOption::Create(layout_object, *this);

  if (isHTMLInputElement(node) &&
      toHTMLInputElement(node)->type() == InputTypeNames::radio)
    return AXRadioInput::Create(layout_object, *this);

  if (layout_object->IsSVGRoot())
    return AXSVGRoot::Create(layout_object, *this);

  if (layout_object->IsBoxModelObject()) {
    LayoutBoxModelObject* css_box = ToLayoutBoxModelObject(layout_object);
    if (css_box->IsListBox())
      return AXListBox::Create(ToLayoutListBox(css_box), *this);
    if (css_box->IsMenuList())
      return AXMenuList::Create(ToLayoutMenuList(css_box), *this);

    // standard tables
    if (css_box->IsTable())
      return AXTable::Create(ToLayoutTable(css_box), *this);
    if (css_box->IsTableRow())
      return AXTableRow::Create(ToLayoutTableRow(css_box), *this);
    if (css_box->IsTableCell())
      return AXTableCell::Create(ToLayoutTableCell(css_box), *this);

    // progress bar
    if (css_box->IsProgress())
      return AXProgressIndicator::Create(ToLayoutProgress(css_box), *this);

    // input type=range
    if (css_box->IsSlider())
      return AXSlider::Create(ToLayoutSlider(css_box), *this);
  }

  return AXLayoutObject::Create(layout_object, *this);
}

AXObject* AXObjectCacheImpl::CreateFromNode(Node* node) {
  if (IsMenuListOption(node))
    return AXMenuListOption::Create(toHTMLOptionElement(node), *this);

  if (isHTMLAreaElement(node))
    return AXImageMapLink::Create(toHTMLAreaElement(node), *this);

  return AXNodeObject::Create(node, *this);
}

AXObject* AXObjectCacheImpl::CreateFromInlineTextBox(
    AbstractInlineTextBox* inline_text_box) {
  return AXInlineTextBox::Create(inline_text_box, *this);
}

AXObject* AXObjectCacheImpl::GetOrCreate(Node* node) {
  if (!node)
    return 0;

  if (!node->IsElementNode() && !node->IsTextNode() && !node->IsDocumentNode())
    return 0;  // Only documents, elements and text nodes get a11y objects

  if (AXObject* obj = Get(node))
    return obj;

  // If the node has a layout object, prefer using that as the primary key for
  // the AXObject, with the exception of an HTMLAreaElement, which is
  // created based on its node.
  if (node->GetLayoutObject() && !isHTMLAreaElement(node))
    return GetOrCreate(node->GetLayoutObject());

  if (!node->parentElement())
    return 0;

  if (isHTMLHeadElement(node))
    return 0;

  AXObject* new_obj = CreateFromNode(node);

  // Will crash later if we have two objects for the same node.
  DCHECK(!Get(node));

  const AXID ax_id = GetOrCreateAXID(new_obj);

  node_object_mapping_.Set(node, ax_id);
  new_obj->Init();
  new_obj->SetLastKnownIsIgnoredValue(new_obj->AccessibilityIsIgnored());

  if (node->IsElementNode())
    UpdateTreeIfElementIdIsAriaOwned(ToElement(node));

  return new_obj;
}

AXObject* AXObjectCacheImpl::GetOrCreate(LayoutObject* layout_object) {
  if (!layout_object)
    return 0;

  if (AXObject* obj = Get(layout_object))
    return obj;

  AXObject* new_obj = CreateFromRenderer(layout_object);

  // Will crash later if we have two objects for the same layoutObject.
  DCHECK(!Get(layout_object));

  const AXID axid = GetOrCreateAXID(new_obj);

  layout_object_mapping_.Set(layout_object, axid);
  new_obj->Init();
  new_obj->SetLastKnownIsIgnoredValue(new_obj->AccessibilityIsIgnored());

  return new_obj;
}

AXObject* AXObjectCacheImpl::GetOrCreate(
    AbstractInlineTextBox* inline_text_box) {
  if (!inline_text_box)
    return 0;

  if (AXObject* obj = Get(inline_text_box))
    return obj;

  AXObject* new_obj = CreateFromInlineTextBox(inline_text_box);

  // Will crash later if we have two objects for the same inlineTextBox.
  DCHECK(!Get(inline_text_box));

  const AXID axid = GetOrCreateAXID(new_obj);

  inline_text_box_object_mapping_.Set(inline_text_box, axid);
  new_obj->Init();
  new_obj->SetLastKnownIsIgnoredValue(new_obj->AccessibilityIsIgnored());

  return new_obj;
}

AXObject* AXObjectCacheImpl::GetOrCreate(AccessibilityRole role) {
  AXObject* obj = nullptr;

  // will be filled in...
  switch (role) {
    case kColumnRole:
      obj = AXTableColumn::Create(*this);
      break;
    case kTableHeaderContainerRole:
      obj = AXTableHeaderContainer::Create(*this);
      break;
    case kSliderThumbRole:
      obj = AXSliderThumb::Create(*this);
      break;
    case kMenuListPopupRole:
      obj = AXMenuListPopup::Create(*this);
      break;
    case kSpinButtonRole:
      obj = AXSpinButton::Create(*this);
      break;
    case kSpinButtonPartRole:
      obj = AXSpinButtonPart::Create(*this);
      break;
    default:
      obj = nullptr;
  }

  if (!obj)
    return 0;

  GetOrCreateAXID(obj);

  obj->Init();
  return obj;
}

void AXObjectCacheImpl::Remove(AXID ax_id) {
  if (!ax_id)
    return;

  // first fetch object to operate some cleanup functions on it
  AXObject* obj = objects_.at(ax_id);
  if (!obj)
    return;

  obj->Detach();
  RemoveAXID(obj);

  // finally remove the object
  if (!objects_.Take(ax_id))
    return;

  DCHECK_GE(objects_.size(), ids_in_use_.size());
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

  if (node->GetLayoutObject()) {
    Remove(node->GetLayoutObject());
    return;
  }
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
  const AXID existing_axid = obj->AxObjectID();
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

  AXID obj_id = object->AxObjectID();
  if (!obj_id)
    return;
  DCHECK(!HashTraits<AXID>::IsDeletedValue(obj_id));
  DCHECK(ids_in_use_.Contains(obj_id));
  object->SetAXObjectID(0);
  ids_in_use_.erase(obj_id);

  if (aria_owner_to_children_mapping_.Contains(obj_id)) {
    Vector<AXID> child_axi_ds = aria_owner_to_children_mapping_.at(obj_id);
    for (size_t i = 0; i < child_axi_ds.size(); ++i)
      aria_owned_child_to_owner_mapping_.erase(child_axi_ds[i]);
    aria_owner_to_children_mapping_.erase(obj_id);
  }
  aria_owned_child_to_owner_mapping_.erase(obj_id);
  aria_owned_child_to_real_parent_mapping_.erase(obj_id);
  aria_owner_to_ids_mapping_.erase(obj_id);
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

void AXObjectCacheImpl::SelectionChanged(Node* node) {
  AXObject* nearestAncestor = NearestExistingAncestor(node);
  if (nearestAncestor)
    nearestAncestor->SelectionChanged();
}

void AXObjectCacheImpl::TextChanged(Node* node) {
  TextChanged(Get(node));
}

void AXObjectCacheImpl::TextChanged(LayoutObject* layout_object) {
  TextChanged(Get(layout_object));
}

void AXObjectCacheImpl::TextChanged(AXObject* obj) {
  if (!obj)
    return;

  obj->TextChanged();
  PostNotification(obj, AXObjectCacheImpl::kAXTextChanged);
}

void AXObjectCacheImpl::UpdateCacheAfterNodeIsAttached(Node* node) {
  // Calling get() will update the AX object if we had an AXNodeObject but now
  // we need an AXLayoutObject, because it was reparented to a location outside
  // of a canvas.
  Get(node);
  if (node->IsElementNode())
    UpdateTreeIfElementIdIsAriaOwned(ToElement(node));
}

void AXObjectCacheImpl::ChildrenChanged(Node* node) {
  ChildrenChanged(Get(node));
}

void AXObjectCacheImpl::ChildrenChanged(LayoutObject* layout_object) {
  ChildrenChanged(Get(layout_object));
}

void AXObjectCacheImpl::ChildrenChanged(AXObject* obj) {
  if (!obj)
    return;

  obj->ChildrenChanged();
}

void AXObjectCacheImpl::NotificationPostTimerFired(TimerBase*) {
  notification_post_timer_.Stop();

  unsigned i = 0, count = notifications_to_post_.size();
  for (i = 0; i < count; ++i) {
    AXObject* obj = notifications_to_post_[i].first;

    if (!obj->AxObjectID())
      continue;

    if (obj->IsDetached())
      continue;

#if DCHECK_IS_ON()
    // Make sure none of the layout views are in the process of being layed out.
    // Notifications should only be sent after the layoutObject has finished
    if (obj->IsAXLayoutObject()) {
      AXLayoutObject* layout_obj = ToAXLayoutObject(obj);
      LayoutObject* layout_object = layout_obj->GetLayoutObject();
      if (layout_object && layout_object->View())
        DCHECK(!layout_object->View()->GetLayoutState());
    }
#endif

    AXNotification notification = notifications_to_post_[i].second;
    PostPlatformNotification(obj, notification);

    if (notification == kAXChildrenChanged && obj->ParentObjectIfExists() &&
        obj->LastKnownIsIgnoredValue() != obj->AccessibilityIsIgnored())
      ChildrenChanged(obj->ParentObject());
  }

  notifications_to_post_.clear();
}

void AXObjectCacheImpl::PostNotification(LayoutObject* layout_object,
                                         AXNotification notification) {
  if (!layout_object)
    return;
  PostNotification(Get(layout_object), notification);
}

void AXObjectCacheImpl::PostNotification(Node* node,
                                         AXNotification notification) {
  if (!node)
    return;
  PostNotification(Get(node), notification);
}

void AXObjectCacheImpl::PostNotification(AXObject* object,
                                         AXNotification notification) {
  if (!object)
    return;

  modification_count_++;
  notifications_to_post_.push_back(std::make_pair(object, notification));
  if (!notification_post_timer_.IsActive())
    notification_post_timer_.StartOneShot(0, BLINK_FROM_HERE);
}

bool AXObjectCacheImpl::IsAriaOwned(const AXObject* child) const {
  return aria_owned_child_to_owner_mapping_.Contains(child->AxObjectID());
}

AXObject* AXObjectCacheImpl::GetAriaOwnedParent(const AXObject* child) const {
  return ObjectFromAXID(
      aria_owned_child_to_owner_mapping_.at(child->AxObjectID()));
}

void AXObjectCacheImpl::UpdateAriaOwns(
    const AXObject* owner,
    const Vector<String>& id_vector,
    HeapVector<Member<AXObject>>& owned_children) {
  //
  // Update the map from the AXID of this element to the ids of the owned
  // children, and the reverse map from ids to possible AXID owners.
  //

  HashSet<String> current_ids =
      aria_owner_to_ids_mapping_.at(owner->AxObjectID());
  HashSet<String> new_ids;
  bool ids_changed = false;
  for (const String& id : id_vector) {
    new_ids.insert(id);
    if (!current_ids.Contains(id)) {
      ids_changed = true;
      HashSet<AXID>* owners = id_to_aria_owners_mapping_.at(id);
      if (!owners) {
        owners = new HashSet<AXID>();
        id_to_aria_owners_mapping_.Set(id, WTF::WrapUnique(owners));
      }
      owners->insert(owner->AxObjectID());
    }
  }
  for (const String& id : current_ids) {
    if (!new_ids.Contains(id)) {
      ids_changed = true;
      HashSet<AXID>* owners = id_to_aria_owners_mapping_.at(id);
      if (owners) {
        owners->erase(owner->AxObjectID());
        if (owners->IsEmpty())
          id_to_aria_owners_mapping_.erase(id);
      }
    }
  }
  if (ids_changed)
    aria_owner_to_ids_mapping_.Set(owner->AxObjectID(), new_ids);

  //
  // Now figure out the ids that actually correspond to children that exist and
  // that we can legally own (not cyclical, not already owned, etc.) and update
  // the maps and |ownedChildren| based on that.
  //

  // Figure out the children that are owned by this object and are in the tree.
  TreeScope& scope = owner->GetNode()->GetTreeScope();
  Vector<AXID> new_child_axi_ds;
  for (const String& id_name : id_vector) {
    Element* element = scope.getElementById(AtomicString(id_name));
    if (!element)
      continue;

    AXObject* child = GetOrCreate(element);
    if (!child)
      continue;

    // If this child is already aria-owned by a different owner, continue.
    // It's an author error if this happens and we don't worry about which of
    // the two owners wins ownership of the child, as long as only one of them
    // does.
    if (IsAriaOwned(child) && GetAriaOwnedParent(child) != owner)
      continue;

    // You can't own yourself!
    if (child == owner)
      continue;

    // Walk up the parents of the owner object, make sure that this child
    // doesn't appear there, as that would create a cycle.
    bool found_cycle = false;
    for (AXObject* parent = owner->ParentObject(); parent && !found_cycle;
         parent = parent->ParentObject()) {
      if (parent == child)
        found_cycle = true;
    }
    if (found_cycle)
      continue;

    new_child_axi_ds.push_back(child->AxObjectID());
    owned_children.push_back(child);
  }

  // Compare this to the current list of owned children, and exit early if there
  // are no changes.
  Vector<AXID> current_child_axi_ds =
      aria_owner_to_children_mapping_.at(owner->AxObjectID());
  bool same = true;
  if (current_child_axi_ds.size() != new_child_axi_ds.size()) {
    same = false;
  } else {
    for (size_t i = 0; i < current_child_axi_ds.size() && same; ++i) {
      if (current_child_axi_ds[i] != new_child_axi_ds[i])
        same = false;
    }
  }
  if (same)
    return;

  // The list of owned children has changed. Even if they were just reordered,
  // to be safe and handle all cases we remove all of the current owned children
  // and add the new list of owned children.
  for (size_t i = 0; i < current_child_axi_ds.size(); ++i) {
    // Find the AXObject for the child that this owner no longer owns.
    AXID removed_child_id = current_child_axi_ds[i];
    AXObject* removed_child = ObjectFromAXID(removed_child_id);

    // It's possible that this child has already been owned by some other owner,
    // in which case we don't need to do anything.
    if (removed_child && GetAriaOwnedParent(removed_child) != owner)
      continue;

    // Remove it from the child -> owner mapping so it's not owned by this owner
    // anymore.
    aria_owned_child_to_owner_mapping_.erase(removed_child_id);

    if (removed_child) {
      // If the child still exists, find its "real" parent, and reparent it back
      // to its real parent in the tree by detaching it from its current parent
      // and calling childrenChanged on its real parent.
      removed_child->DetachFromParent();
      AXID real_parent_id =
          aria_owned_child_to_real_parent_mapping_.at(removed_child_id);
      AXObject* real_parent = ObjectFromAXID(real_parent_id);
      ChildrenChanged(real_parent);
    }

    // Remove the child -> original parent mapping too since this object has now
    // been reparented back to its original parent.
    aria_owned_child_to_real_parent_mapping_.erase(removed_child_id);
  }

  for (size_t i = 0; i < new_child_axi_ds.size(); ++i) {
    // Find the AXObject for the child that will now be a child of this
    // owner.
    AXID added_child_id = new_child_axi_ds[i];
    AXObject* added_child = ObjectFromAXID(added_child_id);

    // Add this child to the mapping from child to owner.
    aria_owned_child_to_owner_mapping_.Set(added_child_id, owner->AxObjectID());

    // Add its parent object to a mapping from child to real parent. If later
    // this owner doesn't own this child anymore, we need to return it to its
    // original parent.
    AXObject* original_parent = added_child->ParentObject();
    aria_owned_child_to_real_parent_mapping_.Set(added_child_id,
                                                 original_parent->AxObjectID());

    // Now detach the object from its original parent and call childrenChanged
    // on the original parent so that it can recompute its list of children.
    added_child->DetachFromParent();
    ChildrenChanged(original_parent);
  }

  // Finally, update the mapping from the owner to the list of child IDs.
  aria_owner_to_children_mapping_.Set(owner->AxObjectID(), new_child_axi_ds);
}

void AXObjectCacheImpl::UpdateTreeIfElementIdIsAriaOwned(Element* element) {
  if (!element->HasID())
    return;

  String id = element->GetIdAttribute();
  HashSet<AXID>* owners = id_to_aria_owners_mapping_.at(id);
  if (!owners)
    return;

  AXObject* ax_element = GetOrCreate(element);
  if (!ax_element)
    return;

  // If it's already owned, call childrenChanged on the owner to make sure it's
  // still an owner.
  if (IsAriaOwned(ax_element)) {
    AXObject* owned_parent = GetAriaOwnedParent(ax_element);
    DCHECK(owned_parent);
    ChildrenChanged(owned_parent);
    return;
  }

  // If it's not already owned, check the possible owners based on our mapping
  // from ids to elements that have that id listed in their aria-owns attribute.
  for (const auto& ax_id : *owners) {
    AXObject* owner = ObjectFromAXID(ax_id);
    if (owner)
      ChildrenChanged(owner);
  }
}

void AXObjectCacheImpl::CheckedStateChanged(Node* node) {
  PostNotification(node, AXObjectCacheImpl::kAXCheckedStateChanged);
}

void AXObjectCacheImpl::ListboxOptionStateChanged(HTMLOptionElement* option) {
  PostNotification(option, kAXCheckedStateChanged);
}

void AXObjectCacheImpl::ListboxSelectedChildrenChanged(
    HTMLSelectElement* select) {
  PostNotification(select, kAXSelectedChildrenChanged);
}

void AXObjectCacheImpl::ListboxActiveIndexChanged(HTMLSelectElement* select) {
  AXObject* obj = Get(select);
  if (!obj || !obj->IsAXListBox())
    return;

  ToAXListBox(obj)->ActiveIndexChanged();
}

void AXObjectCacheImpl::RadiobuttonRemovedFromGroup(
    HTMLInputElement* group_member) {
  AXObject* obj = Get(group_member);
  if (!obj || !obj->IsAXRadioInput())
    return;

  // The 'posInSet' and 'setSize' attributes should be updated from the first
  // node, as the removed node is already detached from tree.
  HTMLInputElement* first_radio =
      ToAXRadioInput(obj)->FindFirstRadioButtonInGroup(group_member);
  AXObject* first_obj = Get(first_radio);
  if (!first_obj || !first_obj->IsAXRadioInput())
    return;

  ToAXRadioInput(first_obj)->UpdatePosAndSetSize(1);
  PostNotification(first_obj, kAXAriaAttributeChanged);
  ToAXRadioInput(first_obj)->RequestUpdateToNextNode(true);
}

void AXObjectCacheImpl::HandleLayoutComplete(LayoutObject* layout_object) {
  if (!layout_object)
    return;

  modification_count_++;

  // Create the AXObject if it didn't yet exist - that's always safe at the
  // end of a layout, and it allows an AX notification to be sent when a page
  // has its first layout, rather than when the document first loads.
  if (AXObject* obj = GetOrCreate(layout_object))
    PostNotification(obj, kAXLayoutComplete);
}

void AXObjectCacheImpl::HandleClicked(Node* node) {
  if (AXObject* obj = GetOrCreate(node))
    PostNotification(obj, kAXClicked);
}

void AXObjectCacheImpl::HandleAriaExpandedChange(Node* node) {
  if (AXObject* obj = GetOrCreate(node))
    obj->HandleAriaExpandedChanged();
}

void AXObjectCacheImpl::HandleAriaSelectedChanged(Node* node) {
  AXObject* obj = Get(node);
  if (!obj)
    return;

  PostNotification(obj, kAXCheckedStateChanged);

  AXObject* listbox = obj->ParentObjectUnignored();
  if (listbox && listbox->RoleValue() == kListBoxRole)
    PostNotification(listbox, kAXSelectedChildrenChanged);
}

void AXObjectCacheImpl::HandleActiveDescendantChanged(Node* node) {
  // Changing the active descendant should trigger recomputing all
  // cached values even if it doesn't result in a notification, because
  // it can affect what's focusable or not.
  modification_count_++;

  if (AXObject* obj = GetOrCreate(node))
    obj->HandleActiveDescendantChanged();
}

void AXObjectCacheImpl::HandleAriaRoleChanged(Node* node) {
  if (AXObject* obj = GetOrCreate(node)) {
    obj->UpdateAccessibilityRole();
    modification_count_++;
    obj->NotifyIfIgnoredValueChanged();
  }
}

void AXObjectCacheImpl::HandleAttributeChanged(const QualifiedName& attr_name,
                                               Element* element) {
  if (attr_name == roleAttr)
    HandleAriaRoleChanged(element);
  else if (attr_name == altAttr || attr_name == titleAttr)
    TextChanged(element);
  else if (attr_name == forAttr && isHTMLLabelElement(*element))
    LabelChanged(element);
  else if (attr_name == idAttr)
    UpdateTreeIfElementIdIsAriaOwned(element);

  if (!attr_name.LocalName().StartsWith("aria-"))
    return;

  if (attr_name == aria_activedescendantAttr)
    HandleActiveDescendantChanged(element);
  else if (attr_name == aria_valuenowAttr || attr_name == aria_valuetextAttr)
    PostNotification(element, AXObjectCacheImpl::kAXValueChanged);
  else if (attr_name == aria_labelAttr || attr_name == aria_labeledbyAttr ||
           attr_name == aria_labelledbyAttr)
    TextChanged(element);
  else if (attr_name == aria_checkedAttr || attr_name == aria_pressedAttr)
    CheckedStateChanged(element);
  else if (attr_name == aria_selectedAttr)
    HandleAriaSelectedChanged(element);
  else if (attr_name == aria_expandedAttr)
    HandleAriaExpandedChange(element);
  else if (attr_name == aria_hiddenAttr)
    ChildrenChanged(element->parentNode());
  else if (attr_name == aria_invalidAttr)
    PostNotification(element, AXObjectCacheImpl::kAXInvalidStatusChanged);
  else if (attr_name == aria_ownsAttr)
    ChildrenChanged(element);
  else
    PostNotification(element, AXObjectCacheImpl::kAXAriaAttributeChanged);
}

void AXObjectCacheImpl::LabelChanged(Element* element) {
  TextChanged(toHTMLLabelElement(element)->control());
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
      PostNotification(layout_object, kAXChildrenChanged);
    }
  }
}

Settings* AXObjectCacheImpl::GetSettings() {
  return document_->GetSettings();
}

bool AXObjectCacheImpl::AccessibilityEnabled() {
  Settings* settings = this->GetSettings();
  if (!settings)
    return false;
  return settings->GetAccessibilityEnabled();
}

bool AXObjectCacheImpl::InlineTextBoxAccessibilityEnabled() {
  Settings* settings = this->GetSettings();
  if (!settings)
    return false;
  return settings->GetInlineTextBoxAccessibilityEnabled();
}

const Element* AXObjectCacheImpl::RootAXEditableElement(const Node* node) {
  const Element* result = RootEditableElement(*node);
  const Element* element =
      node->IsElementNode() ? ToElement(node) : node->parentElement();

  for (; element; element = element->parentElement()) {
    if (NodeIsTextControl(element))
      result = element;
  }

  return result;
}

AXObject* AXObjectCacheImpl::FirstAccessibleObjectFromNode(const Node* node) {
  if (!node)
    return 0;

  AXObject* accessible_object = GetOrCreate(node->GetLayoutObject());
  while (accessible_object && accessible_object->AccessibilityIsIgnored()) {
    node = NodeTraversal::Next(*node);

    while (node && !node->GetLayoutObject())
      node = NodeTraversal::NextSkippingChildren(*node);

    if (!node)
      return 0;

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
  if (!node)
    return false;

  if (!node->IsElementNode())
    return false;

  bool is_null = true;
  bool hidden = AccessibleNode::GetPropertyOrARIAAttribute(
      ToElement(node), AOMBooleanProperty::kHidden, is_null);
  return !is_null && !hidden;
}

void AXObjectCacheImpl::PostPlatformNotification(AXObject* obj,
                                                 AXNotification notification) {
  if (!obj || !obj->GetDocument() || !obj->DocumentFrameView() ||
      !obj->DocumentFrameView()->GetFrame().GetPage())
    return;
  // Send via WebFrameClient
  WebLocalFrameImpl* webframe = WebLocalFrameImpl::FromFrame(
      obj->GetDocument()->AxObjectCacheOwner().GetFrame());
  if (webframe && webframe->Client()) {
    webframe->Client()->PostAccessibilityEvent(
        WebAXObject(obj), static_cast<WebAXEvent>(notification));
  }
}

void AXObjectCacheImpl::HandleFocusedUIElementChanged(Node* old_focused_node,
                                                      Node* new_focused_node) {
  if (!new_focused_node)
    return;

  Page* page = new_focused_node->GetDocument().GetPage();
  if (!page)
    return;

  AXObject* focused_object = this->FocusedObject();
  if (!focused_object)
    return;

  AXObject* old_focused_object = Get(old_focused_node);

  PostPlatformNotification(old_focused_object, kAXBlur);
  PostPlatformNotification(focused_object, kAXFocusedUIElementChanged);
}

void AXObjectCacheImpl::HandleInitialFocus() {
  PostNotification(document_, AXObjectCache::kAXFocusedUIElementChanged);
}

void AXObjectCacheImpl::HandleEditableTextContentChanged(Node* node) {
  if (!node)
    return;

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
  PostNotification(obj, AXObjectCache::kAXValueChanged);
}

void AXObjectCacheImpl::HandleTextFormControlChanged(Node* node) {
  HandleEditableTextContentChanged(node);
}

void AXObjectCacheImpl::HandleTextMarkerDataAdded(Node* start, Node* end) {
  AXObject* start_object = Get(start);
  AXObject* end_object = Get(end);
  if (!start_object || !end_object)
    return;

  // Notify the client of new text marker data.
  PostNotification(start_object, kAXChildrenChanged);
  if (start_object != end_object) {
    PostNotification(end_object, kAXChildrenChanged);
  }
}

void AXObjectCacheImpl::HandleValueChanged(Node* node) {
  PostNotification(node, AXObjectCache::kAXValueChanged);
}

void AXObjectCacheImpl::HandleUpdateActiveMenuOption(LayoutMenuList* menu_list,
                                                     int option_index) {
  AXObject* obj = GetOrCreate(menu_list);
  if (!obj || !obj->IsMenuList())
    return;

  ToAXMenuList(obj)->DidUpdateActiveOption(option_index);
}

void AXObjectCacheImpl::DidShowMenuListPopup(LayoutMenuList* menu_list) {
  AXObject* obj = Get(menu_list);
  if (!obj || !obj->IsMenuList())
    return;

  ToAXMenuList(obj)->DidShowPopup();
}

void AXObjectCacheImpl::DidHideMenuListPopup(LayoutMenuList* menu_list) {
  AXObject* obj = Get(menu_list);
  if (!obj || !obj->IsMenuList())
    return;

  ToAXMenuList(obj)->DidHidePopup();
}

void AXObjectCacheImpl::HandleLoadComplete(Document* document) {
  PostNotification(GetOrCreate(document), AXObjectCache::kAXLoadComplete);
  AddPermissionStatusListener();
}

void AXObjectCacheImpl::HandleLayoutComplete(Document* document) {
  PostNotification(GetOrCreate(document), AXObjectCache::kAXLayoutComplete);
}

void AXObjectCacheImpl::HandleScrolledToAnchor(const Node* anchor_node) {
  if (!anchor_node)
    return;
  AXObject* obj = GetOrCreate(anchor_node->GetLayoutObject());
  if (!obj)
    return;
  if (obj->AccessibilityIsIgnored())
    obj = obj->ParentObjectUnignored();
  PostPlatformNotification(obj, kAXScrolledToAnchor);
}

void AXObjectCacheImpl::HandleScrollPositionChanged(
    LocalFrameView* frame_view) {
  AXObject* target_ax_object =
      GetOrCreate(frame_view->GetFrame().GetDocument());
  PostPlatformNotification(target_ax_object, kAXScrollPositionChanged);
}

void AXObjectCacheImpl::HandleScrollPositionChanged(
    LayoutObject* layout_object) {
  PostPlatformNotification(GetOrCreate(layout_object),
                           kAXScrollPositionChanged);
}

const AtomicString& AXObjectCacheImpl::ComputedRoleForNode(Node* node) {
  AXObject* obj = GetOrCreate(node);
  if (!obj)
    return AXObject::RoleName(kUnknownRole);
  return AXObject::RoleName(obj->RoleValue());
}

String AXObjectCacheImpl::ComputedNameForNode(Node* node) {
  AXObject* obj = GetOrCreate(node);
  if (!obj)
    return "";

  return obj->ComputedName();
}

void AXObjectCacheImpl::OnTouchAccessibilityHover(const IntPoint& location) {
  AXObject* hit = Root()->AccessibilityHitTest(location);
  if (hit) {
    // Ignore events on a frame or plug-in, because the touch events
    // will be re-targeted there and we don't want to fire duplicate
    // accessibility events.
    if (hit->GetLayoutObject() &&
        hit->GetLayoutObject()->IsLayoutEmbeddedContent())
      return;

    PostPlatformNotification(hit, kAXHover);
  }
}

void AXObjectCacheImpl::SetCanvasObjectBounds(HTMLCanvasElement* canvas,
                                              Element* element,
                                              const LayoutRect& rect) {
  AXObject* obj = GetOrCreate(element);
  if (!obj)
    return;

  AXObject* ax_canvas = GetOrCreate(canvas);
  if (!ax_canvas)
    return;

  obj->SetElementRect(rect, ax_canvas);
}

void AXObjectCacheImpl::AddPermissionStatusListener() {
  ConnectToPermissionService(document_->GetExecutionContext(),
                             mojo::MakeRequest(&permission_service_));

  if (permission_observer_binding_.is_bound())
    permission_observer_binding_.Close();

  mojom::blink::PermissionObserverPtr observer;
  permission_observer_binding_.Bind(mojo::MakeRequest(&observer));
  permission_service_->AddPermissionObserver(
      CreatePermissionDescriptor(
          mojom::blink::PermissionName::ACCESSIBILITY_EVENTS),
      document_->GetExecutionContext()->GetSecurityOrigin(),
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

  permission_service_->RequestPermission(
      CreatePermissionDescriptor(
          mojom::blink::PermissionName::ACCESSIBILITY_EVENTS),
      document_->GetExecutionContext()->GetSecurityOrigin(),
      UserGestureIndicator::ProcessingUserGesture(),
      ConvertToBaseCallback(WTF::Bind(
          &AXObjectCacheImpl::OnPermissionStatusChange, WrapPersistent(this))));
}

void AXObjectCacheImpl::ContextDestroyed(ExecutionContext*) {
  permission_service_.reset();
  permission_observer_binding_.Close();
}

DEFINE_TRACE(AXObjectCacheImpl) {
  visitor->Trace(document_);
  visitor->Trace(node_object_mapping_);

  visitor->Trace(objects_);
  visitor->Trace(notifications_to_post_);

  AXObjectCache::Trace(visitor);
}

}  // namespace blink
