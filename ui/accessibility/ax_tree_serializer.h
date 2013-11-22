// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_SERIALIZER_H_
#define UI_ACCESSIBILITY_AX_TREE_SERIALIZER_H_

#include <set>

#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "ui/accessibility/ax_tree_source.h"
#include "ui/accessibility/ax_tree_update.h"

namespace ui {

struct ClientTreeNode;

// AXTreeSerializer is a helper class that serializes incremental
// updates to an AXTreeSource as a vector of AXNodeData structs.
// These structs can be unserialized by an AXTree. An AXTreeSerializer
// keeps track of the tree of node ids that its client is aware of, so
// it will automatically include, as part of any update, any additional nodes
// that the client is not aware of yet.
//
// When the AXTreeSource changes, call SerializeChanges to serialize the
// changes to the tree as an AXTreeUpdate. If a single node has changed,
// pass that node to SerializeChanges. If a node has been added or removed,
// pass the parent of that node to SerializeChanges and it will automatically
// handle changes to its set of children.
//
// TODO(dmazzoni): add sample usage code.
template<class AXSourceNode>
class AXTreeSerializer {
 public:
  explicit AXTreeSerializer(AXTreeSource<AXSourceNode>* tree);

  // Throw out the internal state that keeps track of the nodes the client
  // knows about. This has the effect that the next update will send the
  // entire tree over because it assumes the client knows nothing.
  void Reset();

  // Serialize all changes to |node| and append them to |out_update|.
  void SerializeChanges(const AXSourceNode* node,
                        AXTreeUpdate* out_update);

 private:
  // Delete the given client tree node and recursively delete all of its
  // descendants.
  void DeleteClientSubtree(ClientTreeNode* client_node);

  void SerializeChangedNodes(const AXSourceNode* node,
                             std::set<int>* ids_serialized,
                             AXTreeUpdate* out_update);

  // The tree source.
  AXTreeSource<AXSourceNode>* tree_;

  // Our representation of the client tree.
  ClientTreeNode* client_root_;

