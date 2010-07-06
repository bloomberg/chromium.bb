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


#ifndef O3D_CORE_CROSS_GPU2D_INTERVAL_TREE_H_
#define O3D_CORE_CROSS_GPU2D_INTERVAL_TREE_H_

#include <vector>

#include "base/logging.h"
#include "core/cross/gpu2d/arena.h"
#include "core/cross/gpu2d/red_black_tree.h"

namespace o3d {
namespace gpu2d {

// Class representing a closed interval which can hold an arbitrary
// type as its endpoints and a piece of user data. An important
// characteristic for the algorithms we use is that if two intervals
// have identical endpoints but different user data, they are not
// considered to be equal. This situation can arise when representing
// the vertical extents of bounding boxes of overlapping triangles,
// where the pointer to the triangle is the user data of the interval.
//
// The following constructors and operators must be implemented on
// type T:
//
//   - Copy constructor (if user data is desired)
//   - operator<
//   - operator==
//   - operator=
//
// If the UserData type is specified, it must support a copy
// constructor and assignment operator.
//
// *Note* that the destructors of type T and UserData will *not* be
// called by this class. They must not allocate any memory in their
// constructors.
//
// Note that this class requires a copy constructor and assignment
// operator in order to be stored in the red-black tree.
template<class T, class UserData = void*>
class Interval {
 public:
  // Constructor from endpoints. This constructor only works when the
  // UserData type is a pointer or other type which can be initialized
  // with NULL.
  Interval(const T& low, const T& high)
      : low_(low),
        high_(high),
        data_(NULL),
        max_high_(high) {
  }

  // Constructor from two endpoints plus explicit user data.
  Interval(const T& low, const T& high, const UserData data)
      : low_(low),
        high_(high),
        data_(data),
        max_high_(high) {
  }

  const T& low() const {
    return low_;
  }

  const T& high() const {
    return high_;
  }

  const UserData& data() const {
    return data_;
  }

  // Returns true if this interval overlaps that specified by the
  // given low and high endpoints.
  bool Overlaps(const T& low, const T& high) const {
    if (this->high() < low)
      return false;
    if (high < this->low())
      return false;
    return true;
  }

  // Returns true if this interval overlaps the other.
  bool Overlaps(const Interval& other) const {
    return Overlaps(other.low(), other.high());
  }

  // Returns true if this interval is "less" than the other. The
  // comparison is performed on the low endpoints of the intervals.
  bool operator<(const Interval& other) const {
    return low() < other.low();
  }

  // Returns true if this interval is strictly equal to the other,
  // including comparison of the user data.
  bool operator==(const Interval& other) const {
    return (low() == other.low() &&
            high() == other.high() &&
            data() == other.data());
  }

  // Summary information needed for efficient queries.
  const T& max_high() const {
    return max_high_;
  }

  // Updates the cache of the summary information for this interval.
  // It is not really const, but it does not affect the user-level
  // state, and must be declared const to obey the contract in the
  // RedBlackTree that Node::data() returns a const reference.
  void set_max_high(const T& max_high) const {
    max_high_ = max_high;
  }

 private:
  T low_;
  T high_;
  UserData data_;
  // See the documentation for set_max_high() for an explanation of
  // why this must be mutable.
  mutable T max_high_;
};

// Suppport for logging Intervals.
template<class T, class UserData>
std::ostream& operator<<(std::ostream& ostr,  // NOLINT
                         const Interval<T, UserData>& arg) {
  ostr << "[Interval (" << arg.low()
       << ", " << arg.high()
       << "), data " << arg.data() << "]";
  return ostr;
}

// An interval tree, which is a form of augmented red-black tree. It
// supports efficient (O(lg n)) insertion, removal and querying of
// intervals in the tree.
template<class T, class UserData = void*>
class IntervalTree : public RedBlackTree<Interval<T, UserData> > {
 public:
  // Typedef to reduce typing when declaring intervals to be stored in
  // this tree.
  typedef Interval<T, UserData> IntervalType;

  IntervalTree()
      : RedBlackTree<IntervalType>() {
    Init();
  }

  explicit IntervalTree(Arena* arena)
      : RedBlackTree<IntervalType>(arena) {
    Init();
  }

  // Returns all intervals in the tree which overlap the given query
  // interval. The returned intervals are sorted by increasing low
  // endpoint.
  std::vector<IntervalType> AllOverlaps(const IntervalType& interval) {
    std::vector<IntervalType> result;
    AllOverlaps(interval, result);
    return result;
  }

  // Returns all intervals in the tree which overlap the given query
  // interval. The returned intervals are sorted by increasing low
  // endpoint.
  void AllOverlaps(const IntervalType& interval,
                   std::vector<IntervalType>& result) {
    // gcc requires explicit dereference of "this" here
    SearchForOverlapsFrom(this->root(), interval, result);
  }

  // Helper to create interval objects.
  static IntervalType MakeInterval(const T& low,
                                   const T& high,
                                   const UserData data = NULL) {
    return IntervalType(low, high, data);
  }

 private:
  typedef typename RedBlackTree<IntervalType>::Node IntervalNode;

  // Initializes the tree.
  void Init() {
    // gcc requires explicit dereference of "this" here
    this->set_needs_full_ordering_comparisons(true);
  }

  // Starting from the given node, adds all overlaps with the given
  // interval to the result vector. The intervals are sorted by
  // increasing low endpoint.
  void SearchForOverlapsFrom(IntervalNode* node,
                             const IntervalType& interval,
                             std::vector<IntervalType>& res) {
    if (node == NULL)
      return;

    // Because the intervals are sorted by left endpoint, inorder
    // traversal produces results sorted as desired.

    // See whether we need to traverse the left subtree.
    IntervalNode* left = node->left();
    if (left != NULL &&
        // This is equivalent to:
        // interval.low() <= left->data().max_high()
        !(left->data().max_high() < interval.low())) {
      SearchForOverlapsFrom(left, interval, res);
    }

    // Check for overlap with current node.
    if (node->data().Overlaps(interval)) {
      res.push_back(node->data());
    }

    // See whether we need to traverse the right subtree.
    // This is equivalent to:
    // node->data().low() <= interval.high()
    if (!(interval.high() < node->data().low())) {
      SearchForOverlapsFrom(node->right(), interval, res);
    }
  }

  virtual bool UpdateNode(IntervalNode* node) {
    // Would use const T&, but need to reassign this reference in this
    // function.
    const T* cur_max = &node->data().high();
    IntervalNode* left = node->left();
    if (left != NULL) {
      if (*cur_max < left->data().max_high()) {
        cur_max = &left->data().max_high();
      }
    }
    IntervalNode* right = node->right();
    if (right != NULL) {
      if (*cur_max < right->data().max_high()) {
        cur_max = &right->data().max_high();
      }
    }
    // This is phrased like this to avoid needing operator!= on type T.
    if (!(*cur_max == node->data().max_high())) {
      node->data().set_max_high(*cur_max);
      return true;
    }
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(IntervalTree);
};

}  // namespace gpu2d
}  // namespace o3d

#endif  // O3D_CORE_CROSS_GPU2D_INTERVAL_TREE_H_

