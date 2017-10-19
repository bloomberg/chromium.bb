// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree.h"

#include <stddef.h>

#include <set>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "ui/accessibility/ax_node.h"
#include "ui/gfx/transform.h"

namespace ui {

namespace {

std::string TreeToStringHelper(AXNode* node, int indent) {
  std::string result = std::string(2 * indent, ' ');
  result += node->data().ToString() + "\n";
  for (int i = 0; i < node->child_count(); ++i)
    result += TreeToStringHelper(node->ChildAtIndex(i), indent + 1);
  return result;
}

template <typename K, typename V>
bool KeyValuePairsKeysMatch(std::vector<std::pair<K, V>> pairs1,
                            std::vector<std::pair<K, V>> pairs2) {
  if (pairs1.size() != pairs2.size())
    return false;
  for (size_t i = 0; i < pairs1.size(); ++i) {
    if (pairs1[i].first != pairs2[i].first)
      return false;
  }
  return true;
}

template <typename K, typename V>
std::map<K, V> MapFromKeyValuePairs(std::vector<std::pair<K, V>> pairs) {
  std::map<K, V> result;
  for (size_t i = 0; i < pairs.size(); ++i)
    result[pairs[i].first] = pairs[i].second;
  return result;
}

// Given two vectors of <K, V> key, value pairs representing an "old" vs "new"
// state, or "before" vs "after", calls a callback function for each key that
// changed value.
template <typename K, typename V, typename F>
void CallIfAttributeValuesChanged(const std::vector<std::pair<K, V>>& pairs1,
                                  const std::vector<std::pair<K, V>>& pairs2,
                                  const V& empty_value,
                                  F callback) {
  // Fast path - if they both have the same keys in the same order.
  if (KeyValuePairsKeysMatch(pairs1, pairs2)) {
    for (size_t i = 0; i < pairs1.size(); ++i) {
      if (pairs1[i].second != pairs2[i].second)
        callback(pairs1[i].first, pairs1[i].second, pairs2[i].second);
    }
    return;
  }

  // Slower path - they don't have the same keys in the same order, so
  // check all keys against each other, using maps to prevent this from
  // becoming O(n^2) as the size grows.
  auto map1 = MapFromKeyValuePairs(pairs1);
  auto map2 = MapFromKeyValuePairs(pairs2);
  for (size_t i = 0; i < pairs1.size(); ++i) {
    const auto& new_iter = map2.find(pairs1[i].first);
    if (pairs1[i].second != empty_value && new_iter == map2.end())
      callback(pairs1[i].first, pairs1[i].second, empty_value);
  }

  for (size_t i = 0; i < pairs2.size(); ++i) {
    const auto& iter = map1.find(pairs2[i].first);
    if (iter == map1.end())
      callback(pairs2[i].first, empty_value, pairs2[i].second);
    else if (iter->second != pairs2[i].second)
      callback(pairs2[i].first, iter->second, pairs2[i].second);
  }
}

}  // namespace

// Intermediate state to keep track of during a tree update.
struct AXTreeUpdateState {
  AXTreeUpdateState() : new_root(nullptr) {}
  // Returns whether this update changes |node|.
  bool HasChangedNode(const AXNode* node) {
    return changed_node_ids.find(node->id()) != changed_node_ids.end();
  }

  // Returns whether this update removes |node|.
  bool HasRemovedNode(const AXNode* node) {
    return removed_node_ids.find(node->id()) != removed_node_ids.end();
  }

  // During an update, this keeps track of all nodes that have been
  // implicitly referenced as part of this update, but haven't been
  // updated yet. It's an error if there are any pending nodes at the
  // end of Unserialize.
  std::set<AXNode*> pending_nodes;

  // This is similar to above, but we store node ids here because this list gets
  // generated before any nodes get created or re-used. Its purpose is to allow
  // us to know what nodes will be updated so we can make more intelligent
  // decisions about when to notify delegates of removals or reparenting.
  std::set<int> changed_node_ids;

  // Keeps track of new nodes created during this update.
  std::set<AXNode*> new_nodes;

  // The new root in this update, if any.
  AXNode* new_root;

