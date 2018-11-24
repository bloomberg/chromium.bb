// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_selection.h"

#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/set_selection_options.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"

namespace blink {

//
// AXSelection::Builder
//

AXSelection::Builder& AXSelection::Builder::SetBase(const AXPosition& base) {
  DCHECK(base.IsValid());
  selection_.base_ = base;
  return *this;
}

AXSelection::Builder& AXSelection::Builder::SetBase(const Position& base) {
  const auto ax_base = AXPosition::FromPosition(base);
  DCHECK(ax_base.IsValid());
  selection_.base_ = ax_base;
  return *this;
}

AXSelection::Builder& AXSelection::Builder::SetExtent(
    const AXPosition& extent) {
  DCHECK(extent.IsValid());
  selection_.extent_ = extent;
  return *this;
}

AXSelection::Builder& AXSelection::Builder::SetExtent(const Position& extent) {
  const auto ax_extent = AXPosition::FromPosition(extent);
  DCHECK(ax_extent.IsValid());
  selection_.extent_ = ax_extent;
  return *this;
}

AXSelection::Builder& AXSelection::Builder::SetSelection(
    const SelectionInDOMTree& selection) {
  if (selection.IsNone())
    return *this;

  selection_.base_ = AXPosition::FromPosition(selection.Base());
  selection_.extent_ = AXPosition::FromPosition(selection.Extent());
  return *this;
}

const AXSelection AXSelection::Builder::Build() {
  if (!selection_.Base().IsValid() || !selection_.Extent().IsValid())
    return {};

  const Document* document = selection_.Base().ContainerObject()->GetDocument();
  DCHECK(document);
  DCHECK(document->IsActive());
  DCHECK(!document->NeedsLayoutTreeUpdate());
  // We don't support selections that span across documents.
  if (selection_.Extent().ContainerObject()->GetDocument() != document)
    return {};

#if DCHECK_IS_ON()
  selection_.dom_tree_version_ = document->DomTreeVersion();
  selection_.style_version_ = document->StyleVersion();
#endif
  return selection_;
}

//
// AXSelection
//

// static
void AXSelection::ClearCurrentSelection(const Document& document) {
  LocalFrame* frame = document.GetFrame();
  if (!frame)
    return;

  FrameSelection& frame_selection = frame->Selection();
  if (!frame_selection.IsAvailable())
    return;

  frame_selection.Clear();
}

// static
AXSelection AXSelection::FromCurrentSelection(
    const Document& document,
    const AXSelectionBehavior selection_behavior) {
  LocalFrame* frame = document.GetFrame();
  if (!frame)
    return {};

  FrameSelection& frame_selection = frame->Selection();
  if (!frame_selection.IsAvailable())
    return {};

  return FromSelection(frame_selection.GetSelectionInDOMTree(),
                       selection_behavior);
}

// static
AXSelection AXSelection::FromSelection(
    const SelectionInDOMTree& selection,
    const AXSelectionBehavior selection_behavior) {
  if (selection.IsNone())
    return {};
  DCHECK(selection.AssertValid());

  const Position dom_base = selection.Base();
  const Position dom_extent = selection.Extent();
  const TextAffinity base_affinity = TextAffinity::kDownstream;
  const TextAffinity extent_affinity = selection.Affinity();

  AXPositionAdjustmentBehavior base_adjustment =
      AXPositionAdjustmentBehavior::kMoveRight;
  AXPositionAdjustmentBehavior extent_adjustment =
      AXPositionAdjustmentBehavior::kMoveRight;
  switch (selection_behavior) {
    case AXSelectionBehavior::kShrinkToValidDOMRange:
      if (selection.IsBaseFirst()) {
        base_adjustment = AXPositionAdjustmentBehavior::kMoveRight;
        extent_adjustment = AXPositionAdjustmentBehavior::kMoveLeft;
      } else {
        base_adjustment = AXPositionAdjustmentBehavior::kMoveLeft;
        extent_adjustment = AXPositionAdjustmentBehavior::kMoveRight;
      }
      break;
    case AXSelectionBehavior::kExtendToValidDOMRange:
      if (selection.IsBaseFirst()) {
        base_adjustment = AXPositionAdjustmentBehavior::kMoveLeft;
        extent_adjustment = AXPositionAdjustmentBehavior::kMoveRight;
      } else {
        base_adjustment = AXPositionAdjustmentBehavior::kMoveRight;
        extent_adjustment = AXPositionAdjustmentBehavior::kMoveLeft;
      }
      break;
  }

  const auto ax_base =
      AXPosition::FromPosition(dom_base, base_affinity, base_adjustment);
  const auto ax_extent =
      AXPosition::FromPosition(dom_extent, extent_affinity, extent_adjustment);

  AXSelection::Builder selection_builder;
  selection_builder.SetBase(ax_base).SetExtent(ax_extent);
  return selection_builder.Build();
}

AXSelection::AXSelection() : base_(), extent_() {
#if DCHECK_IS_ON()
  dom_tree_version_ = 0;
  style_version_ = 0;
#endif
}

bool AXSelection::IsValid() const {
  if (!base_.IsValid() || !extent_.IsValid())
    return false;

  // We don't support selections that span across documents.
  if (base_.ContainerObject()->GetDocument() !=
      extent_.ContainerObject()->GetDocument()) {
    return false;
  }

  DCHECK(!base_.ContainerObject()->GetDocument()->NeedsLayoutTreeUpdate());
#if DCHECK_IS_ON()
  DCHECK_EQ(base_.ContainerObject()->GetDocument()->DomTreeVersion(),
            dom_tree_version_);
  DCHECK_EQ(base_.ContainerObject()->GetDocument()->StyleVersion(),
            style_version_);
#endif  // DCHECK_IS_ON()
  return true;
}

const SelectionInDOMTree AXSelection::AsSelection(
    const AXSelectionBehavior selection_behavior) const {
  if (!IsValid())
    return {};

  AXPositionAdjustmentBehavior base_adjustment =
      AXPositionAdjustmentBehavior::kMoveRight;
  AXPositionAdjustmentBehavior extent_adjustment =
      AXPositionAdjustmentBehavior::kMoveRight;
  switch (selection_behavior) {
    case AXSelectionBehavior::kShrinkToValidDOMRange:
      if (base_ <= extent_) {
        base_adjustment = AXPositionAdjustmentBehavior::kMoveRight;
        extent_adjustment = AXPositionAdjustmentBehavior::kMoveLeft;
      } else {
        base_adjustment = AXPositionAdjustmentBehavior::kMoveLeft;
        extent_adjustment = AXPositionAdjustmentBehavior::kMoveRight;
      }
      break;
    case AXSelectionBehavior::kExtendToValidDOMRange:
      if (base_ <= extent_) {
        base_adjustment = AXPositionAdjustmentBehavior::kMoveLeft;
        extent_adjustment = AXPositionAdjustmentBehavior::kMoveRight;
      } else {
        base_adjustment = AXPositionAdjustmentBehavior::kMoveRight;
        extent_adjustment = AXPositionAdjustmentBehavior::kMoveLeft;
      }
      break;
  }

  const auto dom_base = base_.ToPositionWithAffinity(base_adjustment);
  const auto dom_extent = extent_.ToPositionWithAffinity(extent_adjustment);
  SelectionInDOMTree::Builder selection_builder;
  selection_builder.SetBaseAndExtent(dom_base.GetPosition(),
                                     dom_extent.GetPosition());
  if (extent_.IsTextPosition())
    selection_builder.SetAffinity(extent_.Affinity());
  return selection_builder.Build();
}

void AXSelection::Select(const AXSelectionBehavior selection_behavior) {
  if (!IsValid()) {
    NOTREACHED() << "Trying to select an invalid accessibility selection.";
    return;
  }

  const SelectionInDOMTree selection = AsSelection(selection_behavior);
  DCHECK(selection.AssertValid());
  Document* document = selection.Base().GetDocument();
  if (!document) {
    NOTREACHED();
    return;
  }

  LocalFrame* frame = document->GetFrame();
  if (!frame) {
    NOTREACHED();
    return;
  }

  FrameSelection& frame_selection = frame->Selection();
  if (!frame_selection.IsAvailable())
    return;

  SetSelectionOptions::Builder options_builder;
  options_builder.SetIsDirectional(true)
      .SetShouldCloseTyping(true)
      .SetShouldClearTypingStyle(true);
  frame_selection.ClearDocumentCachedRange();
  frame_selection.SetSelection(selection, options_builder.Build());

  // Cache the newly created document range. This doesn't affect the already
  // applied selection. Note that DOM's |Range| object has a start and an end
  // container that need to be in DOM order. See the DOM specification for more
  // information: https://dom.spec.whatwg.org/#interface-range
  Range* range = Range::Create(*document);
  if (selection.Extent().IsNull()) {
    DCHECK(selection.Base().IsNotNull())
        << "AX selections converted to DOM selections should have at least one "
           "endpoint non-null.\n"
        << *this << '\n'
        << selection;
    range->setStart(selection.Base().ComputeContainerNode(),
                    selection.Base().ComputeOffsetInContainerNode());
    range->setEnd(selection.Base().ComputeContainerNode(),
                  selection.Base().ComputeOffsetInContainerNode());
  } else if (selection.Base() < selection.Extent()) {
    range->setStart(selection.Base().ComputeContainerNode(),
                    selection.Base().ComputeOffsetInContainerNode());
    range->setEnd(selection.Extent().ComputeContainerNode(),
                  selection.Extent().ComputeOffsetInContainerNode());
  } else {
    range->setStart(selection.Extent().ComputeContainerNode(),
                    selection.Extent().ComputeOffsetInContainerNode());
    range->setEnd(selection.Base().ComputeContainerNode(),
                  selection.Base().ComputeOffsetInContainerNode());
  }
  frame_selection.CacheRangeOfDocument(range);
}

String AXSelection::ToString() const {
  if (!IsValid())
    return "Invalid AXSelection";
  return "AXSelection from " + Base().ToString() + " to " + Extent().ToString();
}

bool operator==(const AXSelection& a, const AXSelection& b) {
  DCHECK(a.IsValid() && b.IsValid());
  return a.Base() == b.Base() && a.Extent() == b.Extent();
}

bool operator!=(const AXSelection& a, const AXSelection& b) {
  return !(a == b);
}

std::ostream& operator<<(std::ostream& ostream, const AXSelection& selection) {
  return ostream << selection.ToString().Utf8().data();
}

}  // namespace blink
