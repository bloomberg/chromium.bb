// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_TREE_NODE_MODEL_H_
#define UI_BASE_MODELS_TREE_NODE_MODEL_H_
#pragma once

#include <algorithm>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/scoped_ptr.h"
#include "base/scoped_vector.h"
#include "base/stl_util-inl.h"
#include "base/string16.h"
#include "ui/base/models/tree_model.h"

namespace ui {

// TreeNodeModel and TreeNodes provide an implementation of TreeModel around
// TreeNodes. TreeNodes form a directed acyclic graph.
//
// TreeNodes own their children, so that deleting a node deletes all
// descendants.
//
// TreeNodes do NOT maintain a pointer back to the model. As such, if you
// are using TreeNodes with a TreeNodeModel you will need to notify the observer
// yourself any time you make any change directly to the TreeNodes. For example,
// if you directly invoke SetTitle on a node it does not notify the
// observer, you will need to do it yourself. This includes the following
// methods: SetTitle, Remove and Add. TreeNodeModel provides cover
// methods that mutate the TreeNodes and notify the observer. If you are using
// TreeNodes with a TreeNodeModel use the cover methods to save yourself the
// headache.
//
// The following example creates a TreeNode with two children and then
// creates a TreeNodeModel from it:
//
// TreeNodeWithValue<int> root =
//     new TreeNodeWithValue<int>(ASCIIToUTF16("root"), 0);
// root.add(new TreeNodeWithValue<int>(ASCIIToUTF16("child 1"), 1));
// root.add(new TreeNodeWithValue<int>(ASCIIToUTF16("child 2"), 1));
// TreeNodeModel<TreeNodeWithValue<int>>* model =
//     new TreeNodeModel<TreeNodeWithValue<int>>(root);
//
// Two variants of TreeNode are provided here:
//
// . TreeNode itself is intended for subclassing. It has one type parameter
//   that corresponds to the type of the node. When subclassing use your class
//   name as the type parameter, eg:
//   class MyTreeNode : public TreeNode<MyTreeNode> .
// . TreeNodeWithValue is a trivial subclass of TreeNode that has one type
//   type parameter: a value type that is associated with the node.
//
// Which you use depends upon the situation. If you want to subclass and add
// methods, then use TreeNode. If you don't need any extra methods and just
// want to associate a value with each node, then use TreeNodeWithValue.
//
// Regardless of which TreeNode you use, if you are using the nodes with a
// TreeView take care to notify the observer when mutating the nodes.

template <class NodeType>
class TreeNodeModel;

// TreeNode -------------------------------------------------------------------

template <class NodeType>
class TreeNode : public TreeModelNode {
 public:
  TreeNode() : parent_(NULL) { }

  explicit TreeNode(const string16& title)
      : title_(title), parent_(NULL) {}

  virtual ~TreeNode() {
  }

  // Adds the specified child node.
  virtual void Add(int index, NodeType* child) {
    DCHECK(child);
    DCHECK_LE(0, index);
    DCHECK_GE(GetChildCount(), index);
    // If the node has a parent, remove it from its parent.
    NodeType* node_parent = child->parent();
    if (node_parent)
      node_parent->Remove(node_parent->GetIndexOf(child));
    child->parent_ = static_cast<NodeType*>(this);
    children_->insert(children_->begin() + index, child);
  }

  // Removes the node by index. This does NOT delete the specified node, it is
  // up to the caller to delete it when done.
  virtual NodeType* Remove(int index) {
    DCHECK(index >= 0 && index < GetChildCount());
    NodeType* node = GetChild(index);
    node->parent_ = NULL;
    children_->erase(index + children_->begin());
    return node;
  }

  // Removes all the children from this node. This does NOT delete the nodes.
  void RemoveAll() {
    for (size_t i = 0; i < children_->size(); ++i)
      children_[i]->parent_ = NULL;
    children_->clear();
  }

  // Returns the number of children.
  int GetChildCount() const {
    return static_cast<int>(children_->size());
  }

  // Returns the number of all nodes in teh subtree rooted at this node,
  // including this node.
  int GetTotalNodeCount() const {
    int count = 1;  // Start with one to include the node itself.
    for (size_t i = 0; i < children_->size(); ++i) {
      const TreeNode<NodeType>* child = children_[i];
      count += child->GetTotalNodeCount();
    }
    return count;
  }

  // Returns a child by index.
  NodeType* GetChild(int index) {
    DCHECK(index >= 0 && index < GetChildCount());
    return children_[index];
  }
  const NodeType* GetChild(int index) const {
    DCHECK_LE(0, index);
    DCHECK_GT(GetChildCount(), index);
    return children_[index];
  }

  // Returns the parent of this object, or NULL if it's the root.
  const NodeType* parent() const { return parent_; }
  NodeType* parent() { return parent_; }

  // Returns the index of |node|, or -1 if |node| is not a child of this.
  int GetIndexOf(const NodeType* node) const {
    DCHECK(node);
    typename std::vector<NodeType*>::const_iterator i =
        std::find(children_->begin(), children_->end(), node);
    if (i != children_->end())
      return static_cast<int>(i - children_->begin());
    return -1;
  }

