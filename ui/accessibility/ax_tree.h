// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_H_
#define UI_ACCESSIBILITY_AX_TREE_H_

#include <set>

#include "base/containers/hash_tables.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_update.h"

namespace ui {

class AXNode;

// AXTree is a live, managed tree of AXNode objects that can receive
// updates from another AXTreeSource via AXTreeUpdates, and it can be
// used as a source for sending updates to another client tree.
// It's designed to be subclassed to implement support for native
// accessibility APIs on a specific platform.
class AX_EXPORT AXTree {
 public:
  AXTree();
  explicit AXTree(const AXTreeUpdate& initial_state);
  virtual ~AXTree();

  virtual AXNode* GetRoot() const;
  virtual AXNode* GetFromId(int32 id) const;

  // Returns true on success. If it returns false, it's a fatal error
  // and this tree should be destroyed, and the source of the tree update
  // should not be trusted any longer.
  virtual bool Unserialize(const AXTreeUpdate& update);

  // A string describing the error from an unsuccessful Unserialize,
  // for testing and debugging.
  const std::string& error() { return error_; }

 protected:
  // Subclasses can override this to use a subclass of AXNode.
  virtual AXNode* CreateNode(AXNode* parent, int32 id, int32 index_in_parent);

  // This is called from within Unserialize(), it returns true on success.
  // Subclasses can override this to do additional processing. |pending_nodes|
  // is updated to contain all nodes that have been implicitly referenced
  // as part of this update, but haven't been updated yet. It's an error if
  // there are any pending nodes at the end of Unserialize.
  virtual bool UpdateNode(const AXNodeData& src,
                          std::set<AXNode*>* pending_nodes);

  // Subclasses can override this to do special behavior when the root changes.
  virtual void OnRootChanged();

 private:
  // Convenience function to create a node and call Initialize on it.
  AXNode* CreateAndInitializeNode(
      AXNode* parent, int32 id, int32 index_in_parent);

  // Call Destroy() on |node|, and delete it from the id map, and then
  // call recursively on all nodes in its subtree.
  void DestroyNodeAndSubtree(AXNode* node);

  // Iterate over the children of |node| and for each child, destroy the
  // child and its subtree if its id is not in |new_child_ids|. Returns
  // true on success, false on fatal error.
  bool DeleteOldChildren(AXNode* node,
                         const std::vector<int32> new_child_ids);

  // Iterate over |new_child_ids| and populate |new_children| with
  // pointers to child nodes, reusing existing nodes already in the tree
  // if they exist, and creating otherwise. Reparenting is disallowed, so
  // if the id already exists as the child of another node, that's an
  // error. Returns true on success, false on fatal error. See
  // UpdateNode, above, for an explanation of |pending_nodes|.
  bool CreateNewChildVector(AXNode* node,
                            const std::vector<int32> new_child_ids,
                            std::vector<AXNode*>* new_children,
                            std::set<AXNode*>* pending_nodes);

  AXNode* root_;
  base::hash_map<int32, AXNode*> id_map_;
  std::string error_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_H_
