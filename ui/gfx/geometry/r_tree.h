// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_R_TREE_H_
#define UI_GFX_GEOMETRY_R_TREE_H_

#include <vector>

#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

// Defines a heirarchical bounding rectangle data structure for Rect objects,
// associated with a generic unique key K for efficient spatial queries. The
// R*-tree algorithm is used to build the trees. Based on the papers:
//
// A Guttman 'R-trees:  a dynamic index structure for spatial searching', Proc
// ACM SIGMOD Int Conf on Management of Data, 47-57, 1984
//
// N Beckmann, H-P Kriegel, R Schneider, B Seeger 'The R*-tree: an efficient and
// robust access method for points and rectangles', Proc ACM SIGMOD Int Conf on
// Management of Data, 322-331, 1990
class GFX_EXPORT RTree {
 public:
  RTree(size_t min_children, size_t max_children);
  ~RTree();

  // Insert a new rect into the tree, associated with provided key. Note that if
  // rect is empty, this rect will not actually be inserted. Duplicate keys
  // overwrite old entries.
  void Insert(const Rect& rect, intptr_t key);

  // If present, remove the supplied key from the tree.
  void Remove(intptr_t key);

  // Fills a supplied list matches_out with all keys having bounding rects
  // intersecting query_rect.
  void Query(const Rect& query_rect,
             base::hash_set<intptr_t>* matches_out) const;

  // Removes all objects from the tree.
  void Clear();

 private:
  // Private data structure class for storing internal nodes or leaves with keys
  // of R-Trees. Note that "leaf" nodes can still have children, the children
  // will all be Nodes with non-NULL record pointers.
  class GFX_EXPORT Node {
   public:
    // Level counts from -1 for "record" Nodes, that is Nodes that wrap key
    // values, to 0 for leaf Nodes, that is Nodes that only have record
    // children, up to the root Node, which has level equal to the height of the
    // tree.
    explicit Node(int level);

    // Builds a new record Node.
    Node(const Rect& rect, intptr_t key);

    virtual ~Node();

    // Deletes all children and any attached record.
    void Clear();

    // Recursive call to build a list of rects that intersect the query_rect.
    void Query(const Rect& query_rect,
               base::hash_set<intptr_t>* matches_out) const;

    // Recompute our bounds by taking the union of all children rects. Will then
    // call RecomputeBounds() on our parent for recursive bounds recalculation
    // up to the root.
    void RecomputeBounds();

    // Removes number_to_remove nodes from this Node, and appends them to the
    // supplied list. Does not repair bounds upon completion.
    void RemoveNodesForReinsert(size_t number_to_remove,
                                ScopedVector<Node>* nodes);

    // Given a pointer to a child node within this Node, remove it from our
    // list. If that child had any children, append them to the supplied orphan
    // list. Returns the new count of this node after removal. Does not
    // recompute bounds, as this node itself may be removed if it now has too
    // few children.
    size_t RemoveChild(Node* child_node, ScopedVector<Node>* orphans);

    // Does what it says on the tin. Returns NULL if no children. Does not
    // recompute bounds.
    scoped_ptr<Node> RemoveAndReturnLastChild();

    // Given a node, returns the best fit node for insertion of that node at
    // the nodes level().
    Node* ChooseSubtree(Node* node);

    // Adds the provided node to this Node. Returns the new count of records
    // stored in the Node. Will recompute the bounds of this node after
    // addition.
    size_t AddChild(Node* node);

    // Returns a sibling to this Node with at least min_children and no greater
    // than max_children of this Node's children assigned to it, and having the
    // same parent. Bounds will be valid on both Nodes after this call.
    Node* Split(size_t min_children, size_t max_children);

    // For record nodes only, to support re-insert, allows setting the rect.
    void SetRect(const Rect& rect);

    // Returns a pointer to the parent of this Node, or NULL if no parent.
    Node* parent() const { return parent_; }

    // 0 level() would mean that this Node is a leaf. 1 would mean that this
    // Node has children that are leaves. Calling level() on root_ returns the
    // height of the tree - 1. A level of -1 means that this is a Record node.
    int level() const { return level_; }

    const Rect& rect() const { return rect_; }

    size_t count() const { return children_.size(); }

    intptr_t key() const { return key_; }

   private:
    // Returns all records stored in this node and its children.
    void GetAllValues(base::hash_set<intptr_t>* matches_out) const;

    // Used for sorting Nodes along vertical and horizontal axes
    static bool CompareVertical(Node* a, Node* b);

    static bool CompareHorizontal(Node* a, Node* b);

    static bool CompareCenterDistanceFromParent(Node* a, Node* b);

    // Returns the increase in overlap value, as defined in Beckmann et al as
    // the sum of the areas of the intersection of each |children_| rectangle
    // (excepting the candidate child) with the argument rectangle. The
    // expanded_rect argument is computed as the union of the candidate child
    // rect and the argument rect, and is included here to avoid recomputation.
    // Here the candidate child is indicated by index in |children_|, and
    // expanded_rect is the alread-computed union of candidate's rect and
    // rect.
    int OverlapIncreaseToAdd(const Rect& rect,
                             size_t candidate,
                             const Rect& expanded_rect);

    // Bounds recomputation without calling parents to do the same.
    void RecomputeBoundsNoParents();

