// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/SelectionTemplate.h"

#include <ostream>  // NOLINT
#include "platform/wtf/Assertions.h"

namespace blink {

template <typename Strategy>
SelectionTemplate<Strategy>::SelectionTemplate(const SelectionTemplate& other)
    : base_(other.base_),
      extent_(other.extent_),
      affinity_(other.affinity_),
      direction_(other.direction_),
      is_directional_(other.is_directional_)
#if DCHECK_IS_ON()
      ,
      dom_tree_version_(other.dom_tree_version_)
#endif
{
  DCHECK(other.AssertValid());
}

template <typename Strategy>
SelectionTemplate<Strategy>::SelectionTemplate() = default;

template <typename Strategy>
bool SelectionTemplate<Strategy>::operator==(
    const SelectionTemplate& other) const {
  DCHECK(AssertValid());
  DCHECK(other.AssertValid());
  if (IsNone())
    return other.IsNone();
  if (other.IsNone())
    return false;
  DCHECK_EQ(base_.GetDocument(), other.GetDocument()) << *this << ' ' << other;
  return base_ == other.base_ && extent_ == other.extent_ &&
         affinity_ == other.affinity_ &&
         is_directional_ == other.is_directional_;
}

template <typename Strategy>
bool SelectionTemplate<Strategy>::operator!=(
    const SelectionTemplate& other) const {
  return !operator==(other);
}

template <typename Strategy>
DEFINE_TRACE(SelectionTemplate<Strategy>) {
  visitor->Trace(base_);
  visitor->Trace(extent_);
}

template <typename Strategy>
const PositionTemplate<Strategy>& SelectionTemplate<Strategy>::Base() const {
  DCHECK(AssertValid());
  DCHECK(!base_.IsOrphan()) << base_;
  return base_;
}

template <typename Strategy>
Document* SelectionTemplate<Strategy>::GetDocument() const {
  DCHECK(AssertValid());
  return base_.GetDocument();
}

template <typename Strategy>
const PositionTemplate<Strategy>& SelectionTemplate<Strategy>::Extent() const {
  DCHECK(AssertValid());
  DCHECK(!extent_.IsOrphan()) << extent_;
  return extent_;
}

template <typename Strategy>
bool SelectionTemplate<Strategy>::IsCaret() const {
  return base_.IsNotNull() && base_ == extent_;
}

template <typename Strategy>
bool SelectionTemplate<Strategy>::IsRange() const {
  return base_ != extent_;
}

template <typename Strategy>
bool SelectionTemplate<Strategy>::AssertValidFor(
    const Document& document) const {
  if (!AssertValid())
    return false;
  if (base_.IsNull())
    return true;
  DCHECK_EQ(base_.GetDocument(), document) << *this;
  return true;
}

#if DCHECK_IS_ON()
template <typename Strategy>
bool SelectionTemplate<Strategy>::AssertValid() const {
  if (base_.IsNull())
    return true;
  DCHECK_EQ(base_.GetDocument()->DomTreeVersion(), dom_tree_version_) << *this;
  DCHECK(!base_.IsOrphan()) << *this;
  DCHECK(!extent_.IsOrphan()) << *this;
  DCHECK_EQ(base_.GetDocument(), extent_.GetDocument());
  return true;
}
#else
template <typename Strategy>
bool SelectionTemplate<Strategy>::AssertValid() const {
  return true;
}
#endif

#ifndef NDEBUG
template <typename Strategy>
void SelectionTemplate<Strategy>::ShowTreeForThis() const {
  if (base_.IsNull()) {
    LOG(INFO) << "\nbase is null";
    return;
  }

  LOG(INFO) << "\n"
            << base_.AnchorNode()
                   ->ToMarkedTreeString(base_.AnchorNode(), "B",
                                        extent_.AnchorNode(), "E")
                   .Utf8()
                   .data()
            << "base: " << base_.ToAnchorTypeAndOffsetString().Utf8().data()
            << "\n"
            << "extent: "
            << extent_.ToAnchorTypeAndOffsetString().Utf8().data();
}
#endif

template <typename Strategy>
const PositionTemplate<Strategy>&
SelectionTemplate<Strategy>::ComputeEndPosition() const {
  return IsBaseFirst() ? extent_ : base_;
}

template <typename Strategy>
const PositionTemplate<Strategy>&
SelectionTemplate<Strategy>::ComputeStartPosition() const {
  return IsBaseFirst() ? base_ : extent_;
}

template <typename Strategy>
bool SelectionTemplate<Strategy>::IsBaseFirst() const {
  DCHECK(AssertValid());
  if (base_ == extent_) {
    DCHECK_EQ(direction_, Direction::kForward);
    return true;
  }
  if (direction_ == Direction::kForward) {
    DCHECK_LE(base_, extent_);
    return true;
  }
  if (direction_ == Direction::kBackward) {
    DCHECK_GT(base_, extent_);
    return false;
  }
  // Note: Since same position can be represented in different anchor type,
  // e.g. Position(div, 0) and BeforeNode(first-child), we use |<=| to check
  // forward selection.
  DCHECK_EQ(direction_, Direction::kNotComputed);
  direction_ = base_ <= extent_ ? Direction::kForward : Direction::kBackward;
  return direction_ == Direction::kForward;
}

template <typename Strategy>
void SelectionTemplate<Strategy>::ResetDirectionCache() const {
  direction_ = base_ == extent_ ? Direction::kForward : Direction::kNotComputed;
}

template <typename Strategy>
SelectionType SelectionTemplate<Strategy>::Type() const {
  if (base_.IsNull())
    return kNoSelection;
  if (base_ == extent_)
    return kCaretSelection;
  return kRangeSelection;
}

template <typename Strategy>
void SelectionTemplate<Strategy>::PrintTo(std::ostream* ostream,
                                          const char* type) const {
  if (IsNone()) {
    *ostream << "()";
    return;
  }
  *ostream << type << '(';
#if DCHECK_IS_ON()
  if (dom_tree_version_ != base_.GetDocument()->DomTreeVersion()) {
    *ostream << "Dirty: " << dom_tree_version_;
    *ostream << " != " << base_.GetDocument()->DomTreeVersion() << ' ';
  }
#endif
  *ostream << "base: " << base_ << ", extent: " << extent_ << ')';
}

std::ostream& operator<<(std::ostream& ostream,
                         const SelectionInDOMTree& selection) {
  selection.PrintTo(&ostream, "Selection");
  return ostream;
}

std::ostream& operator<<(std::ostream& ostream,
                         const SelectionInFlatTree& selection) {
  selection.PrintTo(&ostream, "SelectionInFlatTree");
  return ostream;
}

// --

template <typename Strategy>
SelectionTemplate<Strategy>::Builder::Builder(
    const SelectionTemplate<Strategy>& selection)
    : selection_(selection) {}

template <typename Strategy>
SelectionTemplate<Strategy>::Builder::Builder() = default;

template <typename Strategy>
SelectionTemplate<Strategy> SelectionTemplate<Strategy>::Builder::Build()
    const {
  DCHECK(selection_.AssertValid());
  selection_.ResetDirectionCache();
  return selection_;
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::Collapse(
    const PositionTemplate<Strategy>& position) {
  DCHECK(position.IsConnected()) << position;
  selection_.base_ = position;
  selection_.extent_ = position;
#if DCHECK_IS_ON()
  selection_.dom_tree_version_ = position.GetDocument()->DomTreeVersion();
#endif
  return *this;
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::Collapse(
    const PositionWithAffinityTemplate<Strategy>& position_with_affinity) {
  Collapse(position_with_affinity.GetPosition());
  SetAffinity(position_with_affinity.Affinity());
  return *this;
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::Extend(
    const PositionTemplate<Strategy>& position) {
  DCHECK(position.IsConnected()) << position;
  DCHECK_EQ(selection_.GetDocument(), position.GetDocument());
  DCHECK(selection_.Base().IsConnected()) << selection_.Base();
  DCHECK(selection_.AssertValid());
  selection_.extent_ = position;
  return *this;
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::SelectAllChildren(const Node& node) {
  DCHECK(node.CanContainRangeEndPoint()) << node;
  return SetBaseAndExtent(
      EphemeralRangeTemplate<Strategy>::RangeOfContents(node));
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::SetAffinity(TextAffinity affinity) {
  selection_.affinity_ = affinity;
  return *this;
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::SetBaseAndExtent(
    const EphemeralRangeTemplate<Strategy>& range) {
  if (range.IsNull()) {
    selection_.base_ = PositionTemplate<Strategy>();
    selection_.extent_ = PositionTemplate<Strategy>();
#if DCHECK_IS_ON()
    selection_.dom_tree_version_ = 0;
#endif
    return *this;
  }
  return Collapse(range.StartPosition()).Extend(range.EndPosition());
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::SetBaseAndExtent(
    const PositionTemplate<Strategy>& base,
    const PositionTemplate<Strategy>& extent) {
  if (base.IsNull()) {
    DCHECK(extent.IsNull()) << extent;
    return SetBaseAndExtent(EphemeralRangeTemplate<Strategy>());
  }
  DCHECK(extent.IsNotNull());
  return Collapse(base).Extend(extent);
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::SetBaseAndExtentDeprecated(
    const PositionTemplate<Strategy>& base,
    const PositionTemplate<Strategy>& extent) {
  if (base.IsNotNull() && extent.IsNotNull()) {
    return SetBaseAndExtent(base, extent);
  }
  if (base.IsNotNull())
    return Collapse(base);
  if (extent.IsNotNull())
    return Collapse(extent);
  return SetBaseAndExtent(EphemeralRangeTemplate<Strategy>());
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::SetIsDirectional(bool is_directional) {
  selection_.is_directional_ = is_directional;
  return *this;
}

template class CORE_TEMPLATE_EXPORT SelectionTemplate<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT
    SelectionTemplate<EditingInFlatTreeStrategy>;

}  // namespace blink
