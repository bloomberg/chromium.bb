// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_relation_cache.h"

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"

namespace blink {

AXRelationCache::AXRelationCache(AXObjectCacheImpl* object_cache)
    : object_cache_(object_cache) {}

AXRelationCache::~AXRelationCache() = default;

void AXRelationCache::DoInitialDocumentScan() {
  // Init the relation cache with elements already in the document.
  Document& document = object_cache_->GetDocument();
  for (Element& element :
       ElementTraversal::DescendantsOf(*document.documentElement())) {
    const auto& id = element.FastGetAttribute(html_names::kForAttr);
    if (!id.IsEmpty())
      all_previously_seen_label_target_ids_.insert(id);

    // Ensure correct ancestor chains even when not all AXObject's in the
    // document are created, e.g. in the devtools accessibility panel.
    // Defers adding aria-owns targets as children of their new parents,
    // and to the relation cache, until the appropriate document lifecycle.
#if DCHECK_IS_ON()
    DCHECK(document.Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean)
        << "Unclean document at lifecycle " << document.Lifecycle().ToString();
#endif
    if (element.FastHasAttribute(html_names::kAriaOwnsAttr)) {
      if (AXObject* owner = GetOrCreate(&element, nullptr)) {
        owner_ids_to_update_.insert(owner->AXObjectID());
      }
    }
  }

  initialized_ = true;
}

void AXRelationCache::ProcessUpdatesWithCleanLayout() {
  if (!initialized_)
    DoInitialDocumentScan();

  for (AXID aria_owns_obj_id : owner_ids_to_update_) {
    AXObject* obj = ObjectFromAXID(aria_owns_obj_id);
    if (obj)
      UpdateAriaOwnsWithCleanLayout(obj);
  }

  owner_ids_to_update_.clear();
}

bool AXRelationCache::IsAriaOwned(const AXObject* child) const {
  return child &&
         aria_owned_child_to_owner_mapping_.Contains(child->AXObjectID());
}

AXObject* AXRelationCache::GetAriaOwnedParent(const AXObject* child) const {
  // Child IDs may still be present in owning parents whose list of children
  // have been marked as requiring an update, but have not been updated yet.
  HashMap<AXID, AXID>::const_iterator iter =
      aria_owned_child_to_owner_mapping_.find(child->AXObjectID());
  if (iter == aria_owned_child_to_owner_mapping_.end())
    return nullptr;
  return ObjectFromAXID(iter->value);
}

// Update reverse relation map, where relation_source is related to target_ids.
void AXRelationCache::UpdateReverseRelations(const AXObject* relation_source,
                                             const Vector<String>& target_ids) {
  AXID relation_source_axid = relation_source->AXObjectID();

  // Add entries to reverse map.
  for (const String& target_id : target_ids) {
    auto result =
        id_attr_to_related_mapping_.insert(target_id, HashSet<AXID>());
    result.stored_value->value.insert(relation_source_axid);
  }
}

static bool ContainsCycle(AXObject* owner, AXObject* child) {
  // Walk up the parents of the owner object, make sure that this child
  // doesn't appear there, as that would create a cycle.
  for (AXObject* parent = owner; parent; parent = parent->ParentObject()) {
    if (parent == child)
      return true;
  }
  return false;
}

bool AXRelationCache::IsValidOwnsRelation(AXObject* owner,
                                          AXObject* child) const {
  if (!IsValidOwner(owner) || !IsValidOwnedChild(child))
    return false;

  // If this child is already aria-owned by a different owner, continue.
  // It's an author error if this happens and we don't worry about which of
  // the two owners wins ownership, as long as only one of them does.
  if (IsAriaOwned(child) && GetAriaOwnedParent(child) != owner)
    return false;

  // You can't own yourself or an ancestor!
  if (ContainsCycle(owner, child))
    return false;

  return true;
}

bool AXRelationCache::IsValidOwner(AXObject* owner) {
  if (!owner->GetNode()) {
    NOTREACHED() << "Cannot use aria-owns without a node on both ends";
    return false;
  }

  // Can't have children.
  if (!owner->CanHaveChildren())
    return false;

  // No aria-owns in editable controlsm otherwise wreaks havoc.
  if (owner->IsNativeTextControl() || owner->HasContentEditableAttributeSet())
    return false;

  // Image maps can only use <img usemap> to "own" <area> children.
  // This requires special parenting logic, and aria-owns is prevented here in
  // order to keep things from getting too complex.
  if (owner->RoleValue() == ax::mojom::blink::Role::kImageMap)
    return false;

  // Similarly, do not allow <area> to own another object.
  if (owner->IsImageMapLink())
    return false;

  return true;
}

bool AXRelationCache::IsValidOwnedChild(AXObject* child) {
  if (!child)
    return false;

  if (!child->GetNode()) {
    NOTREACHED() << "Cannot use aria-owns without a node on both ends";
    return false;
  }

  if (child->IsImageMapLink())
    return false;  // An area can't be owned, only parented by <img usemap>.

  return true;
}

void AXRelationCache::UnmapOwnedChildren(const AXObject* owner,
                                         const Vector<AXID> child_ids) {
  for (AXID removed_child_id : child_ids) {
    // Find the AXObject for the child that this owner no longer owns.
    AXObject* removed_child = ObjectFromAXID(removed_child_id);

    // It's possible that this child has already been owned by some other
    // owner, in which case we don't need to do anything.
    if (removed_child && GetAriaOwnedParent(removed_child) != owner)
      continue;

    // Remove it from the child -> owner mapping so it's not owned by this
    // owner anymore.
    aria_owned_child_to_owner_mapping_.erase(removed_child_id);

    if (removed_child) {
      // If the child still exists, find its "real" parent, and reparent it
      // back to its real parent in the tree by detaching it from its current
      // parent and calling childrenChanged on its real parent.
      removed_child->DetachFromParent();
      // Recompute the real parent and cache it.
      AXObject* real_parent = removed_child->ParentObject();
      ChildrenChanged(real_parent);
    }
  }
}

void AXRelationCache::MapOwnedChildren(const AXObject* owner,
                                       const Vector<AXID> child_ids) {
  for (AXID added_child_id : child_ids) {
    AXObject* added_child = ObjectFromAXID(added_child_id);

    // Add this child to the mapping from child to owner.
    aria_owned_child_to_owner_mapping_.Set(added_child_id, owner->AXObjectID());

    // Now detach the object from its original parent and call childrenChanged
    // on the original parent so that it can recompute its list of children.
    AXObject* original_parent = added_child->ParentObject();
    added_child->DetachFromParent();
    added_child->SetParent(const_cast<AXObject*>(owner));
    ChildrenChanged(original_parent);
  }
}

void AXRelationCache::UpdateAriaOwnsFromAttrAssociatedElementsWithCleanLayout(
    AXObject* owner,
    const HeapVector<Member<Element>>& attr_associated_elements,
    HeapVector<Member<AXObject>>& validated_owned_children_result) {
  // attr-associated elements have already had their scope validated, but they
  // need to be further validated to determine if they introduce a cycle or are
  // already owned by another element.

  Vector<String> owned_id_vector;
  for (const auto& element : attr_associated_elements) {
    // Pass in owner parent assuming that the owns relationship will be valid.
    // It will be cleared below if the owns relationship is found to be invalid.
    AXObject* child = GetOrCreate(element, owner);

    // TODO(meredithl): Determine how to update reverse relations for elements
    // without an id.
    if (element->GetIdAttribute())
      owned_id_vector.push_back(element->GetIdAttribute());
    if (IsValidOwnsRelation(const_cast<AXObject*>(owner), child)) {
      validated_owned_children_result.push_back(child);
    } else if (child) {
      // Invalid owns relation: repair the parent that was set above.
      child->SetParent(child->ComputeParentImpl());
    }
  }

  // Track reverse relations for future tree updates.
  UpdateReverseRelations(owner, owned_id_vector);

  // Update the internal mappings of owned children.
  UpdateAriaOwnerToChildrenMappingWithCleanLayout(
      owner, validated_owned_children_result);
}

void AXRelationCache::GetAriaOwnedChildren(
    const AXObject* owner,
    HeapVector<Member<AXObject>>& validated_owned_children_result) {
  if (!aria_owner_to_children_mapping_.Contains(owner->AXObjectID()))
    return;
  Vector<AXID> current_child_axids =
      aria_owner_to_children_mapping_.at(owner->AXObjectID());
  for (AXID child_id : current_child_axids) {
    AXObject* child = ObjectFromAXID(child_id);
    if (child) {
      validated_owned_children_result.push_back(child);
      DCHECK(IsAriaOwned(child)) << "Owned child not in owned child map";
    }
  }
}

void AXRelationCache::UpdateAriaOwnsWithCleanLayout(AXObject* owner) {
  Element* element = owner->GetElement();
  if (!element)
    return;

  DCHECK(!element->GetDocument().NeedsLayoutTreeUpdateForNode(*element));

  Vector<String> owned_id_vector;
  owner->TokenVectorFromAttribute(owned_id_vector, html_names::kAriaOwnsAttr);

  // Track reverse relations for future tree updates.
  UpdateReverseRelations(owner, owned_id_vector);

  // We first check if the element has an explicitly set aria-owns association.
  // Explicitly set elements are validated on setting time (that they are in a
  // valid scope etc). The content attribute can contain ids that are not
  // legally ownable.
  HeapVector<Member<AXObject>> owned_children;
  if (element && element->HasExplicitlySetAttrAssociatedElements(
                     html_names::kAriaOwnsAttr)) {
    UpdateAriaOwnsFromAttrAssociatedElementsWithCleanLayout(
        owner,
        element->GetElementArrayAttribute(html_names::kAriaOwnsAttr).value(),
        owned_children);
  } else {
    // Figure out the ids that actually correspond to children that exist
    // and that we can legally own (not cyclical, not already owned, etc.) and
    // update the maps and |validated_owned_children_result| based on that.
    //
    // Figure out the children that are owned by this object and are in the
    // tree.
    TreeScope& scope = element->GetTreeScope();
    Vector<AXID> validated_owned_child_axids;
    for (const String& id_name : owned_id_vector) {
      Element* child_element = scope.getElementById(AtomicString(id_name));
      // Pass in owner parent assuming that the owns relationship will be valid.
      // It will be cleared below if the owns relationship is found to be
      // invalid.
      AXObject* child = GetOrCreate(child_element, owner);
      if (IsValidOwnsRelation(const_cast<AXObject*>(owner), child)) {
        owned_children.push_back(child);
      } else if (child) {
        // Invalid owns relation: repair the parent that was set above.
        child->SetParent(child->ComputeParentImpl());
      }
    }
  }

  // Update the internal validated mapping of owned children. This will
  // fire an event if the mapping has changed.
  UpdateAriaOwnerToChildrenMappingWithCleanLayout(owner, owned_children);
}

void AXRelationCache::UpdateAriaOwnerToChildrenMappingWithCleanLayout(
    AXObject* owner,
    HeapVector<Member<AXObject>>& validated_owned_children_result) {
  Vector<AXID> validated_owned_child_axids;
  for (auto& child : validated_owned_children_result)
    validated_owned_child_axids.push_back(child->AXObjectID());

  // Compare this to the current list of owned children, and exit early if
  // there are no changes.
  Vector<AXID> current_child_axids =
      aria_owner_to_children_mapping_.at(owner->AXObjectID());
  if (current_child_axids == validated_owned_child_axids)
    return;

  // The list of owned children has changed. Even if they were just reordered,
  // to be safe and handle all cases we remove all of the current owned
  // children and add the new list of owned children.
  UnmapOwnedChildren(owner, current_child_axids);
  MapOwnedChildren(owner, validated_owned_child_axids);

#if DCHECK_IS_ON()
  // Owned children must be in tree to avoid serialization issues.
  for (AXObject* child : validated_owned_children_result) {
    DCHECK(child->AccessibilityIsIncludedInTree())
        << "Owned child not in tree: " << child->ToString(true, false)
        << "\nRecompute included in tree: "
        << child->ComputeAccessibilityIsIgnoredButIncludedInTree();
  }
#endif

  // Finally, update the mapping from the owner to the list of child IDs.
  aria_owner_to_children_mapping_.Set(owner->AXObjectID(),
                                      validated_owned_child_axids);

  ChildrenChanged(owner);
  owner->UpdateChildrenIfNecessary();
}

bool AXRelationCache::MayHaveHTMLLabelViaForAttribute(
    const HTMLElement& labelable) {
  const AtomicString& id = labelable.GetIdAttribute();
  if (id.IsEmpty())
    return false;
  return all_previously_seen_label_target_ids_.Contains(id);
}

// Fill source_objects with AXObjects for relations pointing to target.
void AXRelationCache::GetReverseRelated(
    Node* target,
    HeapVector<Member<AXObject>>& source_objects) {
  auto* element = DynamicTo<Element>(target);
  if (!element)
    return;

  if (!element->HasID())
    return;

  auto it = id_attr_to_related_mapping_.find(element->GetIdAttribute());
  if (it == id_attr_to_related_mapping_.end())
    return;

  for (const auto& source_axid : it->value) {
    AXObject* source_object = ObjectFromAXID(source_axid);
    if (source_object)
      source_objects.push_back(source_object);
  }
}

void AXRelationCache::UpdateRelatedTree(Node* node, AXObject* obj) {
  HeapVector<Member<AXObject>> related_sources;
#if DCHECK_IS_ON()
  DCHECK(node);
  AXObject* obj_for_node = Get(node);
  DCHECK(!obj || obj_for_node == obj)
      << "Object and node did not match:"
      << "\n* node = " << node << "\n* obj = " << obj->ToString(true, true)
      << "\n* obj_for_node = "
      << (obj_for_node ? obj_for_node->ToString(true, true) : "null");
#endif
  AXObject* related_target = obj ? obj : Get(node);
  // If it's already owned, schedule an update on the owner.
  if (related_target && IsAriaOwned(related_target)) {
    AXObject* owned_parent = GetAriaOwnedParent(related_target);
    DCHECK(owned_parent);
    owner_ids_to_update_.insert(owned_parent->AXObjectID());
  }

  // Ensure children are updated if there is a change.
  GetReverseRelated(node, related_sources);
  for (AXObject* related : related_sources) {
    if (related) {
      owner_ids_to_update_.insert(related->AXObjectID());
      ChildrenChanged(related);
    }
  }

  UpdateRelatedText(node);
}

void AXRelationCache::UpdateRelatedText(Node* node) {
  // Walk up ancestor chain from node and refresh text of any related content.
  // TODO(crbug.com/1109265): It's very likely this loop should only walk the
  // unignored AXObject chain, but doing so breaks a number of tests related to
  // name or description computation / invalidation.
  for (Node* current_node = node; current_node;
       current_node = current_node->parentNode()) {
    // Reverse relations via aria-labelledby, aria-describedby, aria-owns.
    HeapVector<Member<AXObject>> related_sources;
    GetReverseRelated(current_node, related_sources);
    for (AXObject* related : related_sources) {
      if (related)
        object_cache_->MarkAXObjectDirty(related, /*subtree=*/false);
    }

    // Ancestors that may derive their accessible name from descendant content
    // should also handle text changed events when descendant content changes.
    if (current_node != node) {
      AXObject* obj = Get(current_node);
      if (obj && obj->SupportsNameFromContents(/*recursive=*/false))
        object_cache_->MarkAXObjectDirty(obj, /*subtree=*/false);
    }

    // Forward relation via <label for="[id]">.
    if (IsA<HTMLLabelElement>(*current_node))
      LabelChanged(current_node);
  }
}

void AXRelationCache::RemoveAXID(AXID obj_id) {
  // Need to remove from maps.
  // There are maps from children to their owners, and owners to their children.
  // In addition, the removed id may be an owner, or be owned, or both.

  // |obj_id| owned others:
  if (aria_owner_to_children_mapping_.Contains(obj_id)) {
    // |obj_id| longer owns anything.
    Vector<AXID> child_axids = aria_owner_to_children_mapping_.at(obj_id);
    aria_owned_child_to_owner_mapping_.RemoveAll(child_axids);
    // Owned children are no longer owned by |obj_id|
    aria_owner_to_children_mapping_.erase(obj_id);
  }

  // Another id owned |obj_id|:
  if (aria_owned_child_to_owner_mapping_.Contains(obj_id)) {
    // Previous owner no longer relevant to this child.
    // Also, remove |obj_id| from previous owner's owned child list:
    AXID owner_id = aria_owned_child_to_owner_mapping_.Take(obj_id);
    const Vector<AXID>& owners_owned_children =
        aria_owner_to_children_mapping_.at(owner_id);
    for (wtf_size_t index = 0; index < owners_owned_children.size(); index++) {
      if (owners_owned_children[index] == obj_id) {
        aria_owner_to_children_mapping_.at(owner_id).EraseAt(index);
        break;
      }
    }
  }
}

AXObject* AXRelationCache::ObjectFromAXID(AXID axid) const {
  return object_cache_->ObjectFromAXID(axid);
}

AXObject* AXRelationCache::Get(Node* node) {
  return object_cache_->Get(node);
}

AXObject* AXRelationCache::GetOrCreate(Node* node, const AXObject* owner) {
  return object_cache_->GetOrCreate(node, const_cast<AXObject*>(owner));
}

void AXRelationCache::ChildrenChanged(AXObject* object) {
  object->ChildrenChanged();
}

void AXRelationCache::LabelChanged(Node* node) {
  const auto& id =
      To<HTMLElement>(node)->FastGetAttribute(html_names::kForAttr);
  if (!id.IsEmpty()) {
    all_previously_seen_label_target_ids_.insert(id);
    if (auto* control = To<HTMLLabelElement>(node)->control()) {
      if (AXObject* obj = Get(control))
        object_cache_->MarkAXObjectDirty(obj, /*subtree=*/false);
    }
  }
}

}  // namespace blink