    // Split() helper methods.
    //
    // Given two vectors of Nodes sorted by vertical or horizontal bounds, this
    // function populates two vectors of Rectangles in which the ith element is
    // the Union of all bounding rectangles [0,i] in the associated sorted array
    // of Nodes.
    static void BuildLowBounds(const std::vector<Node*>& vertical_sort,
                               const std::vector<Node*>& horizontal_sort,
                               std::vector<Rect>* vertical_bounds,
                               std::vector<Rect>* horizontal_bounds);

    // Given two vectors of Nodes sorted by vertical or horizontal bounds, this
    // function populates two vectors of Rectangles in which the ith element is
    // the Union of all bounding rectangles [i, count()) in the associated
    // sorted array of Nodes.
    static void BuildHighBounds(const std::vector<Node*>& vertical_sort,
                                const std::vector<Node*>& horizontal_sort,
                                std::vector<Rect>* vertical_bounds,
                                std::vector<Rect>* horizontal_bounds);

    // Returns true if this is a vertical split, false if a horizontal one.
    // Based on ChooseSplitAxis algorithm in Beckmann et al. Chooses the axis
    // with the lowest sum of margin values of bounding boxes.
    static bool ChooseSplitAxis(const std::vector<Rect>& low_vertical_bounds,
                                const std::vector<Rect>& high_vertical_bounds,
                                const std::vector<Rect>& low_horizontal_bounds,
                                const std::vector<Rect>& high_horizontal_bounds,
                                size_t min_children,
                                size_t max_children);

    // Used by SplitNode to calculate optimal index of split, after determining
    // along which axis to sort and split the children rectangles. Returns the
    // index to the first element in the split children as sorted by the bounds
    // vectors.
    static size_t ChooseSplitIndex(size_t min_children,
                                   size_t max_children,
                                   const std::vector<Rect>& low_bounds,
                                   const std::vector<Rect>& high_bounds);

    // Takes our children_ and divides them into a new node, starting at index
    // split_index in sorted_children.
    Node* DivideChildren(const std::vector<Rect>& low_bounds,
                         const std::vector<Rect>& high_bounds,
                         const std::vector<Node*>& sorted_children,
                         size_t split_index);

    // Returns a pointer to the child node that will result in the least overlap
    // increase with the addition of node_rect, as defined in the Beckmann et al
    // paper, or NULL if there's a tie found. Requires a precomputed vector of
    // expanded rectangles where the ith rectangle in the vector is the union of
    // |children_|[i] and node_rect.
    Node* LeastOverlapIncrease(const Rect& node_rect,
                               const std::vector<Rect>& expanded_rects);

    // Returns a pointer to the child node that will result in the least area
    // enlargement if the argument node rectangle were to be added to that
    // nodes' bounding box. Requires a precomputed vector of expanded rectangles
    // where the ith rectangle in the vector is the union of |children_|[i] and
    // node_rect.
    Node* LeastAreaEnlargement(const Rect& node_rect,
                               const std::vector<Rect>& expanded_rects);

    // This Node's bounding rectangle.
    Rect rect_;

    // The height of the node in the tree, counting from -1 at the record node
    // to 0 at the leaf up to the root node which has level equal to the height
    // of the tree.
    int level_;

    // Pointers to all of our children Nodes.
    ScopedVector<Node> children_;

    // A weak pointer to our parent Node in the RTree. The root node will have a
    // NULL value for |parent_|.
    Node* parent_;

    // If this is a record Node, then |key_| will be non-NULL and will contain
    // the key data. Otherwise, NULL.
    intptr_t key_;

    friend class RTreeTest;
    FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeBuildHighBounds);
    FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeBuildLowBounds);
    FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeChooseSplitAxisAndIndex);
    FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeChooseSubtree);
    FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeCompareCenterDistanceFromParent);
    FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeCompareHorizontal);
    FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeCompareVertical);
    FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeDivideChildren);
    FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeLeastAreaEnlargement);
    FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeLeastOverlapIncrease);
    FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeOverlapIncreaseToAdd);
    FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeRemoveAndReturnLastChild);
    FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeRemoveChildNoOrphans);
    FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeRemoveChildOrphans);
    FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeRemoveNodesForReinsert);
    FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeSplit);

    DISALLOW_COPY_AND_ASSIGN(Node);
  };

  // Supports re-insertion of Nodes based on the strategies outlined in
  // Beckmann et al.
  void InsertNode(Node* node, int* highset_reinsert_level);

  // Supports removal of nodes for tree without deletion.
  void RemoveNode(Node* node);

  // A pointer to the root node in the RTree.
  scoped_ptr<Node> root_;

  // The parameters used to define the shape of the RTree.
  size_t min_children_;
  size_t max_children_;

  // A map of supplied keys to their Node representation within the RTree, for
  // efficient retrieval of keys without requiring a bounding rect.
  base::hash_map<intptr_t, Node*> record_map_;

  friend class RTreeTest;
  FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeBuildHighBounds);
  FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeBuildLowBounds);
  FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeChooseSplitAxisAndIndex);
  FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeChooseSubtree);
  FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeCompareCenterDistanceFromParent);
  FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeCompareHorizontal);
  FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeCompareVertical);
  FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeDivideChildren);
  FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeLeastAreaEnlargement);
  FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeLeastOverlapIncrease);
  FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeOverlapIncreaseToAdd);
  FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeRemoveAndReturnLastChild);
  FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeRemoveChildNoOrphans);
  FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeRemoveChildOrphans);
  FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeRemoveNodesForReinsert);
  FRIEND_TEST_ALL_PREFIXES(RTreeTest, NodeSplit);

  DISALLOW_COPY_AND_ASSIGN(RTree);
};

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_R_TREE_H_
