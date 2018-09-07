// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_TRAVERSAL_RANGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_TRAVERSAL_RANGE_H_

#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class Node;

template <class Iterator>
class TraversalRange {
  STACK_ALLOCATED();

 public:
  using StartNodeType = typename Iterator::StartNodeType;
  explicit TraversalRange(const StartNodeType* start) : start_(start) {}
  Iterator begin() { return Iterator(start_); }
  Iterator end() { return Iterator::End(); }

 private:
  Member<const StartNodeType> start_;
};

// TODO(yoichio): Simplify iterator templates.
template <class TraversalNext>
class TraversalIteratorBase {
  STACK_ALLOCATED();

 public:
  using NodeType = typename TraversalNext::TraversalNodeType;
  NodeType& operator*() { return *current_; }
  bool operator!=(const TraversalIteratorBase& rval) const {
    return current_ != rval.current_;
  }

 protected:
  explicit TraversalIteratorBase(NodeType* current) : current_(current) {}

  Member<NodeType> current_;
};

template <class TraversalNext>
class TraversalAncestorsIterator : public TraversalIteratorBase<TraversalNext> {
  STACK_ALLOCATED();

 public:
  using StartNodeType = Node;
  using TraversalIteratorBase<TraversalNext>::current_;
  explicit TraversalAncestorsIterator(const StartNodeType* start)
      : TraversalIteratorBase<TraversalNext>(
            const_cast<StartNodeType*>(start)) {}
  void operator++() { current_ = TraversalNext::Parent(*current_); }
  static TraversalAncestorsIterator End() {
    return TraversalAncestorsIterator();
  }

 private:
  TraversalAncestorsIterator()
      : TraversalIteratorBase<TraversalNext>(nullptr) {}
};

template <class TraversalNext>
class TraversalChildrenIterator : public TraversalIteratorBase<TraversalNext> {
  STACK_ALLOCATED();

 public:
  using StartNodeType = Node;
  using TraversalIteratorBase<TraversalNext>::current_;
  explicit TraversalChildrenIterator(const StartNodeType* start)
      : TraversalIteratorBase<TraversalNext>(
            TraversalNext::FirstChild(*start)) {}
  void operator++() { current_ = TraversalNext::NextSibling(*current_); }
  static TraversalChildrenIterator End() { return TraversalChildrenIterator(); }

 private:
  TraversalChildrenIterator() : TraversalIteratorBase<TraversalNext>(nullptr) {}
};

template <class TraversalNext>
class TraversalNextIterator : public TraversalIteratorBase<TraversalNext> {
  STACK_ALLOCATED();

 public:
  using StartNodeType = typename TraversalNext::TraversalNodeType;
  using TraversalIteratorBase<TraversalNext>::current_;
  explicit TraversalNextIterator(const StartNodeType* start)
      : TraversalIteratorBase<TraversalNext>(
            const_cast<StartNodeType*>(start)) {}
  void operator++() { current_ = TraversalNext::Next(*current_); }
  static TraversalNextIterator End() { return TraversalNextIterator(nullptr); }
};

template <class TraversalNext>
class TraversalDescendantIterator
    : public TraversalIteratorBase<TraversalNext> {
  STACK_ALLOCATED();

 public:
  using StartNodeType = Node;
  using TraversalIteratorBase<TraversalNext>::current_;
  explicit TraversalDescendantIterator(const StartNodeType* start)
      : TraversalIteratorBase<TraversalNext>(
            TraversalNext::FirstWithin(*start)),
        root_(start) {}
  void operator++() { current_ = TraversalNext::Next(*current_, root_); }
  static TraversalDescendantIterator End() {
    return TraversalDescendantIterator();
  }

 private:
  TraversalDescendantIterator()
      : TraversalIteratorBase<TraversalNext>(nullptr), root_(nullptr) {}
  Member<const Node> root_;
};

template <class TraversalNext>
class TraversalInclusiveDescendantIterator
    : public TraversalIteratorBase<TraversalNext> {
  STACK_ALLOCATED();

 public:
  using StartNodeType = typename TraversalNext::TraversalNodeType;
  using NodeType = typename TraversalNext::TraversalNodeType;
  using TraversalIteratorBase<TraversalNext>::current_;
  explicit TraversalInclusiveDescendantIterator(const StartNodeType* start)
      : TraversalIteratorBase<TraversalNext>(const_cast<NodeType*>(start)),
        root_(start) {}
  void operator++() { current_ = TraversalNext::Next(*current_, root_); }
  static TraversalInclusiveDescendantIterator End() {
    return TraversalInclusiveDescendantIterator(nullptr);
  }

 private:
  Member<const StartNodeType> root_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_TRAVERSAL_RANGE_H_
