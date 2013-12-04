// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_serializable_tree.h"

#include "ui/accessibility/ax_node.h"

namespace ui {

// This class is an implementation of the AXTreeSource interface with
// AXNode as the node type, that just delegates to an AXTree. The purpose
// of this is so that AXTreeSerializer only needs to work with the
// AXTreeSource abstraction and doesn't need to actually know about
// AXTree directly. Another AXTreeSource is used to abstract the Blink
// accessibility tree.
class AX_EXPORT AXTreeSourceAdapter : public AXTreeSource<AXNode> {
 public:
  AXTreeSourceAdapter(AXTree* tree) : tree_(tree) {}
  virtual ~AXTreeSourceAdapter() {}

  // AXTreeSource implementation.
  virtual AXNode* GetRoot() const OVERRIDE {
    return tree_->GetRoot();
  }

  virtual AXNode* GetFromId(int32 id) const OVERRIDE {
    return tree_->GetFromId(id);
  }

  virtual int32 GetId(const AXNode* node) const OVERRIDE {
    return node->id();
  }

  virtual int GetChildCount(const AXNode* node) const OVERRIDE {
    return node->child_count();
  }

  virtual AXNode* GetChildAtIndex(const AXNode* node, int index)
      const OVERRIDE {
    return node->ChildAtIndex(index);
  }

  virtual AXNode* GetParent(const AXNode* node) const OVERRIDE {
    return node->parent();
  }

  virtual void SerializeNode(
      const AXNode* node, AXNodeData* out_data) const OVERRIDE {
    *out_data = node->data();
  }

 private:
  AXTree* tree_;
};

AXSerializableTree::AXSerializableTree()
    : AXTree() {}

AXSerializableTree::AXSerializableTree(const AXTreeUpdate& initial_state)
    : AXTree(initial_state) {
}

AXSerializableTree::~AXSerializableTree() {
}

AXTreeSource<AXNode>* AXSerializableTree::CreateTreeSource() {
  return new AXTreeSourceAdapter(this);
}

}  // namespace ui