  // A map from IDs to nodes in the client tree.
  base::hash_map<int32, ClientTreeNode*> client_id_map_;
};

// In order to keep track of what nodes the client knows about, we keep a
// representation of the client tree - just IDs and parent/child
// relationships.
struct AX_EXPORT ClientTreeNode {
  ClientTreeNode();
  virtual ~ClientTreeNode();
  int32 id;
  ClientTreeNode* parent;
  std::vector<ClientTreeNode*> children;
};

template<class AXSourceNode>
AXTreeSerializer<AXSourceNode>::AXTreeSerializer(
    AXTreeSource<AXSourceNode>* tree)
    : tree_(tree),
      client_root_(NULL) {
}

template<class AXSourceNode>
void AXTreeSerializer<AXSourceNode>::Reset() {
  if (client_root_) {
    DeleteClientSubtree(client_root_);
    client_root_ = NULL;
  }
}

template<class AXSourceNode>
void AXTreeSerializer<AXSourceNode>::SerializeChanges(
    const AXSourceNode* node,
    AXTreeUpdate* out_update) {
  std::set<int> ids_serialized;
  SerializeChangedNodes(node, &ids_serialized, out_update);
}

template<class AXSourceNode>
void AXTreeSerializer<AXSourceNode>::DeleteClientSubtree(
    ClientTreeNode* client_node) {
  for (size_t i = 0; i < client_node->children.size(); ++i) {
    client_id_map_.erase(client_node->children[i]->id);
    DeleteClientSubtree(client_node->children[i]);
  }
  client_node->children.clear();
}

template<class AXSourceNode>
void AXTreeSerializer<AXSourceNode>::SerializeChangedNodes(
    const AXSourceNode* node,
    std::set<int>* ids_serialized,
    AXTreeUpdate* out_update) {
  int id = tree_->GetId(node);
  if (ids_serialized->find(id) != ids_serialized->end())
    return;
  ids_serialized->insert(id);

  // This method has three responsibilities:
  // 1. Serialize |node| into an AXNodeData, and append it to
  //    the AXTreeUpdate to be sent to the client.
  // 2. Determine if |node| has any new children that the client doesn't
  //    know about yet, and call SerializeChangedNodes recursively on those.
  // 3. Update our internal data structure that keeps track of what nodes
  //    the client knows about.

  // First, find the ClientTreeNode for this id in our data structure where
  // we keep track of what accessibility objects the client already knows
  // about. If we don't find it, then this must be the new root of the
  // accessibility tree.
  //
  // TODO(dmazzoni): handle the case where the root changes.
  ClientTreeNode* client_node = NULL;
  base::hash_map<int32, ClientTreeNode*>::iterator iter =
      client_id_map_.find(id);
  if (iter != client_id_map_.end()) {
    client_node = iter->second;
  } else {
    if (client_root_) {
      client_id_map_.erase(client_root_->id);
      DeleteClientSubtree(client_root_);
    }
    client_root_ = new ClientTreeNode();
    client_node = client_root_;
    client_node->id = id;
    client_node->parent = NULL;
    client_id_map_[client_node->id] = client_node;
  }

  // Iterate over the ids of the children of |node|.
  // Create a set of the child ids so we can quickly look
  // up which children are new and which ones were there before.
  // Also catch the case where a child is already in the client tree
  // data structure with a different parent, and make sure the old parent
  // clears this node first.
  base::hash_set<int32> new_child_ids;
  int child_count = tree_->GetChildCount(node);
  for (int i = 0; i < child_count; ++i) {
    AXSourceNode* child = tree_->GetChildAtIndex(node, i);
    int new_child_id = tree_->GetId(child);
    new_child_ids.insert(new_child_id);

    ClientTreeNode* client_child = client_id_map_[new_child_id];
    if (client_child && client_child->parent != client_node) {
      // The child is being reparented. Find the source tree node
      // corresponding to the old parent, or the closest ancestor
      // still in the tree.
      ClientTreeNode* client_parent = client_child->parent;
      AXSourceNode* parent = NULL;
      while (client_parent) {
        parent = tree_->GetFromId(client_parent->id);
        if (parent)
          break;
        client_parent = client_parent->parent;
      }
      CHECK(parent);

      // Call SerializeChangedNodes recursively on the old parent,
      // so that the update that clears |child| from its old parent
      // occurs stricly before the update that adds |child| to its
      // new parent.
      SerializeChangedNodes(parent, ids_serialized, out_update);
    }
  }

  // Go through the old children and delete subtrees for child
  // ids that are no longer present, and create a map from
  // id to ClientTreeNode for the rest. It's important to delete
  // first in a separate pass so that nodes that are reparented
  // don't end up children of two different parents in the middle
  // of an update, which can lead to a double-free.
  base::hash_map<int32, ClientTreeNode*> client_child_id_map;
  std::vector<ClientTreeNode*> old_children;
  old_children.swap(client_node->children);
  for (size_t i = 0; i < old_children.size(); ++i) {
    ClientTreeNode* old_child = old_children[i];
    int old_child_id = old_child->id;
    if (new_child_ids.find(old_child_id) == new_child_ids.end()) {
      client_id_map_.erase(old_child_id);
      DeleteClientSubtree(old_child);
    } else {
      client_child_id_map[old_child_id] = old_child;
    }
  }

  // Serialize this node. This fills in all of the fields in
  // AXNodeData except child_ids, which we handle below.
  out_update->nodes.push_back(AXNodeData());
  AXNodeData* serialized_node = &out_update->nodes.back();
  tree_->SerializeNode(node, serialized_node);
  if (serialized_node->id == client_root_->id)
    serialized_node->role = AX_ROLE_ROOT_WEB_AREA;
  serialized_node->child_ids.clear();

  // Iterate over the children, make note of the ones that are new
  // and need to be serialized, and update the ClientTreeNode
  // data structure to reflect the new tree.
  std::vector<AXSourceNode*> children_to_serialize;
  client_node->children.reserve(child_count);
  for (int i = 0; i < child_count; ++i) {
    AXSourceNode* child = tree_->GetChildAtIndex(node, i);
    int child_id = tree_->GetId(child);

    // No need to do anything more with children that aren't new;
    // the client will reuse its existing object.
    if (new_child_ids.find(child_id) == new_child_ids.end())
      continue;

    new_child_ids.erase(child_id);
    serialized_node->child_ids.push_back(child_id);
    if (client_child_id_map.find(child_id) != client_child_id_map.end()) {
      ClientTreeNode* reused_child = client_child_id_map[child_id];
      client_node->children.push_back(reused_child);
    } else {
      ClientTreeNode* new_child = new ClientTreeNode();
      new_child->id = child_id;
      new_child->parent = client_node;
      client_node->children.push_back(new_child);
      client_id_map_[child_id] = new_child;
      children_to_serialize.push_back(child);
    }
  }

  // Serialize all of the new children, recursively.
  for (size_t i = 0; i < children_to_serialize.size(); ++i)
    SerializeChangedNodes(children_to_serialize[i], ids_serialized, out_update);
}

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_SERIALIZER_H_
