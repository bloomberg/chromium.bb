// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_SOURCE_CHECKER_H_
#define UI_ACCESSIBILITY_AX_TREE_SOURCE_CHECKER_H_

#include <map>

#include "base/strings/string_number_conversions.h"
#include "ui/accessibility/ax_tree_source.h"

namespace ui {

template <typename AXSourceNode, typename AXNodeData, typename AXTreeData>
class AXTreeSourceChecker {
 public:
  explicit AXTreeSourceChecker(
      AXTreeSource<AXSourceNode, AXNodeData, AXTreeData>* tree);
  ~AXTreeSourceChecker();

  // Returns true if everything reachable from the root of the tree is
  // consistent in its parent/child connections.
  bool Check();

 private:
  bool Check(AXSourceNode node, std::string indent, std::string* output);

  AXTreeSource<AXSourceNode, AXNodeData, AXTreeData>* tree_;

  std::map<int32_t, int32_t> node_id_to_parent_id_map_;

  DISALLOW_COPY_AND_ASSIGN(AXTreeSourceChecker);
};

template <typename AXSourceNode, typename AXNodeData, typename AXTreeData>
AXTreeSourceChecker<AXSourceNode, AXNodeData, AXTreeData>::AXTreeSourceChecker(
    AXTreeSource<AXSourceNode, AXNodeData, AXTreeData>* tree)
    : tree_(tree) {}

template <typename AXSourceNode, typename AXNodeData, typename AXTreeData>
AXTreeSourceChecker<AXSourceNode, AXNodeData, AXTreeData>::
    ~AXTreeSourceChecker() = default;

template <typename AXSourceNode, typename AXNodeData, typename AXTreeData>
bool AXTreeSourceChecker<AXSourceNode, AXNodeData, AXTreeData>::Check() {
  node_id_to_parent_id_map_.clear();

  AXSourceNode root = tree_->GetRoot();
  if (!tree_->IsValid(root)) {
    LOG(ERROR) << "AXTreeSource root is not valid.";
    return false;
  }

  int32_t root_id = tree_->GetId(root);
  node_id_to_parent_id_map_[root_id] = -1;

  std::string output;
  bool success = Check(root, "", &output);
  if (!success)
    LOG(ERROR) << "AXTreeSource is inconsistent.\n" << output << "\n\n";

  return success;
}

template <typename AXSourceNode, typename AXNodeData, typename AXTreeData>
bool AXTreeSourceChecker<AXSourceNode, AXNodeData, AXTreeData>::Check(
    AXSourceNode node,
    std::string indent,
    std::string* output) {
  *output += indent;
  AXNodeData node_data;
  tree_->SerializeNode(node, &node_data);
  *output += node_data.ToString();

  int32_t node_id = tree_->GetId(node);
  if (node_id <= 0) {
    LOG(ERROR) << "Got a node with id " << node_id
               << ", but all node IDs should be >= 1.";
    return false;
  }

  // Check parent.
  int32_t expected_parent_id = node_id_to_parent_id_map_[node_id];
  AXSourceNode parent = tree_->GetParent(node);
  if (expected_parent_id == -1) {
    if (tree_->IsValid(parent)) {
      LOG(ERROR) << "Node " << node_id << " is the root, so its parent "
                 << "should be invalid, but we got a node with id "
                 << tree_->GetId(parent);
      return false;
    }
  } else {
    if (!tree_->IsValid(parent)) {
      LOG(ERROR) << "Node " << node_id << " is not the root, but its "
                 << "parent was invalid.";
      return false;
    }
    int32_t parent_id = tree_->GetId(parent);
    if (parent_id != expected_parent_id) {
      LOG(ERROR) << "Expected node " << node_id << " to have a parent of "
                 << expected_parent_id << ", but found a parent of "
                 << parent_id;
      return false;
    }
  }

  // Check children.
  std::vector<AXSourceNode> children;
  tree_->GetChildren(node, &children);

  if (children.size() == 0)
    *output += " (no children)";

  for (size_t i = 0; i < children.size(); i++) {
    auto& child = children[i];
    if (!tree_->IsValid(child)) {
      LOG(ERROR) << "Node " << node_id << " has an invalid child.";
      return false;
    }

    int32_t child_id = tree_->GetId(child);
    if (i == 0)
      *output += " child_ids=" + base::NumberToString(child_id);
    else
      *output += "," + base::NumberToString(child_id);

    if (node_id_to_parent_id_map_.find(child_id) !=
        node_id_to_parent_id_map_.end()) {
      *output += "\n" + indent + "  ";
      AXNodeData child_data;
      tree_->SerializeNode(child, &child_data);
      *output += child_data.ToString() + "\n";

      LOG(ERROR) << "Node " << node_id << " has a child with ID " << child_id
                 << ", but we've previously seen a node "
                 << "with that ID, with a parent of "
                 << node_id_to_parent_id_map_[child_id];
      return false;
    }

    node_id_to_parent_id_map_[child_id] = node_id;
  }

  *output += "\n";

  for (auto& child : children) {
    if (!Check(child, indent + "  ", output))
      return false;
  }

  return true;
}

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_SOURCE_CHECKER_H_