  // Sets the title of the node.
  void SetTitle(const string16& string) {
    title_ = string;
  }

  // Returns the title of the node.
  virtual const string16& GetTitle() const {
    return title_;
  }

  // Returns true if this is the root.
  bool IsRoot() const { return (parent_ == NULL); }

  // Returns true if this == ancestor, or one of this nodes parents is
  // ancestor.
  bool HasAncestor(const NodeType* ancestor) const {
    if (ancestor == this)
      return true;
    if (!ancestor)
      return false;
    return parent_ ? parent_->HasAncestor(ancestor) : false;
  }

 protected:
  std::vector<NodeType*>& children() { return children_.get(); }

 private:
  // Title displayed in the tree.
  string16 title_;

  NodeType* parent_;

  // Children.
  ScopedVector<NodeType> children_;

  DISALLOW_COPY_AND_ASSIGN(TreeNode);
};

// TreeNodeWithValue ----------------------------------------------------------

template <class ValueType>
class TreeNodeWithValue : public TreeNode< TreeNodeWithValue<ValueType> > {
 private:
  typedef TreeNode< TreeNodeWithValue<ValueType> > ParentType;

 public:
  TreeNodeWithValue() { }

  explicit TreeNodeWithValue(const ValueType& value)
      : ParentType(string16()), value(value) { }

  TreeNodeWithValue(const string16& title, const ValueType& value)
      : ParentType(title), value(value) { }

  ValueType value;

 private:
  DISALLOW_COPY_AND_ASSIGN(TreeNodeWithValue);
};

// TreeNodeModel --------------------------------------------------------------

// TreeModel implementation intended to be used with TreeNodes.
template <class NodeType>
class TreeNodeModel : public TreeModel {
 public:
  // Creates a TreeNodeModel with the specified root node. The root is owned
  // by the TreeNodeModel.
  explicit TreeNodeModel(NodeType* root)
      : root_(root) {
  }

  virtual ~TreeNodeModel() {}

  // Observer methods, calls into ObserverList.
  virtual void AddObserver(TreeModelObserver* observer) {
    observer_list_.AddObserver(observer);
  }

  virtual void RemoveObserver(TreeModelObserver* observer) {
    observer_list_.RemoveObserver(observer);
  }

  // TreeModel methods, all forward to the nodes.
  virtual NodeType* GetRoot() { return root_.get(); }

  virtual int GetChildCount(TreeModelNode* parent) {
    DCHECK(parent);
    return AsNode(parent)->GetChildCount();
  }

  virtual NodeType* GetChild(TreeModelNode* parent, int index) {
    DCHECK(parent);
    return AsNode(parent)->GetChild(index);
  }

  virtual int GetIndexOf(TreeModelNode* parent, TreeModelNode* child) {
    DCHECK(parent);
    return AsNode(parent)->GetIndexOf(AsNode(child));
  }

  virtual TreeModelNode* GetParent(TreeModelNode* node) {
    DCHECK(node);
    return AsNode(node)->parent();
  }

  NodeType* AsNode(TreeModelNode* model_node) {
    return static_cast<NodeType*>(model_node);
  }

  // Sets the title of the specified node.
  virtual void SetTitle(TreeModelNode* node,
                        const string16& title) {
    DCHECK(node);
    AsNode(node)->SetTitle(title);
    NotifyObserverTreeNodeChanged(node);
  }

  void Add(NodeType* parent, int index, NodeType* child) {
    DCHECK(parent && child);
    parent->Add(index, child);
    NotifyObserverTreeNodesAdded(parent, index, 1);
  }

  NodeType* Remove(NodeType* parent, int index) {
    DCHECK(parent);
    NodeType* child = parent->Remove(index);
    NotifyObserverTreeNodesRemoved(parent, index, 1);
    return child;
  }

  void NotifyObserverTreeNodesAdded(NodeType* parent, int start, int count) {
    FOR_EACH_OBSERVER(TreeModelObserver,
                      observer_list_,
                      TreeNodesAdded(this, parent, start, count));
  }

  void NotifyObserverTreeNodesRemoved(NodeType* parent, int start, int count) {
    FOR_EACH_OBSERVER(TreeModelObserver,
                      observer_list_,
                      TreeNodesRemoved(this, parent, start, count));
  }

  virtual void NotifyObserverTreeNodeChanged(TreeModelNode* node) {
    FOR_EACH_OBSERVER(TreeModelObserver,
                      observer_list_,
                      TreeNodeChanged(this, node));
  }

 protected:
  ObserverList<TreeModelObserver>& observer_list() { return observer_list_; }

 private:
  // The observers.
  ObserverList<TreeModelObserver> observer_list_;
  // The root.
  scoped_ptr<NodeType> root_;

  DISALLOW_COPY_AND_ASSIGN(TreeNodeModel);
};

}  // namespace ui

#endif  // UI_BASE_MODELS_TREE_NODE_MODEL_H_
