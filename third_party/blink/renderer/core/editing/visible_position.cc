/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
 * reserved.
 * Portions Copyright (c) 2011 Motorola Mobility, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/visible_position.h"

#include <ostream>  // NOLINT
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/local_caret_rect.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_caret_navigator.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/wtf/text/cstring.h"

namespace blink {

template <typename Strategy>
VisiblePositionTemplate<Strategy>::VisiblePositionTemplate()
#if DCHECK_IS_ON()
    : dom_tree_version_(0),
      style_version_(0)
#endif
{
}

template <typename Strategy>
VisiblePositionTemplate<Strategy>::VisiblePositionTemplate(
    const PositionWithAffinityTemplate<Strategy>& position_with_affinity)
    : position_with_affinity_(position_with_affinity)
#if DCHECK_IS_ON()
      ,
      dom_tree_version_(position_with_affinity.GetDocument()->DomTreeVersion()),
      style_version_(position_with_affinity.GetDocument()->StyleVersion())
#endif
{
}

template <typename Strategy>
void VisiblePositionTemplate<Strategy>::Trace(blink::Visitor* visitor) {
  visitor->Trace(position_with_affinity_);
}

template <typename Strategy>
VisiblePositionTemplate<Strategy> VisiblePositionTemplate<Strategy>::Create(
    const PositionWithAffinityTemplate<Strategy>& position_with_affinity) {
  if (position_with_affinity.IsNull())
    return VisiblePositionTemplate<Strategy>();
  DCHECK(position_with_affinity.IsConnected()) << position_with_affinity;

  Document& document = *position_with_affinity.GetDocument();
  DCHECK(!document.NeedsLayoutTreeUpdate());
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      document.Lifecycle());

  const PositionTemplate<Strategy> deep_position =
      CanonicalPositionOf(position_with_affinity.GetPosition());
  if (deep_position.IsNull())
    return VisiblePositionTemplate<Strategy>();
  const PositionWithAffinityTemplate<Strategy> downstream_position(
      deep_position);
  if (position_with_affinity.Affinity() == TextAffinity::kDownstream)
    return VisiblePositionTemplate<Strategy>(downstream_position);

  if (RuntimeEnabledFeatures::BidiCaretAffinityEnabled()) {
    // When not at a line wrap or bidi boundary, make sure to end up with
    // |TextAffinity::Downstream| affinity.
    const PositionWithAffinityTemplate<Strategy> upstream_position(
        deep_position, TextAffinity::kUpstream);

    if (!InSameLine(downstream_position, upstream_position))
      return VisiblePositionTemplate<Strategy>(upstream_position);

    if (!NGOffsetMapping::AcceptsPosition(ToPositionInDOMTree(deep_position))) {
      // editing/selection/mixed-editability-10.html reaches here.
      // We can't check bidi in such case. Use downstream as the default.
      // TODO(xiaochengh): Investigate why we reach here and how to work around.
      return VisiblePositionTemplate<Strategy>(downstream_position);
    }

    // Check if the position is at bidi boundary.
    const LayoutObject* layout_object =
        deep_position.AnchorNode()->GetLayoutObject();
    DCHECK(layout_object) << position_with_affinity;
    if (!layout_object->IsInline())
      return VisiblePositionTemplate<Strategy>(downstream_position);
    LayoutBlockFlow* const context =
        NGOffsetMapping::GetInlineFormattingContextOf(*layout_object);
    DCHECK(context);

    // TODO(xiaochengh): The double pointer pattern below is confusing and
    // cumbersome, but necessary for now. Make it easier.
    std::unique_ptr<NGOffsetMapping> mapping_storage;
    const NGOffsetMapping* mapping =
        NGInlineNode::GetOffsetMapping(context, &mapping_storage);
    DCHECK(mapping);

    const base::Optional<unsigned> offset =
        mapping->GetTextContentOffset(ToPositionInDOMTree(deep_position));
    DCHECK(offset.has_value());

    DCHECK(mapping->GetCaretNavigator());
    const NGCaretNavigator& caret_navigator = *mapping->GetCaretNavigator();
    if (caret_navigator.OffsetIsBidiBoundary(offset.value()))
      return VisiblePositionTemplate<Strategy>(upstream_position);
    return VisiblePositionTemplate<Strategy>(downstream_position);
  }

  // When not at a line wrap, make sure to end up with
  // |TextAffinity::Downstream| affinity.
  const PositionWithAffinityTemplate<Strategy> upstream_position(
      deep_position, TextAffinity::kUpstream);
  if (InSameLine(downstream_position, upstream_position))
    return VisiblePositionTemplate<Strategy>(downstream_position);
  return VisiblePositionTemplate<Strategy>(upstream_position);
}

