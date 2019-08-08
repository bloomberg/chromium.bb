// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_OBSERVER_H_
#define UI_ACCESSIBILITY_AX_TREE_OBSERVER_H_

#include "base/observer_list_types.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_export.h"

namespace ui {

class AXNode;
struct AXNodeData;
class AXTree;
struct AXTreeData;

// Used when you want to be notified when changes happen to an AXTree.
//
// Some of the notifications are called in the middle of an update operation.
// Be careful, as the tree may be in an inconsistent state at this time;
// don't walk the parents and children at this time:
//   OnNodeWillBeDeleted
//   OnSubtreeWillBeDeleted
//   OnNodeWillBeReparented
//   OnSubtreeWillBeReparented
//   OnNodeCreated
//   OnNodeReparented
//   OnNodeChanged
//
// In addition, one additional notification is fired at the end of an
// atomic update, and it provides a vector of nodes that were added or
// changed, for final postprocessing:
//   OnAtomicUpdateFinished
//
class AX_EXPORT AXTreeObserver : public base::CheckedObserver {
 public:
  AXTreeObserver();
  ~AXTreeObserver() override;

  // Called before a node's data gets updated.
  virtual void OnNodeDataWillChange(AXTree* tree,
                                    const AXNodeData& old_node_data,
                                    const AXNodeData& new_node_data) {}

  // Individual callbacks for every attribute of AXNodeData that can change.
  virtual void OnRoleChanged(AXTree* tree,
                             AXNode* node,
                             ax::mojom::Role old_role,
                             ax::mojom::Role new_role) {}
  virtual void OnStateChanged(AXTree* tree,
                              AXNode* node,
                              ax::mojom::State state,
                              bool new_value) {}
  virtual void OnStringAttributeChanged(AXTree* tree,
                                        AXNode* node,
                                        ax::mojom::StringAttribute attr,
                                        const std::string& old_value,
                                        const std::string& new_value) {}
  virtual void OnIntAttributeChanged(AXTree* tree,
                                     AXNode* node,
                                     ax::mojom::IntAttribute attr,
                                     int32_t old_value,
                                     int32_t new_value) {}
  virtual void OnFloatAttributeChanged(AXTree* tree,
                                       AXNode* node,
                                       ax::mojom::FloatAttribute attr,
                                       float old_value,
                                       float new_value) {}
  virtual void OnBoolAttributeChanged(AXTree* tree,
                                      AXNode* node,
                                      ax::mojom::BoolAttribute attr,
                                      bool new_value) {}
  virtual void OnIntListAttributeChanged(
      AXTree* tree,
      AXNode* node,
      ax::mojom::IntListAttribute attr,
      const std::vector<int32_t>& old_value,
      const std::vector<int32_t>& new_value) {}
  virtual void OnStringListAttributeChanged(
      AXTree* tree,
      AXNode* node,
      ax::mojom::StringListAttribute attr,
      const std::vector<std::string>& old_value,
      const std::vector<std::string>& new_value) {}

  // Called when tree data changes.
  virtual void OnTreeDataChanged(AXTree* tree,
                                 const AXTreeData& old_data,
                                 const AXTreeData& new_data) {}

  // Called just before a node is deleted. Its id and data will be valid,
  // but its links to parents and children are invalid. This is called
  // in the middle of an update, the tree may be in an invalid state!
  virtual void OnNodeWillBeDeleted(AXTree* tree, AXNode* node) {}

  // Same as OnNodeWillBeDeleted, but only called once for an entire subtree.
  // This is called in the middle of an update, the tree may be in an
  // invalid state!
  virtual void OnSubtreeWillBeDeleted(AXTree* tree, AXNode* node) {}

  // Called just before a node is deleted for reparenting. See
  // |OnNodeWillBeDeleted| for additional information.
  virtual void OnNodeWillBeReparented(AXTree* tree, AXNode* node) {}

  // Called just before a subtree is deleted for reparenting. See
  // |OnSubtreeWillBeDeleted| for additional information.
  virtual void OnSubtreeWillBeReparented(AXTree* tree, AXNode* node) {}

  // Called immediately after a new node is created. The tree may be in
  // the middle of an update, don't walk the parents and children now.
  virtual void OnNodeCreated(AXTree* tree, AXNode* node) {}

  // Called immediately after a node is reparented. The tree may be in the
  // middle of an update, don't walk the parents and children now.
  virtual void OnNodeReparented(AXTree* tree, AXNode* node) {}

  // Called when a node changes its data or children. The tree may be in
  // the middle of an update, don't walk the parents and children now.
  virtual void OnNodeChanged(AXTree* tree, AXNode* node) {}

  enum ChangeType {
    NODE_CREATED,
    SUBTREE_CREATED,
    NODE_CHANGED,
    NODE_REPARENTED,
    SUBTREE_REPARENTED
  };

  struct Change {
    Change(AXNode* node, ChangeType type) {
      this->node = node;
      this->type = type;
    }
    AXNode* node;
    ChangeType type;
  };

  // Called at the end of the update operation. Every node that was added
  // or changed will be included in |changes|, along with an enum indicating
  // the type of change - either (1) a node was created, (2) a node was created
  // and it's the root of a new subtree, or (3) a node was changed. Finally,
  // a bool indicates if the root of the tree was changed or not.
  virtual void OnAtomicUpdateFinished(AXTree* tree,
                                      bool root_changed,
                                      const std::vector<Change>& changes) {}
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_OBSERVER_H_
