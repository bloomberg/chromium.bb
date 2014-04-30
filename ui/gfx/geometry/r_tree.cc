// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/r_tree.h"

#include <algorithm>
#include <limits>

#include "base/logging.h"

namespace {

// Returns the center coordinates of the given rectangle.
gfx::Vector2d CenterOfRect(const gfx::Rect& rect) {
  return rect.OffsetFromOrigin() +
         gfx::Vector2d(rect.width() / 2, rect.height() / 2);
}
}

namespace gfx {

RTree::Node::Node(int level) : level_(level), parent_(NULL), key_(0) {
}

RTree::Node::Node(const Rect& rect, intptr_t key)
    : rect_(rect), level_(-1), parent_(NULL), key_(key) {
}

RTree::Node::~Node() {
  Clear();
}

void RTree::Node::Clear() {
  // Iterate through children and delete them all.
  children_.clear();
  key_ = 0;
}

void RTree::Node::Query(const Rect& query_rect,
                        base::hash_set<intptr_t>* matches_out) const {
  // Check own bounding box for intersection, can cull all children if no
  // intersection.
  if (!rect_.Intersects(query_rect)) {
    return;
  }

  // Conversely if we are completely contained within the query rect we can
  // confidently skip all bounds checks for ourselves and all our children.
  if (query_rect.Contains(rect_)) {
    GetAllValues(matches_out);
    return;
  }

  // We intersect the query rect but we are not are not contained within it.
  // If we are a record node, then add our record value. Otherwise we must
  // query each of our children in turn.
  if (key_) {
    DCHECK_EQ(level_, -1);
    matches_out->insert(key_);
  } else {
    for (size_t i = 0; i < children_.size(); ++i) {
      // Sanity-check our children.
      Node* node = children_[i];
      DCHECK_EQ(node->parent_, this);
      DCHECK_EQ(level_ - 1, node->level_);
      DCHECK(rect_.Contains(node->rect_));
      node->Query(query_rect, matches_out);
    }
  }
}

void RTree::Node::RecomputeBounds() {
  RecomputeBoundsNoParents();
  // Recompute our parent's bounds recursively up to the root.
  if (parent_) {
    parent_->RecomputeBounds();
  }
}

void RTree::Node::RemoveNodesForReinsert(size_t number_to_remove,
                                         ScopedVector<Node>* nodes) {
  DCHECK_GE(children_.size(), number_to_remove);

  // Sort our children by their distance from the center of their rectangles to
  // the center of our bounding rectangle.
  std::sort(children_.begin(),
            children_.end(),
            &RTree::Node::CompareCenterDistanceFromParent);

  // Add lowest distance nodes from our children list to the returned vector.
  nodes->insert(
      nodes->end(), children_.begin(), children_.begin() + number_to_remove);
  // Remove those same nodes from our list, without deleting them.
  children_.weak_erase(children_.begin(), children_.begin() + number_to_remove);
}

size_t RTree::Node::RemoveChild(Node* child_node, ScopedVector<Node>* orphans) {
  // Should actually be one of our children.
  DCHECK_EQ(child_node->parent_, this);

  // Add children of child_node to the orphans vector.
  orphans->insert(orphans->end(),
                  child_node->children_.begin(),
                  child_node->children_.end());
  // Remove without deletion those children from the child_node vector.
  child_node->children_.weak_clear();

  // Find an iterator to this Node in our own children_ vector.
  ScopedVector<Node>::iterator child_it = children_.end();
  for (size_t i = 0; i < children_.size(); ++i) {
    if (children_[i] == child_node) {
      child_it = children_.begin() + i;
      break;
    }
  }
  // Should have found the pointer in our children_ vector.
  DCHECK(child_it != children_.end());
  // Remove without deleting the child node from our children_ vector.
  children_.weak_erase(child_it);

  return children_.size();
}

scoped_ptr<RTree::Node> RTree::Node::RemoveAndReturnLastChild() {
  if (!children_.size())
    return scoped_ptr<Node>();

  scoped_ptr<Node> last_child(children_[children_.size() - 1]);
  DCHECK_EQ(last_child->parent_, this);
  children_.weak_erase(children_.begin() + children_.size() - 1);
  // Invalidate parent, as this child may even become the new root.
  last_child->parent_ = NULL;
  return last_child.Pass();
}

// Uses the R*-Tree algorithm CHOOSELEAF proposed by Beckmann et al.
RTree::Node* RTree::Node::ChooseSubtree(Node* node) {
  // Should never be called on a record node.
  DCHECK(!key_);
  DCHECK(level_ >= 0);
  DCHECK(node);

  // If we are a parent of nodes on the provided node level, we are done.
  if (level_ == node->level_ + 1)
    return this;

  // We are an internal node, and thus guaranteed to have children.
  DCHECK_GT(children_.size(), 0U);

  // Iterate over all children to find best candidate for insertion.
  Node* best_candidate = NULL;

  // Precompute a vector of expanded rects, used both by LeastOverlapIncrease
  // and LeastAreaEnlargement.
  std::vector<Rect> expanded_rects;
  expanded_rects.reserve(children_.size());
  for (size_t i = 0; i < children_.size(); ++i) {
    Rect expanded_rect(node->rect_);
    expanded_rect.Union(children_[i]->rect_);
    expanded_rects.push_back(expanded_rect);
  }

  // For parents of leaf nodes, we pick the node that will cause the least
  // increase in overlap by the addition of this new node. This may detect a
  // tie, in which case it will return NULL.
  if (level_ == 1)
    best_candidate = LeastOverlapIncrease(node->rect_, expanded_rects);

  // For non-parents of leaf nodes, or for parents of leaf nodes with ties in
  // overlap increase, we choose the subtree with least area enlargement caused
  // by the addition of the new node.
  if (!best_candidate)
    best_candidate = LeastAreaEnlargement(node->rect_, expanded_rects);

  DCHECK(best_candidate);
  return best_candidate->ChooseSubtree(node);
}

RTree::Node* RTree::Node::LeastAreaEnlargement(
    const Rect& node_rect,
    const std::vector<Rect>& expanded_rects) {
  Node* best_node = NULL;
  int least_area_enlargement = std::numeric_limits<int>::max();
  for (size_t i = 0; i < children_.size(); ++i) {
    Node* candidate_node = children_[i];
    int area_change = expanded_rects[i].size().GetArea() -
                      candidate_node->rect_.size().GetArea();
    if (area_change < least_area_enlargement) {
      best_node = candidate_node;
      least_area_enlargement = area_change;
    } else if (area_change == least_area_enlargement) {
      // Ties are broken by choosing entry with least area.
      DCHECK(best_node);
      if (candidate_node->rect_.size().GetArea() <
          best_node->rect_.size().GetArea()) {
        best_node = candidate_node;
      }
    }
  }

  DCHECK(best_node);
  return best_node;
}

RTree::Node* RTree::Node::LeastOverlapIncrease(
    const Rect& node_rect,
    const std::vector<Rect>& expanded_rects) {
  Node* best_node = NULL;
  bool has_tied_node = false;
  int least_overlap_increase = std::numeric_limits<int>::max();
  for (size_t i = 0; i < children_.size(); ++i) {
    int overlap_increase =
        OverlapIncreaseToAdd(node_rect, i, expanded_rects[i]);
    if (overlap_increase < least_overlap_increase) {
      least_overlap_increase = overlap_increase;
      best_node = children_[i];
      has_tied_node = false;
    } else if (overlap_increase == least_overlap_increase) {
      has_tied_node = true;
      // If we are tied at zero there is no possible better overlap increase,
      // so we can report a tie early.
      if (overlap_increase == 0) {
        return NULL;
      }
    }
  }

  // If we ended up with a tie return NULL to report it.
  if (has_tied_node)
    return NULL;

  return best_node;
}

int RTree::Node::OverlapIncreaseToAdd(const Rect& rect,
                                      size_t candidate,
                                      const Rect& expanded_rect) {
  Node* candidate_node = children_[candidate];

  // Early-out option for when rect is contained completely within candidate.
  if (candidate_node->rect_.Contains(rect)) {
    return 0;
  }

  int total_original_overlap = 0;
  int total_expanded_overlap = 0;

  // Now calculate overlap with all other rects in this node.
  for (size_t i = 0; i < children_.size(); ++i) {
    // Skip calculating overlap with the candidate rect.
    if (i == candidate)
      continue;
    Node* overlap_node = children_[i];
    Rect overlap_rect = candidate_node->rect_;
    overlap_rect.Intersect(overlap_node->rect_);
    total_original_overlap += overlap_rect.size().GetArea();
    Rect expanded_overlap_rect = expanded_rect;
    expanded_overlap_rect.Intersect(overlap_node->rect_);
    total_expanded_overlap += expanded_overlap_rect.size().GetArea();
  }

  // Compare this overlap increase with best one to date, update best.
  int overlap_increase = total_expanded_overlap - total_original_overlap;
  return overlap_increase;
}

size_t RTree::Node::AddChild(Node* node) {
  DCHECK(node);
  // Sanity-check that the level of the child being added is one more than ours.
  DCHECK_EQ(level_ - 1, node->level_);
  node->parent_ = this;
  children_.push_back(node);
  rect_.Union(node->rect_);
  return children_.size();
}

RTree::Node* RTree::Node::Split(size_t min_children, size_t max_children) {
  // Please don't attempt to split a record Node.
  DCHECK(!key_);
  // We should have too many children to begin with.
  DCHECK_GT(children_.size(), max_children);
  // First determine if splitting along the horizontal or vertical axis. We
  // sort the rectangles of our children by lower then upper values along both
  // horizontal and vertical axes.
  std::vector<Node*> vertical_sort(children_.get());
  std::vector<Node*> horizontal_sort(children_.get());
  std::sort(vertical_sort.begin(),
            vertical_sort.end(),
            &RTree::Node::CompareVertical);
  std::sort(horizontal_sort.begin(),
            horizontal_sort.end(),
            &RTree::Node::CompareHorizontal);

  // We will be examining the bounding boxes of different splits of our children
  // sorted along each axis. Here we precompute the bounding boxes of these
  // distributions. For the low bounds the ith element can be considered the
  // union of all rects [0,i] in the relevant sorted axis array.
  std::vector<Rect> low_vertical_bounds;
  std::vector<Rect> low_horizontal_bounds;
  BuildLowBounds(vertical_sort,
                 horizontal_sort,
                 &low_vertical_bounds,
                 &low_horizontal_bounds);

  // For the high bounds the ith element can be considered the union of all
  // rects [i, children_.size()) in the relevant sorted axis array.
  std::vector<Rect> high_vertical_bounds;
  std::vector<Rect> high_horizontal_bounds;
  BuildHighBounds(vertical_sort,
                  horizontal_sort,
                  &high_vertical_bounds,
                  &high_horizontal_bounds);

  bool is_vertical_split = ChooseSplitAxis(low_vertical_bounds,
                                           high_vertical_bounds,
                                           low_horizontal_bounds,
                                           high_horizontal_bounds,
                                           min_children,
                                           max_children);

  // Lastly we determine optimal index and do the split.
  Node* sibling = NULL;
  if (is_vertical_split) {
    size_t split_index = ChooseSplitIndex(
        min_children, max_children, low_vertical_bounds, high_vertical_bounds);
    sibling = DivideChildren(
        low_vertical_bounds, high_vertical_bounds, vertical_sort, split_index);
  } else {
    size_t split_index = ChooseSplitIndex(min_children,
                                          max_children,
                                          low_horizontal_bounds,
                                          high_horizontal_bounds);
    sibling = DivideChildren(low_horizontal_bounds,
                             high_horizontal_bounds,
                             horizontal_sort,
                             split_index);
  }

  return sibling;
}

// static
void RTree::Node::BuildLowBounds(const std::vector<Node*>& vertical_sort,
                                 const std::vector<Node*>& horizontal_sort,
                                 std::vector<Rect>* vertical_bounds,
                                 std::vector<Rect>* horizontal_bounds) {
  DCHECK_EQ(vertical_sort.size(), horizontal_sort.size());
  Rect vertical_bounds_rect;
  Rect horizontal_bounds_rect;
  vertical_bounds->reserve(vertical_sort.size());
  horizontal_bounds->reserve(horizontal_sort.size());
  for (size_t i = 0; i < vertical_sort.size(); ++i) {
    vertical_bounds_rect.Union(vertical_sort[i]->rect_);
    horizontal_bounds_rect.Union(horizontal_sort[i]->rect_);
    vertical_bounds->push_back(vertical_bounds_rect);
    horizontal_bounds->push_back(horizontal_bounds_rect);
  }
}

// static
void RTree::Node::BuildHighBounds(const std::vector<Node*>& vertical_sort,
                                  const std::vector<Node*>& horizontal_sort,
                                  std::vector<Rect>* vertical_bounds,
                                  std::vector<Rect>* horizontal_bounds) {
  DCHECK_EQ(vertical_sort.size(), horizontal_sort.size());
  Rect vertical_bounds_rect;
  Rect horizontal_bounds_rect;
  vertical_bounds->resize(vertical_sort.size());
  horizontal_bounds->resize(horizontal_sort.size());
  for (int i = static_cast<int>(vertical_sort.size()) - 1; i >= 0; --i) {
    vertical_bounds_rect.Union(vertical_sort[i]->rect_);
    horizontal_bounds_rect.Union(horizontal_sort[i]->rect_);
    vertical_bounds->at(i) = vertical_bounds_rect;
    horizontal_bounds->at(i) = horizontal_bounds_rect;
  }
}

// static
bool RTree::Node::ChooseSplitAxis(
    const std::vector<Rect>& low_vertical_bounds,
    const std::vector<Rect>& high_vertical_bounds,
    const std::vector<Rect>& low_horizontal_bounds,
    const std::vector<Rect>& high_horizontal_bounds,
    size_t min_children,
    size_t max_children) {
  // Examine the possible distributions of each sorted list by iterating through
  // valid split points p, min_children <= p <= max_children - min_children, and
  // computing the sums of the margins of the bounding boxes in each group.
  // Smallest margin sum will determine split axis.
  int smallest_horizontal_margin_sum = std::numeric_limits<int>::max();
  int smallest_vertical_margin_sum = std::numeric_limits<int>::max();
  for (size_t p = min_children; p < max_children - min_children; ++p) {
    int horizontal_margin_sum =
        low_horizontal_bounds[p].width() + low_horizontal_bounds[p].height() +
        high_horizontal_bounds[p].width() + high_horizontal_bounds[p].height();
    int vertical_margin_sum =
        low_vertical_bounds[p].width() + low_vertical_bounds[p].height() +
        high_vertical_bounds[p].width() + high_vertical_bounds[p].height();
    // Update margin minima if necessary.
    smallest_horizontal_margin_sum =
        std::min(horizontal_margin_sum, smallest_horizontal_margin_sum);
    smallest_vertical_margin_sum =
        std::min(vertical_margin_sum, smallest_vertical_margin_sum);
  }

  // Split along the axis perpendicular to the axis with the overall smallest
  // margin sum. Meaning the axis sort resulting in two boxes with the smallest
  // combined margin will become the axis along which the sorted rectangles are
  // distributed to the two Nodes.
  bool is_vertical_split =
      smallest_vertical_margin_sum < smallest_horizontal_margin_sum;
  return is_vertical_split;
}

RTree::Node* RTree::Node::DivideChildren(
    const std::vector<Rect>& low_bounds,
    const std::vector<Rect>& high_bounds,
    const std::vector<Node*>& sorted_children,
    size_t split_index) {
  Node* sibling = new Node(level_);
  sibling->parent_ = parent_;
  rect_ = low_bounds[split_index - 1];
  sibling->rect_ = high_bounds[split_index];
  // Our own children_ vector is unsorted, so we wipe it out and divide the
  // sorted bounds rects between ourselves and our sibling.
  children_.weak_clear();
  children_.insert(children_.end(),
                   sorted_children.begin(),
                   sorted_children.begin() + split_index);
  sibling->children_.insert(sibling->children_.end(),
                            sorted_children.begin() + split_index,
                            sorted_children.end());

  // Fix up sibling parentage or it's gonna be an awkward Thanksgiving.
  for (size_t i = 0; i < sibling->children_.size(); ++i) {
    sibling->children_[i]->parent_ = sibling;
  }

  return sibling;
}

void RTree::Node::SetRect(const Rect& rect) {
  // Record nodes only, please.
  DCHECK(key_);
  rect_ = rect;
}

// Returns all contained record_node values for this node and all children.
void RTree::Node::GetAllValues(base::hash_set<intptr_t>* matches_out) const {
  if (key_) {
    DCHECK_EQ(level_, -1);
    matches_out->insert(key_);
  } else {
    for (size_t i = 0; i < children_.size(); ++i) {
      Node* node = children_[i];
      // Sanity-check our children.
      DCHECK_EQ(node->parent_, this);
      DCHECK_EQ(level_ - 1, node->level_);
      DCHECK(rect_.Contains(node->rect_));
      node->GetAllValues(matches_out);
    }
  }
}

// static
bool RTree::Node::CompareVertical(Node* a, Node* b) {
  // Sort nodes by top coordinate first.
  if (a->rect_.y() < b->rect_.y()) {
    return true;
  } else if (a->rect_.y() == b->rect_.y()) {
    // If top coordinate is equal, sort by lowest bottom coordinate.
    return a->rect_.height() < b->rect_.height();
  }
  return false;
}

// static
bool RTree::Node::CompareHorizontal(Node* a, Node* b) {
  // Sort nodes by left coordinate first.
  if (a->rect_.x() < b->rect_.x()) {
    return true;
  } else if (a->rect_.x() == b->rect_.x()) {
    // If left coordinate is equal, sort by lowest right coordinate.
    return a->rect_.width() < b->rect_.width();
  }
  return false;
}

// Sort these two nodes by the distance of the center of their rects from the
// center of their parent's rect. We don't bother with square roots because we
// are only comparing the two values for sorting purposes.
// static
bool RTree::Node::CompareCenterDistanceFromParent(Node* a, Node* b) {
  // This comparison assumes the nodes have the same parent.
  DCHECK(a->parent_ == b->parent_);
  // This comparison requires that each node have a parent.
  DCHECK(a->parent_);
  // Sanity-check level_ of these nodes is equal.
  DCHECK_EQ(a->level_, b->level_);
  // Also the parent of the nodes should have level one higher.
  DCHECK_EQ(a->level_, a->parent_->level_ - 1);

  // Find the parent.
  Node* p = a->parent();

  Vector2d p_center = CenterOfRect(p->rect_);
  Vector2d a_center = CenterOfRect(a->rect_);
  Vector2d b_center = CenterOfRect(b->rect_);

  return (a_center - p_center).LengthSquared() <
         (b_center - p_center).LengthSquared();
}

size_t RTree::Node::ChooseSplitIndex(size_t min_children,
                                     size_t max_children,
                                     const std::vector<Rect>& low_bounds,
                                     const std::vector<Rect>& high_bounds) {
  int smallest_overlap_area = std::numeric_limits<int>::max();
  int smallest_combined_area = std::numeric_limits<int>::max();
  size_t optimal_split_index = 0;
  for (size_t p = min_children; p < max_children - min_children; ++p) {
    Rect overlap_bounds = low_bounds[p];
    overlap_bounds.Union(high_bounds[p]);
    int overlap_area = overlap_bounds.size().GetArea();
    if (overlap_area < smallest_overlap_area) {
      smallest_overlap_area = overlap_area;
      smallest_combined_area =
          low_bounds[p].size().GetArea() + high_bounds[p].size().GetArea();
      optimal_split_index = p;
    } else if (overlap_area == smallest_overlap_area) {
      // Break ties with smallest combined area of the two bounding boxes.
      int combined_area =
          low_bounds[p].size().GetArea() + high_bounds[p].size().GetArea();
      if (combined_area < smallest_combined_area) {
        smallest_combined_area = combined_area;
        optimal_split_index = p;
      }
    }
  }

  // optimal_split_index currently points at the last element in the first set,
  // so advance it by 1 to point at the first element in the second set.
  return optimal_split_index + 1;
}

void RTree::Node::RecomputeBoundsNoParents() {
  // Clear our rectangle, then reset it to union of our children.
  rect_.SetRect(0, 0, 0, 0);
  for (size_t i = 0; i < children_.size(); ++i) {
    rect_.Union(children_[i]->rect_);
  }
}

RTree::RTree(size_t min_children, size_t max_children)
    : root_(new Node(0)),
      min_children_(min_children),
      max_children_(max_children) {
  // R-Trees require min_children >= 2
  DCHECK_GE(min_children_, 2U);
  // R-Trees also require min_children <= max_children / 2
  DCHECK_LE(min_children_, max_children_ / 2U);
  root_.reset(new Node(0));
}

RTree::~RTree() {
  Clear();
}

void RTree::Insert(const Rect& rect, intptr_t key) {
  // Non-NULL keys, please.
  DCHECK(key);

  Node* record_node = NULL;
  // Check if this key is already present in the tree.
  base::hash_map<intptr_t, Node*>::iterator it = record_map_.find(key);
  if (it != record_map_.end()) {
    // We will re-use this node structure, regardless of re-insert or return.
    record_node = it->second;
    // If the new rect and the current rect are identical we can skip rest of
    // Insert() as nothing has changed.
    if (record_node->rect() == rect)
      return;

    // Remove the node from the tree in its current position.
    RemoveNode(record_node);

    // If we are replacing this key with an empty rectangle we just remove the
    // old node from the list and return, thus preventing insertion of empty
    // rectangles into our spatial database.
    if (rect.IsEmpty()) {
      record_map_.erase(it);
      delete record_node;
      return;
    }

    // Reset the rectangle to the new value.
    record_node->SetRect(rect);
  } else {
    if (rect.IsEmpty())
      return;
    // Build a new record Node for insertion in to tree.
    record_node = new Node(rect, key);
    // Add this new node to our map, for easy retrieval later.
    record_map_.insert(std::make_pair(key, record_node));
  }

  // Call internal Insert with this new node and allowing all re-inserts.
  int starting_level = -1;
  InsertNode(record_node, &starting_level);
}

void RTree::Remove(intptr_t key) {
  // Search the map for the leaf parent that has the provided record.
  base::hash_map<intptr_t, Node*>::iterator it = record_map_.find(key);
  // If not in the map it's not in the tree, we're done.
  if (it == record_map_.end())
    return;

  Node* node = it->second;
  // Remove this node from the map.
  record_map_.erase(it);
  // Now remove it from the RTree.
  RemoveNode(node);
  delete node;

  // Lastly check the root. If it has only one non-leaf child, delete it and
  // replace it with its child.
  if (root_->count() == 1 && root_->level() > 0) {
    scoped_ptr<Node> new_root(root_->RemoveAndReturnLastChild());
    root_.swap(new_root);
  }
}

void RTree::Query(const Rect& query_rect,
                  base::hash_set<intptr_t>* matches_out) const {
  root_->Query(query_rect, matches_out);
}

void RTree::Clear() {
  record_map_.clear();
  root_.reset(new Node(0));
}

void RTree::InsertNode(Node* node, int* highest_reinsert_level) {
  // Find the most appropriate parent to insert node into.
  Node* parent = root_->ChooseSubtree(node);
  DCHECK(parent);
  // Verify ChooseSubtree returned a Node at the correct level.
  DCHECK_EQ(parent->level(), node->level() + 1);
  Node* insert_node = node;
  Node* insert_parent = parent;
  Node* needs_bounds_recomputed = insert_parent->parent();
  ScopedVector<Node> reinserts;
  // Attempt to insert the Node, if this overflows the Node we must handle it.
  while (insert_parent &&
         insert_parent->AddChild(insert_node) > max_children_) {
    // If we have yet to re-insert nodes at this level during this data insert,
    // and we're not at the root, R*-Tree calls for re-insertion of some of the
    // nodes, resulting in a better balance on the tree.
    if (insert_parent->parent() &&
        insert_parent->level() > *highest_reinsert_level) {
      insert_parent->RemoveNodesForReinsert(max_children_ / 3, &reinserts);
      // Adjust highest_reinsert_level to this level.
      *highest_reinsert_level = insert_parent->level();
      // We didn't create any new nodes so we have nothing new to insert.
      insert_node = NULL;
      // RemoveNodesForReinsert() does not recompute bounds, so mark it.
      needs_bounds_recomputed = insert_parent;
      break;
    }

    // Split() will create a sibling to insert_parent both of which will have
    // valid bounds, but this invalidates their parent's bounds.
    insert_node = insert_parent->Split(min_children_, max_children_);
    insert_parent = insert_parent->parent();
    needs_bounds_recomputed = insert_parent;
  }

  // If we have a Node to insert, and we hit the root of the current tree,
  // we create a new root which is the parent of the current root and the
  // insert_node
  if (!insert_parent && insert_node) {
    Node* old_root = root_.release();
    root_.reset(new Node(old_root->level() + 1));
    root_->AddChild(old_root);
    root_->AddChild(insert_node);
  }

  // Recompute bounds along insertion path.
  if (needs_bounds_recomputed) {
    needs_bounds_recomputed->RecomputeBounds();
  }

  // Complete re-inserts, if any.
  for (size_t i = 0; i < reinserts.size(); ++i) {
    InsertNode(reinserts[i], highest_reinsert_level);
  }

  // Clear out reinserts without deleting any of the children, as they have been
  // re-inserted into the tree.
  reinserts.weak_clear();
}

void RTree::RemoveNode(Node* node) {
  // We need to remove this node from its parent.
  Node* parent = node->parent();
  // Record nodes are never allowed as the root, so we should always have a
  // parent.
  DCHECK(parent);
  // Should always be a leaf that had the record.
  DCHECK_EQ(parent->level(), 0);
  ScopedVector<Node> orphans;
  Node* child = node;

  // Traverse up the tree, removing the child from each parent and deleting
  // parent nodes, until we either encounter the root of the tree or a parent
  // that still has sufficient children.
  while (parent && parent->RemoveChild(child, &orphans) < min_children_) {
    if (child != node) {
      delete child;
    }
    child = parent;
    parent = parent->parent();
  }

  // If we stopped deleting nodes up the tree before encountering the root,
  // we'll need to fix up the bounds from the first parent we didn't delete
  // up to the root.
  if (parent) {
    parent->RecomputeBounds();
  }

  // Now re-insert each of the orphaned nodes back into the tree.
  for (size_t i = 0; i < orphans.size(); ++i) {
    int starting_level = -1;
    InsertNode(orphans[i], &starting_level);
  }

  // Clear out the orphans list without deleting any of the children, as they
  // have been re-inserted into the tree.
  orphans.weak_clear();
}

}  // namespace gfx