template <typename Strategy>
VisiblePositionTemplate<Strategy> VisiblePositionTemplate<Strategy>::AfterNode(
    const Node& node) {
  return Create(PositionWithAffinityTemplate<Strategy>(
      PositionTemplate<Strategy>::AfterNode(node)));
}

template <typename Strategy>
VisiblePositionTemplate<Strategy> VisiblePositionTemplate<Strategy>::BeforeNode(
    const Node& node) {
  return Create(PositionWithAffinityTemplate<Strategy>(
      PositionTemplate<Strategy>::BeforeNode(node)));
}

template <typename Strategy>
VisiblePositionTemplate<Strategy>
VisiblePositionTemplate<Strategy>::FirstPositionInNode(const Node& node) {
  return Create(PositionWithAffinityTemplate<Strategy>(
      PositionTemplate<Strategy>::FirstPositionInNode(node)));
}

template <typename Strategy>
VisiblePositionTemplate<Strategy>
VisiblePositionTemplate<Strategy>::InParentAfterNode(const Node& node) {
  return Create(PositionWithAffinityTemplate<Strategy>(
      PositionTemplate<Strategy>::InParentAfterNode(node)));
}

template <typename Strategy>
VisiblePositionTemplate<Strategy>
VisiblePositionTemplate<Strategy>::InParentBeforeNode(const Node& node) {
  return Create(PositionWithAffinityTemplate<Strategy>(
      PositionTemplate<Strategy>::InParentBeforeNode(node)));
}

template <typename Strategy>
VisiblePositionTemplate<Strategy>
VisiblePositionTemplate<Strategy>::LastPositionInNode(const Node& node) {
  return Create(PositionWithAffinityTemplate<Strategy>(
      PositionTemplate<Strategy>::LastPositionInNode(node)));
}

VisiblePosition CreateVisiblePosition(const Position& position,
                                      TextAffinity affinity) {
  return VisiblePosition::Create(PositionWithAffinity(position, affinity));
}

VisiblePosition CreateVisiblePosition(
    const PositionWithAffinity& position_with_affinity) {
  return VisiblePosition::Create(position_with_affinity);
}

VisiblePositionInFlatTree CreateVisiblePosition(
    const PositionInFlatTree& position,
    TextAffinity affinity) {
  return VisiblePositionInFlatTree::Create(
      PositionInFlatTreeWithAffinity(position, affinity));
}

VisiblePositionInFlatTree CreateVisiblePosition(
    const PositionInFlatTreeWithAffinity& position_with_affinity) {
  return VisiblePositionInFlatTree::Create(position_with_affinity);
}

#ifndef NDEBUG

template <typename Strategy>
void VisiblePositionTemplate<Strategy>::ShowTreeForThis() const {
  DeepEquivalent().ShowTreeForThis();
}

#endif

template <typename Strategy>
bool VisiblePositionTemplate<Strategy>::IsValid() const {
#if DCHECK_IS_ON()
  if (IsNull())
    return true;
  Document& document = *position_with_affinity_.GetDocument();
  return dom_tree_version_ == document.DomTreeVersion() &&
         style_version_ == document.StyleVersion() &&
         !document.NeedsLayoutTreeUpdate();
#else
  return true;
#endif
}

template <typename Strategy>
bool VisiblePositionTemplate<Strategy>::IsValidFor(
    const Document& document) const {
  return position_with_affinity_.IsValidFor(document);
}

template class CORE_TEMPLATE_EXPORT VisiblePositionTemplate<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT
    VisiblePositionTemplate<EditingInFlatTreeStrategy>;

std::ostream& operator<<(std::ostream& ostream,
                         const VisiblePosition& position) {
  return ostream << position.DeepEquivalent() << '/' << position.Affinity();
}

std::ostream& operator<<(std::ostream& ostream,
                         const VisiblePositionInFlatTree& position) {
  return ostream << position.DeepEquivalent() << '/' << position.Affinity();
}

}  // namespace blink

#ifndef NDEBUG

void showTree(const blink::VisiblePosition* vpos) {
  if (vpos) {
    vpos->ShowTreeForThis();
    return;
  }
  DVLOG(0) << "Cannot showTree for (nil) VisiblePosition.";
}

void showTree(const blink::VisiblePosition& vpos) {
  vpos.ShowTreeForThis();
}

#endif
