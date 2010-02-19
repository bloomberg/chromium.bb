/*
 * Copyright 2010, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// A red-black tree, which is a form of a balanced binary tree. It
// supports efficient insertion, deletion and queries of comparable
// elements. The same element may be inserted multiple times. The
// algorithmic complexity of common operations is:
//
//   Insertion: O(lg(n))
//   Deletion:  O(lg(n))
//   Querying:  O(lg(n))
//
// The data type that is stored in this red-black tree must support
// the default constructor, the copy constructor, and the "<" and "=="
// operators. It must _not_ rely on having its destructor called. This
// implementation internally allocates storage in large chunks and
// does not call the destructor on each object.
//
// Note that when complex types are stored in this red/black tree, it
// is possible that single invocations of the "<" and "==" operators
// will be insufficient to describe the ordering of elements in the
// tree during queries. As a concrete example, consider the case where
// intervals are stored in the tree sorted by low endpoint. The "<"
// operator on the Interval class only compares the low endpoint, but
// the "==" operator takes into account the high endpoint as well.
// This makes the necessary logic for querying and deletion somewhat
// more complex. In order to properly handle such situations, the
// property "needs_full_ordering_comparisons" must be set to true on
// the tree.
//
// This red-black tree is designed to be _augmented_; subclasses can
// add additional and summary information to each node to efficiently
// store and index more complex data structures. A concrete example is
// the IntervalTree, which extends each node with a summary statistic
// to efficiently store one-dimensional intervals.
//
// The design of this red-black tree comes from Cormen, Leiserson,
// and Rivest, _Introduction to Algorithms_, MIT Press, 1990.

#ifndef O3D_CORE_CROSS_GPU2D_RED_BLACK_TREE_H_
#define O3D_CORE_CROSS_GPU2D_RED_BLACK_TREE_H_

#include <string>

#include "base/logging.h"
#include "core/cross/gpu2d/arena.h"

namespace o3d {
namespace gpu2d {

template<class T>
class RedBlackTree {
 public:
  // Visitor interface for walking all of the tree's elements.
  class Visitor {
   public:
    virtual ~Visitor() {}
    virtual void Visit(const T& data) = 0;
  };

  // Constructs a new red-black tree, allocating temporary objects
  // from a newly constructed Arena.
  RedBlackTree()
      : arena_(new Arena()),
        should_delete_arena_(true),
        root_(NULL),
        needs_full_ordering_comparisons_(false),
        verbose_debugging_(false) {
  }

  // Constructs a new red-black tree, allocating temporary objects
  // from the given Arena. The Arena is not destroyed when this tree
  // is.
  explicit RedBlackTree(Arena* arena)
      : arena_(arena),
        should_delete_arena_(false),
        root_(NULL),
        needs_full_ordering_comparisons_(false),
        verbose_debugging_(false) {
  }

  // Destructor deletes the internal Arena if it was not explicitly
  // passed to the constructor.
  virtual ~RedBlackTree() {
    if (should_delete_arena_) {
      delete arena_;
    }
  }

  // Inserts a datum into the tree.
  void Insert(const T& data) {
    Node* node = new Node(data);
    InsertNode(node);
  }

  // Deletes the given datum from the tree. Returns true if the datum
  // was found in the tree.
  bool Delete(const T& data) {
    Node* node = TreeSearch(data);
    if (node != NULL) {
      DeleteNode(node);
      return true;
    }
    return false;
  }

  // Returns true if the tree contains the given datum.
  bool Contains(const T& data) {
    return TreeSearch(data) != NULL;
  }

  // Visits the nodes of this tree in order, calling the visitor on
  // each one.
  void VisitInorder(Visitor* visitor) {
    if (root_ != NULL) {
      VisitInorderImpl(root_, visitor);
    }
  }

  // Returns the number of elements in the tree.
  int NumElements() {
    Counter counter;
    VisitInorder(&counter);
    return counter.count();
  }

  // Returns true if the tree's invariants are preserved.
  bool Verify() {
    int black_count;
    return VerifyFromNode(root_, &black_count);
  }

  // Dumps the tree's contents to the logging info stream for
  // debugging purposes.
  void Dump() {
    DumpFromNode(root_, 0);
  }

  // Sets whether this tree requires full ordering comparisons; see
  // the class documentation for an explanation of this property.
  void set_needs_full_ordering_comparisons(
      bool needs_full_ordering_comparisons) {
    needs_full_ordering_comparisons_ = needs_full_ordering_comparisons;
  }

  // Turns on or off verbose debugging of the tree, causing many
  // messages to be logged during insertion and other operations in
  // debug mode.
  void set_verbose_debugging(bool on_or_off) {
    verbose_debugging_ = on_or_off;
  }

 protected:
  enum Color {
    kRed = 1,
    kBlack
  };

  // The base Node class which is stored in the tree. Nodes are only
  // an internal concept; users of the tree deal only with the data
  // they store in it.
  class Node {
   public:
    // Constructor. Newly-created nodes are colored red.
    explicit Node(const T& data)
        : left_(NULL),
          right_(NULL),
          parent_(NULL),
          color_(kRed),
          data_(data) {
    }

    virtual ~Node() {
    }

    Color color() const {
      return color_;
    }

    void set_color(Color color) {
      color_ = color;
    }

    // Fetches the user data.
    const T& data() const {
      return data_;
    }

    // Copies all user-level fields from the source node, but not
    // internal fields. For example, the base implementation of this
    // method copies the "data_" field, but not the child or parent
    // fields. Any augmentation information also does not need to be
    // copied, as it will be recomputed. Subclasses must call the
    // superclass implementation.
    virtual void CopyFrom(Node* src) {
      data_ = src->data();
    }

    Node* left() const {
      return left_;
    }

    void set_left(Node* node) {
      left_ = node;
    }

    Node* right() const {
      return right_;
    }

    void set_right(Node* node) {
      right_ = node;
    }

    Node* parent() const {
      return parent_;
    }

    void set_parent(Node* node) {
      parent_ = node;
    }

   private:
    Node* left_;
    Node* right_;
    Node* parent_;
    Color color_;
    T data_;
    DISALLOW_COPY_AND_ASSIGN(Node);
  };

  // Returns the root of the tree, which is needed by some subclasses.
  Node* root() const {
    return root_;
  }

 private:
  // This virtual method is the hook that subclasses should use when
  // augmenting the red-black tree with additional per-node summary
  // information. For example, in the case of an interval tree, this
  // is used to compute the maximum endpoint of the subtree below the
  // given node based on the values in the left and right children. It
  // is guaranteed that this will be called in the correct order to
  // properly update such summary information based only on the values
  // in the left and right children. This method should return true if
  // the node's summary information changed.
  virtual bool UpdateNode(Node* node) {
    return false;
  }

  //----------------------------------------------------------------------
  // Generic binary search tree operations
  //

  // Searches the tree for the given datum.
  Node* TreeSearch(const T& data) {
    if (needs_full_ordering_comparisons_) {
      return TreeSearchFullComparisons(root_, data);
    } else {
      return TreeSearchNormal(root_, data);
    }
  }

  // Searches the tree using the normal comparison operations,
  // suitable for simple data types such as numbers.
  Node* TreeSearchNormal(Node* current, const T& data) {
    while (current != NULL) {
      if (current->data() == data) {
        return current;
      }
      if (data < current->data()) {
        current = current->left();
      } else {
        current = current->right();
      }
    }
    return NULL;
  }

  // Searches the tree using multiple comparison operations, required
  // for data types with more complex behavior such as intervals.
  Node* TreeSearchFullComparisons(Node* current, const T& data) {
    if (current == NULL) {
      return NULL;
    }
    if (data < current->data()) {
      return TreeSearchFullComparisons(current->left(), data);
    } else if (current->data() < data) {
      return TreeSearchFullComparisons(current->right(), data);
    } else {
      if (data == current->data()) {
        return current;
      } else {
        // We may need to traverse both the left and right subtrees.
        Node* result = TreeSearchFullComparisons(current->left(), data);
        if (result == NULL) {
          result = TreeSearchFullComparisons(current->right(), data);
        }
        return result;
      }
    }
  }

  // Inserts the given node into the tree.
  void TreeInsert(Node* z) {
    Node* y = NULL;
    Node* x = root_;
    while (x != NULL) {
      y = x;
      if (z->data() < x->data())
        x = x->left();
      else
        x = x->right();
    }
    z->set_parent(y);
    if (y == NULL) {
      root_ = z;
    } else {
      if (z->data() < y->data()) {
        y->set_left(z);
      } else {
        y->set_right(z);
      }
    }
  }

  // Finds the node following the given one in sequential ordering of
  // their data, or NULL if none exists.
  Node* TreeSuccessor(Node* x) {
    if (x->right() != NULL)
      return TreeMinimum(x->right());
    Node* y = x->parent();
    while (y != NULL && x == y->right()) {
      x = y;
      y = y->parent();
    }
    return y;
  }

  // Finds the minimum element in the sub-tree rooted at the given
  // node.
  Node* TreeMinimum(Node* x) {
    while (x->left() != NULL) {
      x = x->left();
    }
    return x;
  }

  // Helper for maintaining the augmented red-black tree.
  void PropagateUpdates(Node* start) {
    bool should_continue = true;
    while (start != NULL && should_continue) {
      should_continue = UpdateNode(start);
      start = start->parent();
    }
  }

  //----------------------------------------------------------------------
  // Red-Black tree operations
  //

  // Left-rotates the subtree rooted at x.
  void LeftRotate(Node* x) {
    // Set y.
    Node* y = x->right();
    // Turn y's left subtree into x's right subtree.
    x->set_right(y->left());
    if (y->left() != NULL) {
      y->left()->set_parent(x);
    }
    // Link x's parent to y.
    y->set_parent(x->parent());
    if (x->parent() == NULL) {
      root_ = y;
    } else {
      if (x == x->parent()->left()) {
        x->parent()->set_left(y);
      } else {
        x->parent()->set_right(y);
      }
    }
    // Put x on y's left.
    y->set_left(x);
    x->set_parent(y);
    // Update nodes lowest to highest.
    UpdateNode(x);
    UpdateNode(y);
  }

  // Right-rotates the subtree rooted at y.
  void RightRotate(Node* y) {
    // Set x.
    Node* x = y->left();
    // Turn x's right subtree into y's left subtree.
    y->set_left(x->right());
    if (x->right() != NULL) {
      x->right()->set_parent(y);
    }
    // Link y's parent to x.
    x->set_parent(y->parent());
    if (y->parent() == NULL) {
      root_ = x;
    } else {
      if (y == y->parent()->left()) {
        y->parent()->set_left(x);
      } else {
        y->parent()->set_right(x);
      }
    }
    // Put y on x's right.
    x->set_right(y);
    y->set_parent(x);
    // Update nodes lowest to highest.
    UpdateNode(y);
    UpdateNode(x);
  }

  // Inserts the given node into the tree.
  void InsertNode(Node* x) {
    TreeInsert(x);
    x->set_color(kRed);
    UpdateNode(x);

    DLOG_IF(INFO, verbose_debugging_) << "  RedBlackTree::InsertNode";

    // The node from which to start propagating updates upwards.
    Node* update_start = x->parent();

    while (x != root_ && x->parent()->color() == kRed) {
      if (x->parent() == x->parent()->parent()->left()) {
        Node* y = x->parent()->parent()->right();
        if (y != NULL && y->color() == kRed) {
          // Case 1
          DLOG_IF(INFO, verbose_debugging_) << "  Case 1/1";
          x->parent()->set_color(kBlack);
          y->set_color(kBlack);
          x->parent()->parent()->set_color(kRed);
          UpdateNode(x->parent());
          x = x->parent()->parent();
          UpdateNode(x);
          update_start = x->parent();
        } else {
          if (x == x->parent()->right()) {
            DLOG_IF(INFO, verbose_debugging_) << "  Case 1/2";
            // Case 2
            x = x->parent();
            LeftRotate(x);
          }
          // Case 3
          DLOG_IF(INFO, verbose_debugging_) << "  Case 1/3";
          x->parent()->set_color(kBlack);
          x->parent()->parent()->set_color(kRed);
          RightRotate(x->parent()->parent());
          update_start = x->parent()->parent();
        }
      } else {
        // Same as "then" clause with "right" and "left" exchanged.
        Node* y = x->parent()->parent()->left();
        if (y != NULL && y->color() == kRed) {
          // Case 1
          DLOG_IF(INFO, verbose_debugging_) << "  Case 2/1";
          x->parent()->set_color(kBlack);
          y->set_color(kBlack);
          x->parent()->parent()->set_color(kRed);
          UpdateNode(x->parent());
          x = x->parent()->parent();
          UpdateNode(x);
          update_start = x->parent();
        } else {
          if (x == x->parent()->left()) {
            // Case 2
            DLOG_IF(INFO, verbose_debugging_) << "  Case 2/2";
            x = x->parent();
            RightRotate(x);
          }
          // Case 3
          DLOG_IF(INFO, verbose_debugging_) << "  Case 2/3";
          x->parent()->set_color(kBlack);
          x->parent()->parent()->set_color(kRed);
          LeftRotate(x->parent()->parent());
          update_start = x->parent()->parent();
        }
      }
    }

    PropagateUpdates(update_start);

    root_->set_color(kBlack);
  }

  // Restores the red-black property to the tree after splicing out
  // a node. Note that x may be NULL, which is why x_parent must be
  // supplied.
  void DeleteFixup(Node* x, Node* x_parent) {
    while (x != root_ && (x == NULL || x->color() == kBlack)) {
      if (x == x_parent->left()) {
        // Note: the text points out that w can not be NULL.
        // The reason is not obvious from simply looking at
        // the code; it comes about from the properties of the
        // red-black tree.
        Node* w = x_parent->right();
        DCHECK(w != NULL) << "x's sibling should not be null.";
        if (w->color() == kRed) {
          // Case 1
          w->set_color(kBlack);
          x_parent->set_color(kRed);
          LeftRotate(x_parent);
          w = x_parent->right();
        }
        if ((w->left() == NULL || w->left()->color() == kBlack) &&
            (w->right() == NULL || w->right()->color() == kBlack)) {
          // Case 2
          w->set_color(kRed);
          x = x_parent;
          x_parent = x->parent();
        } else {
          if (w->right() == NULL || w->right()->color() == kBlack) {
            // Case 3
            w->left()->set_color(kBlack);
            w->set_color(kRed);
            RightRotate(w);
            w = x_parent->right();
          }
          // Case 4
          w->set_color(x_parent->color());
          x_parent->set_color(kBlack);
          if (w->right() != NULL) {
            w->right()->set_color(kBlack);
          }
          LeftRotate(x_parent);
          x = root_;
          x_parent = x->parent();
        }
      } else {
        // Same as "then" clause with "right" and "left"
        // exchanged.

        // Note: the text points out that w can not be NULL.
        // The reason is not obvious from simply looking at
        // the code; it comes about from the properties of the
        // red-black tree.
        Node* w = x_parent->left();
        DCHECK(w != NULL) << "x's sibling should not be null.";
        if (w->color() == kRed) {
          // Case 1
          w->set_color(kBlack);
          x_parent->set_color(kRed);
          RightRotate(x_parent);
          w = x_parent->left();
        }
        if ((w->right() == NULL || w->right()->color() == kBlack) &&
            (w->left() == NULL || w->left()->color() == kBlack)) {
          // Case 2
          w->set_color(kRed);
          x = x_parent;
          x_parent = x->parent();
        } else {
          if (w->left() == NULL || w->left()->color() == kBlack) {
            // Case 3
            w->right()->set_color(kBlack);
            w->set_color(kRed);
            LeftRotate(w);
            w = x_parent->left();
          }
          // Case 4
          w->set_color(x_parent->color());
          x_parent->set_color(kBlack);
          if (w->left() != NULL) {
            w->left()->set_color(kBlack);
          }
          RightRotate(x_parent);
          x = root_;
          x_parent = x->parent();
        }
      }
    }
    if (x != NULL) {
      x->set_color(kBlack);
    }
  }

  // Deletes the given node from the tree. Note that this
  // particular node may not actually be removed from the tree;
  // instead, another node might be removed and its contents
  // copied into z.
  void DeleteNode(Node* z) {
    // Y is the node to be unlinked from the tree.
    Node* y;
    if (z->left() == NULL || z->right() == NULL) {
      y = z;
    } else {
      y = TreeSuccessor(z);
    }
    // Y is guaranteed to be non-NULL at this point.
    Node* x;
    if (y->left() != NULL) {
      x = y->left();
    } else {
      x = y->right();
    }
    // X is the child of y which might potentially replace y in
    // the tree. X might be NULL at this point.
    Node* x_parent;
    if (x != NULL) {
      x->set_parent(y->parent());
      x_parent = x->parent();
    } else {
      x_parent = y->parent();
    }
    if (y->parent() == NULL) {
      root_ = x;
    } else {
      if (y == y->parent()->left()) {
        y->parent()->set_left(x);
      } else {
        y->parent()->set_right(x);
      }
    }
    // It has been found empirically that both of these upward
    // propagations are necessary.
    if (y != z) {
      z->CopyFrom(y);
      PropagateUpdates(z);
    }
    if (x_parent != NULL) {
      PropagateUpdates(x_parent);
    }
    if (y->color() == kBlack) {
      DeleteFixup(x, x_parent);
    }
  }

  // Visits the subtree rooted at the given node in order.
  void VisitInorderImpl(Node* node, Visitor* visitor) {
    if (node->left() != NULL)
      VisitInorderImpl(node->left(), visitor);
    visitor->Visit(node->data());
    if (node->right() != NULL)
      VisitInorderImpl(node->right(), visitor);
  }

  //----------------------------------------------------------------------
  // Helper class for NumElements()

  // A Visitor which simply counts the number of visited elements.
  class Counter : public Visitor {
   public:
    Counter() : count_(0) {}

    virtual void Visit(const T& data) {
      ++count_;
    }

    int count() {
      return count_;
    }

   private:
    int count_;
    DISALLOW_COPY_AND_ASSIGN(Counter);
  };

  //----------------------------------------------------------------------
  // Verification and debugging routines
  //

  // Returns true if verification succeeded. Returns in the
  // "black_count" parameter the number of black children along all
  // paths to all leaves of the given node.
  bool VerifyFromNode(Node* node, int* black_count) {
    // Base case is a leaf node.
    if (node == NULL) {
      *black_count = 1;
      return true;
    }

    // Each node is either red or black.
    if (!(node->color() == kRed || node->color() == kBlack))
      return false;

    // Every leaf (or NULL) is black.

    if (node->color() == kRed) {
      // Both of its children are black.
      if (!((node->left() == NULL || node->left()->color() == kBlack)))
        return false;
      if (!((node->right() == NULL || node->right()->color() == kBlack)))
        return false;
    }

    // Every simple path to a leaf node contains the same number of
    // black nodes.
    int left_count = 0, right_count = 0;
    bool left_valid = VerifyFromNode(node->left(), &left_count);
    bool right_valid = VerifyFromNode(node->right(), &right_count);
    if (!left_valid || !right_valid)
      return false;
    *black_count = left_count + (node->color() == kBlack ? 1 : 0);
    return left_count == right_count;
  }

  // Dumps the subtree rooted at the given node.
  void DumpFromNode(Node* node, int indentation) {
    std::string prefix;
    for (int i = 0; i < indentation; i++) {
      prefix += " ";
    }
    prefix += "-";
    if (node == NULL) {
      DLOG(INFO) << prefix;
      return;
    }
    DLOG(INFO) << prefix << " " << node->data()
               << ((node->color() == kBlack) ? " (black)" : " (red)");
    DumpFromNode(node->left(), indentation + 2);
    DumpFromNode(node->right(), indentation + 2);
  }

  //----------------------------------------------------------------------
  // Data members

  Arena* arena_;
  bool should_delete_arena_;
  Node* root_;
  bool needs_full_ordering_comparisons_;
  bool verbose_debugging_;

  DISALLOW_COPY_AND_ASSIGN(RedBlackTree);
};

}  // namespace gpu2d
}  // namespace o3d

#endif  // O3D_CORE_CROSS_GPU2D_RED_BLACK_TREE_H_