  // Keeps track of any nodes removed. Used to identify re-parented nodes.
  std::set<int> removed_node_ids;
};

AXTreeDelegate::AXTreeDelegate() {
}

AXTreeDelegate::~AXTreeDelegate() {
}

AXTree::AXTree()
    : delegate_(NULL), root_(NULL) {
  AXNodeData root;
  root.id = -1;

  AXTreeUpdate initial_state;
  initial_state.root_id = -1;
  initial_state.nodes.push_back(root);
  CHECK(Unserialize(initial_state)) << error();
}

AXTree::AXTree(const AXTreeUpdate& initial_state)
    : delegate_(NULL), root_(NULL) {
  CHECK(Unserialize(initial_state)) << error();
}

AXTree::~AXTree() {
  if (root_)
    DestroyNodeAndSubtree(root_, nullptr);
}

void AXTree::SetDelegate(AXTreeDelegate* delegate) {
  delegate_ = delegate;
}

AXNode* AXTree::GetFromId(int32_t id) const {
  base::hash_map<int32_t, AXNode*>::const_iterator iter = id_map_.find(id);
  return iter != id_map_.end() ? iter->second : NULL;
}

void AXTree::UpdateData(const AXTreeData& new_data) {
  if (data_ == new_data)
    return;

  AXTreeData old_data = data_;
  data_ = new_data;
  if (delegate_)
    delegate_->OnTreeDataChanged(this, old_data, new_data);
}

gfx::RectF AXTree::RelativeToTreeBounds(const AXNode* node,
                                        gfx::RectF bounds,
                                        bool* offscreen) const {
  // If |bounds| is uninitialized, which is not the same as empty,
  // start with the node bounds.
  if (bounds.width() == 0 && bounds.height() == 0) {
    bounds = node->data().location;

    // If the node bounds is empty (either width or height is zero),
    // try to compute good bounds from the children.
    if (bounds.IsEmpty()) {
      bool all_children_offscreen = true;
      for (size_t i = 0; i < node->children().size(); i++) {
        ui::AXNode* child = node->children()[i];
        bool temp_offscreen = false;
        bounds.Union(GetTreeBounds(child, &temp_offscreen));
        if (!temp_offscreen)
          // At least one child is on screen.
          all_children_offscreen = false;
      }
      if (bounds.width() > 0 && bounds.height() > 0) {
        // If all the children are offscreen, the node itself is offscreen.
        if (offscreen != nullptr && all_children_offscreen)
          *offscreen = true;
        return bounds;
      }
    }
  } else {
    bounds.Offset(node->data().location.x(), node->data().location.y());
  }

  while (node != nullptr) {
    if (node->data().transform)
      node->data().transform->TransformRect(&bounds);
    const AXNode* container;

    // Normally we apply any transforms and offsets for each node and
    // then walk up to its offset container - however, if the node has
    // no width or height, walk up to its nearest ancestor until we find
    // one that has bounds.
    if (bounds.width() == 0 && bounds.height() == 0)
      container = node->parent();
    else
      container = GetFromId(node->data().offset_container_id);
    if (!container && container != root())
      container = root();
    if (!container || container == node)
      break;

    gfx::RectF container_bounds = container->data().location;
    bounds.Offset(container_bounds.x(), container_bounds.y());

    // If we don't have any size yet, take the size from this ancestor.
    // The rationale is that it's not useful to the user for an object to
    // have no width or height and it's probably a bug; it's better to
    // reflect the bounds of the nearest ancestor rather than a 0x0 box.
    // Tag this node as 'offscreen' because it has no true size, just a
    // size inherited from the ancestor.
    if (bounds.width() == 0 && bounds.height() == 0) {
      bounds.set_size(container_bounds.size());
      if (offscreen != nullptr)
        *offscreen = true;
    }

    int scroll_x = 0;
    int scroll_y = 0;
    if (container->data().GetIntAttribute(ui::AX_ATTR_SCROLL_X, &scroll_x) &&
        container->data().GetIntAttribute(ui::AX_ATTR_SCROLL_Y, &scroll_y)) {
      bounds.Offset(-scroll_x, -scroll_y);
    }

    // If this is the root web area, make sure we clip the node to fit.
    if (container->data().role == ui::AX_ROLE_ROOT_WEB_AREA) {
      gfx::RectF clipped = bounds;
      clipped.Intersect(container_bounds);
      if (!clipped.IsEmpty()) {
        // We can simply clip it to the container.
        bounds = clipped;
        // No need to update |offscreen| if it is set, because it should be
        // false by default.
      } else {
        // Totally offscreen. Find the nearest edge or corner.
        // Make the minimum dimension 1 instead of 0.
        if (bounds.x() >= container_bounds.width()) {
          bounds.set_x(container_bounds.width() - 1);
          bounds.set_width(1);
        } else if (bounds.x() + bounds.width() <= 0) {
          bounds.set_x(0);
          bounds.set_width(1);
        }
        if (bounds.y() >= container_bounds.height()) {
          bounds.set_y(container_bounds.height() - 1);
          bounds.set_height(1);
        } else if (bounds.y() + bounds.height() <= 0) {
          bounds.set_y(0);
          bounds.set_height(1);
        }
        if (offscreen != nullptr)
          *offscreen |= true;
      }
    }

    node = container;
  }

  return bounds;
}

gfx::RectF AXTree::GetTreeBounds(const AXNode* node, bool* offscreen) const {
  return RelativeToTreeBounds(node, gfx::RectF(), offscreen);
}

std::set<int32_t> AXTree::GetReverseRelations(AXIntAttribute attr,
                                              int32_t dst_id) {
  DCHECK(IsNodeIdIntAttribute(attr));
  return int_reverse_relations_[attr][dst_id];
}

std::set<int32_t> AXTree::GetReverseRelations(AXIntListAttribute attr,
                                              int32_t dst_id) {
  DCHECK(IsNodeIdIntListAttribute(attr));
  return intlist_reverse_relations_[attr][dst_id];
}

bool AXTree::Unserialize(const AXTreeUpdate& update) {
  AXTreeUpdateState update_state;
  int32_t old_root_id = root_ ? root_->id() : 0;

  // First, make a note of any nodes we will touch as part of this update.
  for (size_t i = 0; i < update.nodes.size(); ++i)
    update_state.changed_node_ids.insert(update.nodes[i].id);

  if (update.has_tree_data)
    UpdateData(update.tree_data);

  if (update.node_id_to_clear != 0) {
    AXNode* node = GetFromId(update.node_id_to_clear);
    if (!node) {
      error_ = base::StringPrintf("Bad node_id_to_clear: %d",
                                  update.node_id_to_clear);
      return false;
    }
    if (node == root_) {
      // Clear root_ before calling DestroySubtree so that root_ doesn't
      // ever point to an invalid node.
      AXNode* old_root = root_;
      root_ = nullptr;
      DestroySubtree(old_root, &update_state);
    } else {
      for (int i = 0; i < node->child_count(); ++i)
        DestroySubtree(node->ChildAtIndex(i), &update_state);
      std::vector<AXNode*> children;
      node->SwapChildren(children);
      update_state.pending_nodes.insert(node);
    }
  }

  bool root_exists = GetFromId(update.root_id) != nullptr;
  for (size_t i = 0; i < update.nodes.size(); ++i) {
    bool is_new_root = !root_exists && update.nodes[i].id == update.root_id;
    if (!UpdateNode(update.nodes[i], is_new_root, &update_state))
      return false;
  }

  if (!root_) {
    error_ = "Tree has no root.";
    return false;
  }

  if (!update_state.pending_nodes.empty()) {
    error_ = "Nodes left pending by the update:";
    for (std::set<AXNode*>::iterator iter = update_state.pending_nodes.begin();
         iter != update_state.pending_nodes.end(); ++iter) {
      error_ += base::StringPrintf(" %d", (*iter)->id());
    }
    return false;
  }

  if (delegate_) {
    std::set<AXNode*>& new_nodes = update_state.new_nodes;
    std::vector<AXTreeDelegate::Change> changes;
    changes.reserve(update.nodes.size());
    for (size_t i = 0; i < update.nodes.size(); ++i) {
      AXNode* node = GetFromId(update.nodes[i].id);
      if (!node)
        continue;

      bool is_new_node = new_nodes.find(node) != new_nodes.end();
      bool is_reparented_node =
          is_new_node && update_state.HasRemovedNode(node);

      AXTreeDelegate::ChangeType change = AXTreeDelegate::NODE_CHANGED;
      if (is_new_node) {
        if (is_reparented_node) {
          // A reparented subtree is any new node whose parent either doesn't
          // exist, or is not new.
          bool is_subtree = !node->parent() ||
                            new_nodes.find(node->parent()) == new_nodes.end();
          change = is_subtree ? AXTreeDelegate::SUBTREE_REPARENTED
                              : AXTreeDelegate::NODE_REPARENTED;
        } else {
          // A new subtree is any new node whose parent is either not new, or
          // whose parent happens to be new only because it has been reparented.
          bool is_subtree = !node->parent() ||
                            new_nodes.find(node->parent()) == new_nodes.end() ||
                            update_state.HasRemovedNode(node->parent());
          change = is_subtree ? AXTreeDelegate::SUBTREE_CREATED
                              : AXTreeDelegate::NODE_CREATED;
        }
      }
      changes.push_back(AXTreeDelegate::Change(node, change));
    }
    delegate_->OnAtomicUpdateFinished(
        this, root_->id() != old_root_id, changes);
  }

  return true;
}

std::string AXTree::ToString() const {
  return "AXTree" + data_.ToString() + "\n" + TreeToStringHelper(root_, 0);
}

AXNode* AXTree::CreateNode(AXNode* parent,
                           int32_t id,
                           int32_t index_in_parent,
                           AXTreeUpdateState* update_state) {
  AXNode* new_node = new AXNode(parent, id, index_in_parent);
  id_map_[new_node->id()] = new_node;
  if (delegate_) {
    if (update_state->HasChangedNode(new_node) &&
        !update_state->HasRemovedNode(new_node))
      delegate_->OnNodeCreated(this, new_node);
    else
      delegate_->OnNodeReparented(this, new_node);
  }
  return new_node;
}

bool AXTree::UpdateNode(const AXNodeData& src,
                        bool is_new_root,
                        AXTreeUpdateState* update_state) {
  // This method updates one node in the tree based on serialized data
  // received in an AXTreeUpdate. See AXTreeUpdate for pre and post
  // conditions.

  // Look up the node by id. If it's not found, then either the root
  // of the tree is being swapped, or we're out of sync with the source
  // and this is a serious error.
  AXNode* node = GetFromId(src.id);
  if (node) {
    update_state->pending_nodes.erase(node);
    if (update_state->new_nodes.find(node) == update_state->new_nodes.end())
      CallNodeChangeCallbacks(node, src);
    UpdateReverseRelations(node, src);
    node->SetData(src);
  } else {
    if (!is_new_root) {
      error_ = base::StringPrintf(
          "%d is not in the tree and not the new root", src.id);
      return false;
    }

    update_state->new_root = CreateNode(NULL, src.id, 0, update_state);
    node = update_state->new_root;
    update_state->new_nodes.insert(node);
    UpdateReverseRelations(node, src);
    node->SetData(src);
  }

  if (delegate_)
    delegate_->OnNodeChanged(this, node);

  // First, delete nodes that used to be children of this node but aren't
  // anymore.
  if (!DeleteOldChildren(node, src.child_ids, update_state)) {
    // If DeleteOldChildren failed, we need to carefully clean up before
    // returning false as well. In particular, if this node was a new root,
    // we need to safely destroy the whole tree.
    if (update_state->new_root) {
      AXNode* old_root = root_;
      root_ = nullptr;

      DestroySubtree(old_root, update_state);

      // Delete |node|'s subtree too as long as it wasn't already removed
      // or added elsewhere in the tree.
      if (update_state->removed_node_ids.find(src.id) ==
              update_state->removed_node_ids.end() &&
          update_state->new_nodes.find(node) != update_state->new_nodes.end()) {
        DestroySubtree(node, update_state);
      }
    }
    return false;
  }

  // Now build a new children vector, reusing nodes when possible,
  // and swap it in.
  std::vector<AXNode*> new_children;
  bool success = CreateNewChildVector(
      node, src.child_ids, &new_children, update_state);
  node->SwapChildren(new_children);

  // Update the root of the tree if needed.
  if (is_new_root) {
    // Make sure root_ always points to something valid or null_, even inside
    // DestroySubtree.
    AXNode* old_root = root_;
    root_ = node;
    if (old_root && old_root != node)
      DestroySubtree(old_root, update_state);
  }

  return success;
}

void AXTree::CallNodeChangeCallbacks(AXNode* node, const AXNodeData& new_data) {
  if (!delegate_)
    return;

  const AXNodeData& old_data = node->data();
  delegate_->OnNodeDataWillChange(this, old_data, new_data);

  if (old_data.role != new_data.role)
    delegate_->OnRoleChanged(this, node, old_data.role, new_data.role);

  if (old_data.state != new_data.state) {
    for (int i = AX_STATE_NONE + 1; i <= AX_STATE_LAST; ++i) {
      AXState state = static_cast<AXState>(i);
      if (old_data.HasState(state) != new_data.HasState(state))
        delegate_->OnStateChanged(this, node, state, new_data.HasState(state));
    }
  }

  auto string_callback = [this, node](AXStringAttribute attr,
                                      const std::string& old_string,
                                      const std::string& new_string) {
    delegate_->OnStringAttributeChanged(this, node, attr, old_string,
                                        new_string);
  };
  CallIfAttributeValuesChanged(old_data.string_attributes,
                               new_data.string_attributes, std::string(),
                               string_callback);

  auto bool_callback = [this, node](AXBoolAttribute attr, const bool& old_bool,
                                    const bool& new_bool) {
    delegate_->OnBoolAttributeChanged(this, node, attr, new_bool);
  };
  CallIfAttributeValuesChanged(old_data.bool_attributes,
                               new_data.bool_attributes, false, bool_callback);

  auto float_callback = [this, node](AXFloatAttribute attr,
                                     const float& old_float,
                                     const float& new_float) {
    delegate_->OnFloatAttributeChanged(this, node, attr, old_float, new_float);
  };
  CallIfAttributeValuesChanged(old_data.float_attributes,
                               new_data.float_attributes, 0.0f, float_callback);

  auto int_callback = [this, node](AXIntAttribute attr, const int& old_int,
                                   const int& new_int) {
    delegate_->OnIntAttributeChanged(this, node, attr, old_int, new_int);
  };
  CallIfAttributeValuesChanged(old_data.int_attributes, new_data.int_attributes,
                               0, int_callback);

  auto intlist_callback = [this, node](
                              AXIntListAttribute attr,
                              const std::vector<int32_t>& old_intlist,
                              const std::vector<int32_t>& new_intlist) {
    delegate_->OnIntListAttributeChanged(this, node, attr, old_intlist,
                                         new_intlist);
  };
  CallIfAttributeValuesChanged(old_data.intlist_attributes,
                               new_data.intlist_attributes,
                               std::vector<int32_t>(), intlist_callback);

  auto stringlist_callback =
      [this, node](AXStringListAttribute attr,
                   const std::vector<std::string>& old_stringlist,
                   const std::vector<std::string>& new_stringlist) {
        delegate_->OnStringListAttributeChanged(this, node, attr,
                                                old_stringlist, new_stringlist);
      };
  CallIfAttributeValuesChanged(old_data.stringlist_attributes,
                               new_data.stringlist_attributes,
                               std::vector<std::string>(), stringlist_callback);
}

void AXTree::UpdateReverseRelations(AXNode* node, const AXNodeData& new_data) {
  const AXNodeData& old_data = node->data();
  int id = new_data.id;
  auto int_callback = [this, node, id](AXIntAttribute attr, const int& old_int,
                                       const int& new_int) {
    if (!IsNodeIdIntAttribute(attr))
      return;

    int_reverse_relations_[attr][old_int].erase(id);
    int_reverse_relations_[attr][new_int].insert(id);
  };
  CallIfAttributeValuesChanged(old_data.int_attributes, new_data.int_attributes,
                               0, int_callback);

  auto intlist_callback = [this, node, id](
                              AXIntListAttribute attr,
                              const std::vector<int32_t>& old_intlist,
                              const std::vector<int32_t>& new_intlist) {
    if (!IsNodeIdIntListAttribute(attr))
      return;

    for (int32_t old_id : old_intlist)
      intlist_reverse_relations_[attr][old_id].erase(id);
    for (int32_t new_id : new_intlist)
      intlist_reverse_relations_[attr][new_id].insert(id);
  };
  CallIfAttributeValuesChanged(old_data.intlist_attributes,
                               new_data.intlist_attributes,
                               std::vector<int32_t>(), intlist_callback);
}

void AXTree::DestroySubtree(AXNode* node,
                            AXTreeUpdateState* update_state) {
  DCHECK(update_state);
  if (delegate_) {
    if (!update_state->HasChangedNode(node))
      delegate_->OnSubtreeWillBeDeleted(this, node);
    else
      delegate_->OnSubtreeWillBeReparented(this, node);
  }
  DestroyNodeAndSubtree(node, update_state);
}

void AXTree::DestroyNodeAndSubtree(AXNode* node,
                                   AXTreeUpdateState* update_state) {
  if (delegate_) {
    if (!update_state || !update_state->HasChangedNode(node))
      delegate_->OnNodeWillBeDeleted(this, node);
    else
      delegate_->OnNodeWillBeReparented(this, node);
  }
  id_map_.erase(node->id());
  for (int i = 0; i < node->child_count(); ++i)
    DestroyNodeAndSubtree(node->ChildAtIndex(i), update_state);
  if (update_state) {
    update_state->pending_nodes.erase(node);
    update_state->removed_node_ids.insert(node->id());
  }
  node->Destroy();
}

bool AXTree::DeleteOldChildren(AXNode* node,
                               const std::vector<int32_t>& new_child_ids,
                               AXTreeUpdateState* update_state) {
  // Create a set of child ids in |src| for fast lookup, and return false
  // if a duplicate is found;
  std::set<int32_t> new_child_id_set;
  for (size_t i = 0; i < new_child_ids.size(); ++i) {
    if (new_child_id_set.find(new_child_ids[i]) != new_child_id_set.end()) {
      error_ = base::StringPrintf("Node %d has duplicate child id %d",
                                  node->id(), new_child_ids[i]);
      return false;
    }
    new_child_id_set.insert(new_child_ids[i]);
  }

  // Delete the old children.
  const std::vector<AXNode*>& old_children = node->children();
  for (size_t i = 0; i < old_children.size(); ++i) {
    int old_id = old_children[i]->id();
    if (new_child_id_set.find(old_id) == new_child_id_set.end())
      DestroySubtree(old_children[i], update_state);
  }

  return true;
}

bool AXTree::CreateNewChildVector(AXNode* node,
                                  const std::vector<int32_t>& new_child_ids,
                                  std::vector<AXNode*>* new_children,
                                  AXTreeUpdateState* update_state) {
  bool success = true;
  for (size_t i = 0; i < new_child_ids.size(); ++i) {
    int32_t child_id = new_child_ids[i];
    int32_t index_in_parent = static_cast<int32_t>(i);
    AXNode* child = GetFromId(child_id);
    if (child) {
      if (child->parent() != node) {
        // This is a serious error - nodes should never be reparented.
        // If this case occurs, continue so this node isn't left in an
        // inconsistent state, but return failure at the end.
        error_ = base::StringPrintf(
            "Node %d reparented from %d to %d",
            child->id(),
            child->parent() ? child->parent()->id() : 0,
            node->id());
        success = false;
        continue;
      }
      child->SetIndexInParent(index_in_parent);
    } else {
      child = CreateNode(node, child_id, index_in_parent, update_state);
      update_state->pending_nodes.insert(child);
      update_state->new_nodes.insert(child);
    }
    new_children->push_back(child);
  }

  return success;
}

}  // namespace ui
