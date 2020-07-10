// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_UPDATE_H_
#define UI_ACCESSIBILITY_AX_TREE_UPDATE_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_data.h"

namespace ui {

// An AXTreeUpdate is a serialized representation of an atomic change
// to an AXTree. The sender and receiver must be in sync; the update
// is only meant to bring the tree from a specific previous state into
// its next state. Trying to apply it to the wrong tree should immediately
// die with a fatal assertion.
//
// An AXTreeUpdate consists of an optional node id to clear (meaning
// that all of that node's children and their descendants are deleted),
// followed by an ordered vector of zero or more AXNodeData structures to
// be applied to the tree in order. An update may also include an optional
// update to the AXTreeData structure that applies to the tree as a whole.
//
// Suppose that the next AXNodeData to be applied is |node|. The following
// invariants must hold:
// 1. Either
//   a) |node.id| is already in the tree, or
//   b) the tree is empty, and
//      |node| is the new root of the tree, and
//      |node.role| == WebAXRoleRootWebArea.
// 2. Every child id in |node.child_ids| must either be already a child
//        of this node, or a new id not previously in the tree. It is not
//        allowed to "reparent" a child to this node without first removing
//        that child from its previous parent.
// 3. When a new id appears in |node.child_ids|, the tree should create a
//        new uninitialized placeholder node for it immediately. That
//        placeholder must be updated within the same AXTreeUpdate, otherwise
//        it's a fatal error. This guarantees the tree is always complete
//        before or after an AXTreeUpdate.
template<typename AXNodeData, typename AXTreeData> struct AXTreeUpdateBase {
  AXTreeUpdateBase() = default;
  ~AXTreeUpdateBase() = default;

  // If |has_tree_data| is true, the value of |tree_data| should be used
  // to update the tree data, otherwise it should be ignored.
  bool has_tree_data = false;
  AXTreeData tree_data;

  // The id of a node to clear, before applying any updates,
  // or 0 if no nodes should be cleared. Clearing a node means deleting
  // all of its children and their descendants, but leaving that node in
  // the tree. It's an error to clear a node but not subsequently update it
  // as part of the tree update.
  int node_id_to_clear = 0;

  // The id of the root of the tree, if the root is changing. This is
  // required to be set if the root of the tree is changing or Unserialize
  // will fail. If the root of the tree is not changing this is optional
  // and it is allowed to pass 0.
  int root_id = 0;

  // A vector of nodes to update, according to the rules above.
  std::vector<AXNodeData> nodes;

  // The source of the event.
  ax::mojom::EventFrom event_from = ax::mojom::EventFrom::kNone;

  // Return a multi-line indented string representation, for logging.
  std::string ToString() const;

  // TODO(dmazzoni): location changes
};

using AXTreeUpdate = AXTreeUpdateBase<AXNodeData, AXTreeData>;

template<typename AXNodeData, typename AXTreeData>
std::string AXTreeUpdateBase<AXNodeData, AXTreeData>::ToString() const {
  std::string result;

  if (has_tree_data) {
    result += "AXTreeUpdate tree data:" + tree_data.ToString() + "\n";
  }

  if (node_id_to_clear != 0) {
    result += "AXTreeUpdate: clear node " +
              base::NumberToString(node_id_to_clear) + "\n";
  }

  if (root_id != 0) {
    result += "AXTreeUpdate: root id " + base::NumberToString(root_id) + "\n";
  }

  // The challenge here is that we want to indent the nodes being updated
  // so that parent/child relationships are clear, but we don't have access
  // to the rest of the tree for context, so we have to try to show the
  // relative indentation of child nodes in this update relative to their
  // parents.
  std::unordered_map<int32_t, int> id_to_indentation;
  for (size_t i = 0; i < nodes.size(); ++i) {
    int indent = id_to_indentation[nodes[i].id];
    result += std::string(2 * indent, ' ');
    result += nodes[i].ToString() + "\n";
    for (size_t j = 0; j < nodes[i].child_ids.size(); ++j)
      id_to_indentation[nodes[i].child_ids[j]] = indent + 1;
  }

  return result;
}

// Two tree updates can be merged into one if the second one
// doesn't clear a subtree, doesn't have new tree data, and
// doesn't have a new root id - in other words the second tree
// update consists of only changes to nodes.
template <typename AXNodeData, typename AXTreeData>
bool TreeUpdatesCanBeMerged(
    const AXTreeUpdateBase<AXNodeData, AXTreeData>& u1,
    const AXTreeUpdateBase<AXNodeData, AXTreeData>& u2) {
  if (u2.node_id_to_clear)
    return false;

  if (u2.has_tree_data && u2.tree_data != u1.tree_data)
    return false;

  if (u2.root_id != u1.root_id)
    return false;

  return true;
}

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_UPDATE_H_
