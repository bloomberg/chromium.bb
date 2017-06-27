// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/EphemeralRange.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Range.h"
#include "core/dom/Text.h"

namespace blink {

namespace {
template <typename Strategy>
Node* CommonAncestorContainerNode(const Node* container_a,
                                  const Node* container_b) {
  if (!container_a || !container_b)
    return nullptr;
  return Strategy::CommonAncestor(*container_a, *container_b);
}
}

template <typename Strategy>
EphemeralRangeTemplate<Strategy>::EphemeralRangeTemplate(
    const PositionTemplate<Strategy>& start,
    const PositionTemplate<Strategy>& end)
    : start_position_(start),
      end_position_(end)
#if DCHECK_IS_ON()
      ,
      dom_tree_version_(start.IsNull() ? 0
                                       : start.GetDocument()->DomTreeVersion())
#endif
{
  if (start_position_.IsNull()) {
    DCHECK(end_position_.IsNull());
    return;
  }
  DCHECK(end_position_.IsNotNull());
  DCHECK_EQ(start_position_.GetDocument(), end_position_.GetDocument());
  DCHECK(start_position_.IsConnected());
  DCHECK(end_position_.IsConnected());
  DCHECK_LE(start_position_, end_position_);
}

template <typename Strategy>
EphemeralRangeTemplate<Strategy>::EphemeralRangeTemplate(
    const EphemeralRangeTemplate<Strategy>& other)
    : EphemeralRangeTemplate(other.start_position_, other.end_position_) {
  DCHECK(other.IsValid());
}

template <typename Strategy>
EphemeralRangeTemplate<Strategy>::EphemeralRangeTemplate(
    const PositionTemplate<Strategy>& position)
    : EphemeralRangeTemplate(position, position) {}

template <typename Strategy>
EphemeralRangeTemplate<Strategy>::EphemeralRangeTemplate(const Range* range) {
  if (!range)
    return;
  DCHECK(range->IsConnected());
  start_position_ = FromPositionInDOMTree<Strategy>(range->StartPosition());
  end_position_ = FromPositionInDOMTree<Strategy>(range->EndPosition());
#if DCHECK_IS_ON()
  dom_tree_version_ = range->OwnerDocument().DomTreeVersion();
#endif
}

template <typename Strategy>
EphemeralRangeTemplate<Strategy>::EphemeralRangeTemplate() {}

template <typename Strategy>
EphemeralRangeTemplate<Strategy>::~EphemeralRangeTemplate() {}

template <typename Strategy>
EphemeralRangeTemplate<Strategy>& EphemeralRangeTemplate<Strategy>::operator=(
    const EphemeralRangeTemplate<Strategy>& other) {
  DCHECK(other.IsValid());
  start_position_ = other.start_position_;
  end_position_ = other.end_position_;
#if DCHECK_IS_ON()
  dom_tree_version_ = other.dom_tree_version_;
#endif
  return *this;
}

template <typename Strategy>
bool EphemeralRangeTemplate<Strategy>::operator==(
    const EphemeralRangeTemplate<Strategy>& other) const {
  return StartPosition() == other.StartPosition() &&
         EndPosition() == other.EndPosition();
}

template <typename Strategy>
bool EphemeralRangeTemplate<Strategy>::operator!=(
    const EphemeralRangeTemplate<Strategy>& other) const {
  return !operator==(other);
}

template <typename Strategy>
Document& EphemeralRangeTemplate<Strategy>::GetDocument() const {
  DCHECK(IsNotNull());
  return *start_position_.GetDocument();
}

template <typename Strategy>
PositionTemplate<Strategy> EphemeralRangeTemplate<Strategy>::StartPosition()
    const {
  DCHECK(IsValid());
  return start_position_;
}

template <typename Strategy>
PositionTemplate<Strategy> EphemeralRangeTemplate<Strategy>::EndPosition()
    const {
  DCHECK(IsValid());
  return end_position_;
}

template <typename Strategy>
Node* EphemeralRangeTemplate<Strategy>::CommonAncestorContainer() const {
  return CommonAncestorContainerNode<Strategy>(
      start_position_.ComputeContainerNode(),
      end_position_.ComputeContainerNode());
}

template <typename Strategy>
bool EphemeralRangeTemplate<Strategy>::IsCollapsed() const {
  DCHECK(IsValid());
  return start_position_ == end_position_;
}

template <typename Strategy>
typename EphemeralRangeTemplate<Strategy>::RangeTraversal
EphemeralRangeTemplate<Strategy>::Nodes() const {
  return RangeTraversal(start_position_.NodeAsRangeFirstNode(),
                        end_position_.NodeAsRangePastLastNode());
}

template <typename Strategy>
EphemeralRangeTemplate<Strategy>
EphemeralRangeTemplate<Strategy>::RangeOfContents(const Node& node) {
  return EphemeralRangeTemplate<Strategy>(
      PositionTemplate<Strategy>::FirstPositionInNode(node),
      PositionTemplate<Strategy>::LastPositionInNode(node));
}

#if DCHECK_IS_ON()
template <typename Strategy>
bool EphemeralRangeTemplate<Strategy>::IsValid() const {
  return start_position_.IsNull() ||
         dom_tree_version_ == start_position_.GetDocument()->DomTreeVersion();
}
#else
template <typename Strategy>
bool EphemeralRangeTemplate<Strategy>::IsValid() const {
  return true;
}
#endif

Range* CreateRange(const EphemeralRange& range) {
  if (range.IsNull())
    return nullptr;
  return Range::Create(range.GetDocument(), range.StartPosition(),
                       range.EndPosition());
}

template class CORE_TEMPLATE_EXPORT EphemeralRangeTemplate<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT
    EphemeralRangeTemplate<EditingInFlatTreeStrategy>;

}  // namespace blink
