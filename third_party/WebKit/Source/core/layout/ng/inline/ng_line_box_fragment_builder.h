// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLineBoxFragmentBuilder_h
#define NGLineBoxFragmentBuilder_h

#include "core/layout/ng/geometry/ng_logical_offset.h"
#include "core/layout/ng/inline/ng_inline_break_token.h"
#include "core/layout/ng/inline/ng_line_height_metrics.h"
#include "core/layout/ng/ng_container_fragment_builder.h"
#include "core/layout/ng/ng_positioned_float.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class ComputedStyle;
class NGInlineNode;
class NGPhysicalFragment;

class CORE_EXPORT NGLineBoxFragmentBuilder final
    : public NGContainerFragmentBuilder {
  STACK_ALLOCATED();

 public:
  NGLineBoxFragmentBuilder(NGInlineNode,
                           scoped_refptr<const ComputedStyle>,
                           NGWritingMode);

  NGLogicalSize Size() const final;

  void SetMetrics(const NGLineHeightMetrics&);
  const NGLineHeightMetrics& Metrics() const { return metrics_; }

  void AddPositionedFloat(const NGPositionedFloat&);

  // Set the break token for the fragment to build.
  // A finished break token will be attached if not set.
  void SetBreakToken(scoped_refptr<NGInlineBreakToken>);

  // A data struct to keep NGLayoutResult or fragment until the box tree
  // structures and child offsets are finalized.
  struct Child {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

    scoped_refptr<NGLayoutResult> layout_result;
    scoped_refptr<NGPhysicalFragment> fragment;
    NGLogicalOffset offset;

    bool HasFragment() const { return layout_result || fragment; }
    const NGPhysicalFragment* PhysicalFragment() const;
  };

  // A vector of Child.
  // Unlike the fragment builder, chlidren are mutable.
  // Callers can add to the fragment builder in a batch once finalized.
  class ChildList {
    STACK_ALLOCATED();

   public:
    ChildList() {}
    void operator=(ChildList&& other) {
      children_ = std::move(other.children_);
    }

    Child& operator[](unsigned i) { return children_[i]; }

    unsigned size() const { return children_.size(); }
    bool IsEmpty() const { return children_.IsEmpty(); }
    void ReserveInitialCapacity(unsigned capacity) {
      children_.ReserveInitialCapacity(capacity);
    }
    void clear() { children_.clear(); }

    using iterator = Vector<Child, 16>::iterator;
    iterator begin() { return children_.begin(); }
    iterator end() { return children_.end(); }
    using const_iterator = Vector<Child, 16>::const_iterator;
    const_iterator begin() const { return children_.begin(); }
    const_iterator end() const { return children_.end(); }

    void AddChild(scoped_refptr<NGLayoutResult>, const NGLogicalOffset&);
    void AddChild(scoped_refptr<NGPhysicalFragment>, const NGLogicalOffset&);
    // nullptr child is a placeholder until enclosing inline boxes are closed
    // and we know the final box structure and their positions. This variant
    // helps to avoid needing static_cast when adding a nullptr.
    void AddChild(std::nullptr_t, const NGLogicalOffset&);

    void MoveInBlockDirection(LayoutUnit);
    void MoveInBlockDirection(LayoutUnit, unsigned start, unsigned end);

   private:
    Vector<Child, 16> children_;
  };

  void AddChildren(ChildList&);

  // Creates the fragment. Can only be called once.
  scoped_refptr<NGLayoutResult> ToLineBoxFragment();

 private:
  NGInlineNode node_;

  NGLineHeightMetrics metrics_;
  Vector<NGPositionedFloat> positioned_floats_;

  scoped_refptr<NGInlineBreakToken> break_token_;
};

}  // namespace blink

#endif  // NGLineBoxFragmentBuilder
