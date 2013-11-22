// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree.h"

#include <set>

#include "ui/accessibility/ax_node.h"

namespace ui {

AXTree::AXTree()
    : root_(NULL) {
  AXNodeData root;
  root.id = 0;
  root.role = AX_ROLE_ROOT_WEB_AREA;

  AXTreeUpdate initial_state;
  initial_state.nodes.push_back(root);
  Unserialize(initial_state);
}

AXTree::AXTree(const AXTreeUpdate& initial_state)
    : root_(NULL) {
  Unserialize(initial_state);
}

AXTree::~AXTree() {
  if (root_)
    DestroyNodeAndSubtree(root_);
}

AXNode* AXTree::GetRoot() const {
  return root_;
}

AXNode* AXTree::GetFromId(int32 id) const {
  base::hash_map<int32, AXNode*>::const_iterator iter = id_map_.find(id);
  return iter != id_map_.end() ? (iter->second) : NULL;
}

bool AXTree::Unserialize(const AXTreeUpdate& update) {
  for (size_t i = 0; i < update.nodes.size(); ++i) {
    if (!UpdateNode(update.nodes[i]))
      return false;
  }

  return true;
}

AXNode* AXTree::CreateNode(AXNode* parent, int32 id, int32 index_in_parent) {
  return new AXNode(parent, id, index_in_parent);
}

bool AXTree::UpdateNode(const AXNodeData& src) {
  // This method updates one node in the tree based on serialized data
  // received in an AXTreeUpdate. See AXTreeUpdate for pre and post
  // conditions.

  // Look up the node by id. If it's not found, then either the root
  // of the tree is being swapped, or we're out of sync with the source
  // and this is a serious error.
  AXNode* node = static_cast<AXNode*>(GetFromId(src.id));
  if (!node) {
    if (src.role != AX_ROLE_ROOT_WEB_AREA)
      return false;
    node = CreateAndInitializeNode(NULL, src.id, 0);
  }

  // Set the node's data.
  node->SetData(src);

  // First, delete nodes that used to be children of this node but aren't
  // anymore.
  if (!DeleteOldChildren(node, src.child_ids))
    return false;

  // Now build a new children vector, reusing nodes when possible,
  // and swap it in.
  std::vector<AXNode*> new_children;
  bool success = CreateNewChildVector(node, src.child_ids, &new_children);
  node->SwapChildren(new_children);

  // Update the root of the tree if needed.
  if (src.role == AX_ROLE_ROOT_WEB_AREA &&
      (!root_ || root_->id() != src.id)) {
    if (root_)
      DestroyNodeAndSubtree(root_);
    root_ = node;
    OnRootChanged();
  }

  return success;
}

void AXTree::OnRootChanged() {
}

AXNode* AXTree::CreateAndInitializeNode(
    AXNode* parent, int32 id, int32 index_in_parent) {
  AXNode* node = CreateNode(parent, id, index_in_parent);
  id_map_[node->id()] = node;
  return node;
}

void AXTree::DestroyNodeAndSubtree(AXNode* node) {
  id_map_.erase(node->id());
  for (int i = 0; i < node->child_count(); ++i) {
    AXNode* child = static_cast<AXNode*>(node->ChildAtIndex(i));
    child->Destroy();
  }
  node->Destroy();
}

bool AXTree::DeleteOldChildren(AXNode* node,
                                   const std::vector<int32> new_child_ids) {
  // Create a set of child ids in |src| for fast lookup, and return false
  // if a duplicate is found;
  std::set<int32> new_child_id_set;
  for (size_t i = 0; i < new_child_ids.size(); ++i) {
    if (new_child_id_set.find(new_child_ids[i]) != new_child_id_set.end())
      return false;
    new_child_id_set.insert(new_child_ids[i]);
  }

  // Delete the old children.
  const std::vector<AXNode*>& old_children = node->children();
  for (size_t i = 0; i < old_children.size(); ++i) {
    int old_id = old_children[i]->id();
    if (new_child_id_set.find(old_id) == new_child_id_set.end())
      DestroyNodeAndSubtree(old_children[i]);
  }

  return true;
}

bool AXTree::CreateNewChildVector(AXNode* node,
                                      const std::vector<int32> new_child_ids,
                                      std::vector<AXNode*>* new_children) {
  bool success = true;
  for (size_t i = 0; i < new_child_ids.size(); ++i) {
    int32 child_id = new_child_ids[i];
    int32 index_in_parent = static_cast<int32>(i);
    AXNode* child = static_cast<AXNode*>(GetFromId(child_id));
    if (child) {
      if (child->parent() != node) {
        // This is a serious error - nodes should never be reparented.
        // If this case occurs, continue so this node isn't left in an
        // inconsistent state, but return failure at the end.
        success = false;
        continue;
      }
      child->SetIndexInParent(index_in_parent);
    } else {
      child = CreateAndInitializeNode(node, child_id, index_in_parent);
    }
    new_children->push_back(child);
  }

  return success;
}

}  // namespace ui
