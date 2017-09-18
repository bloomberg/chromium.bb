/*
 * Copyright (C) 2017, Google Inc. All rights reserved.
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

#include "modules/accessibility/AXRelationCache.h"

namespace blink {

AXRelationCache::AXRelationCache(AXObjectCacheImpl* object_cache)
    : object_cache_(object_cache) {}

AXRelationCache::~AXRelationCache() {}

bool AXRelationCache::IsAriaOwned(const AXObject* child) const {
  return aria_owned_child_to_owner_mapping_.Contains(child->AxObjectID());
}

AXObject* AXRelationCache::GetAriaOwnedParent(const AXObject* child) const {
  return ObjectFromAXID(
      aria_owned_child_to_owner_mapping_.at(child->AxObjectID()));
}

// relation_source is related to add_ids, and no longer related to remove_ids
static void UpdateReverseRelationMap(
    HashMap<String, std::unique_ptr<HashSet<AXID>>>& reverse_map,
    AXID relation_source,
    HashSet<String>& add_ids,
    HashSet<String>& remove_ids) {
  // Add entries to reverse map.
  // Vector<String> id_vector;
  // CopyToVector(add_ids, id_vector);
  for (const String& id : add_ids) {
    HashSet<AXID>* axids = reverse_map.at(id);
    if (!axids) {
      axids = new HashSet<AXID>();
      reverse_map.Set(id, WTF::WrapUnique(axids));
    }
    axids->insert(relation_source);
  }

  // Remove entries from reverse map that are no longer needed.
  for (const String& id : remove_ids) {
    HashSet<AXID>* axids = reverse_map.at(id);
    if (axids) {
      axids->erase(relation_source);
      if (axids->IsEmpty())
        reverse_map.erase(id);
    }
  }
}

static HashSet<String> CopyToSet(const Vector<String>& source) {
  HashSet<String> dest;
  for (const String& entry : source)
    dest.insert(entry);
  return dest;
}

static HashSet<String> SubtractSet(const HashSet<String>& set1,
                                   const HashSet<String>& set2) {
  HashSet<String> dest(set1);
  dest.RemoveAll(set2);
  return dest;
}

static bool ContainsCycle(AXObject* owner, AXObject* child) {
  // Walk up the parents of the owner object, make sure that this child
  // doesn't appear there, as that would create a cycle.
  for (AXObject* parent = owner; parent; parent = parent->ParentObject()) {
    if (parent == child)
      return true;
    ;
  }
  return false;
}

bool AXRelationCache::IsValidOwnsRelation(AXObject* owner,
                                          AXObject* child) const {
  if (!child)
    return false;

  // If this child is already aria-owned by a different owner, continue.
  // It's an author error if this happens and we don't worry about which of
  // the two owners wins ownership of the child, as long as only one of them
  // does.
  if (IsAriaOwned(child) && GetAriaOwnedParent(child) != owner)
    return false;

  // You can't own yourself or an ancestor!
  if (ContainsCycle(owner, child))
    return false;

  return true;
}

void AXRelationCache::UnmapOwnedChildren(const AXObject* owner,
                                         const Vector<AXID> child_ids) {
  for (size_t i = 0; i < child_ids.size(); ++i) {
    // Find the AXObject for the child that this owner no longer owns.
    AXID removed_child_id = child_ids[i];
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
      AXID real_parent_id =
          aria_owned_child_to_real_parent_mapping_.at(removed_child_id);
      AXObject* real_parent = ObjectFromAXID(real_parent_id);
      ChildrenChanged(real_parent);
    }

    // Remove the child -> original parent mapping too since this object has
    // now been reparented back to its original parent.
    aria_owned_child_to_real_parent_mapping_.erase(removed_child_id);
  }
}

void AXRelationCache::MapOwnedChildren(const AXObject* owner,
                                       const Vector<AXID> child_ids) {
  for (size_t i = 0; i < child_ids.size(); ++i) {
    // Find the AXObject for the child that will now be a child of this
    // owner.
    AXID added_child_id = child_ids[i];
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
}

void AXRelationCache::UpdateAriaOwns(
    const AXObject* owner,
    const Vector<String>& owned_id_vector,
    HeapVector<Member<AXObject>>& validated_owned_children_result) {
  //
  // Update the map from the AXID of this element to the ids of the owned
  // children, and the reverse map from ids to possible AXID owners.
  //

  HashSet<String> current_owned_ids(
      aria_owner_to_ids_mapping_.at(owner->AxObjectID()));
  HashSet<String> all_new_owned_ids(CopyToSet(owned_id_vector));
  HashSet<String> add_ids(SubtractSet(all_new_owned_ids, current_owned_ids));
  HashSet<String> remove_ids(SubtractSet(current_owned_ids, all_new_owned_ids));

  // Update the maps if necessary (aria_owner_to_ids_mapping_).
  if (!add_ids.IsEmpty() || !remove_ids.IsEmpty()) {
    // Update reverse map.
    UpdateReverseRelationMap(id_to_aria_owners_mapping_, owner->AxObjectID(),
                             add_ids, remove_ids);
    // Update forward map.
    aria_owner_to_ids_mapping_.Set(owner->AxObjectID(), all_new_owned_ids);
  }

  //
  // Now figure out the ids that actually correspond to children that exist
  // and that we can legally own (not cyclical, not already owned, etc.) and
  // update the maps and |validated_owned_children_result| based on that.
  //
  // Figure out the children that are owned by this object and are in the
  // tree.
  TreeScope& scope = owner->GetNode()->GetTreeScope();
  Vector<AXID> validated_owned_child_axids;
  for (const String& id_name : owned_id_vector) {
    Element* element = scope.getElementById(AtomicString(id_name));
    AXObject* child = GetOrCreate(element);
    if (IsValidOwnsRelation(const_cast<AXObject*>(owner), child)) {
      validated_owned_child_axids.push_back(child->AxObjectID());
      validated_owned_children_result.push_back(child);
    }
  }

  // Compare this to the current list of owned children, and exit early if
  // there are no changes.
  Vector<AXID> current_child_axids =
      aria_owner_to_children_mapping_.at(owner->AxObjectID());
  if (current_child_axids == validated_owned_child_axids)
    return;

  // The list of owned children has changed. Even if they were just reordered,
  // to be safe and handle all cases we remove all of the current owned
  // children and add the new list of owned children.
  UnmapOwnedChildren(owner, current_child_axids);
  MapOwnedChildren(owner, validated_owned_child_axids);

  // Finally, update the mapping from the owner to the list of child IDs.
  aria_owner_to_children_mapping_.Set(owner->AxObjectID(),
                                      validated_owned_child_axids);
}

void AXRelationCache::UpdateTreeIfElementIdIsAriaOwned(Element* element) {
  if (!element->HasID())
    return;

  String id = element->GetIdAttribute();
  HashSet<AXID>* owners = id_to_aria_owners_mapping_.at(id);
  if (!owners)
    return;

  AXObject* ax_element = GetOrCreate(element);
  if (!ax_element)
    return;

  // If it's already owned, call childrenChanged on the owner to make sure
  // it's still an owner.
  if (IsAriaOwned(ax_element)) {
    AXObject* owned_parent = GetAriaOwnedParent(ax_element);
    DCHECK(owned_parent);
    ChildrenChanged(owned_parent);
    return;
  }

  // If it's not already owned, check the possible owners based on our mapping
  // from ids to elements that have that id listed in their aria-owns
  // attribute.
  for (const auto& ax_id : *owners) {
    AXObject* owner = ObjectFromAXID(ax_id);
    if (owner)
      ChildrenChanged(owner);
  }
}

void AXRelationCache::RemoveAXID(AXID obj_id) {
  if (aria_owner_to_children_mapping_.Contains(obj_id)) {
    Vector<AXID> child_axids = aria_owner_to_children_mapping_.at(obj_id);
    for (size_t i = 0; i < child_axids.size(); ++i)
      aria_owned_child_to_owner_mapping_.erase(child_axids[i]);
    aria_owner_to_children_mapping_.erase(obj_id);
  }
  aria_owned_child_to_owner_mapping_.erase(obj_id);
  aria_owned_child_to_real_parent_mapping_.erase(obj_id);
  aria_owner_to_ids_mapping_.erase(obj_id);
}

AXObject* AXRelationCache::ObjectFromAXID(AXID axid) const {
  return object_cache_->ObjectFromAXID(axid);
}

AXObject* AXRelationCache::GetOrCreate(Node* node) {
  return object_cache_->GetOrCreate(node);
}

void AXRelationCache::ChildrenChanged(AXObject* object) {
  object_cache_->ChildrenChanged(object);
}

}  // namespace blink
