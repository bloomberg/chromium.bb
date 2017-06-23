// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/spellcheck/ColdModeSpellCheckRequester.h"

#include "core/dom/Element.h"
#include "core/dom/IdleDeadline.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/iterators/CharacterIterator.h"
#include "core/editing/spellcheck/SpellCheckRequester.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/frame/LocalFrame.h"
#include "platform/instrumentation/tracing/TraceEvent.h"

namespace blink {

namespace {

const int kColdModeChunkSize = 16384;  // in UTF16 code units
const int kInvalidLength = -1;
const int kInvalidChunkIndex = -1;

bool ShouldCheckNode(const Node& node) {
  if (!node.IsElementNode())
    return false;
  // TODO(editing-dev): Make |Position| constructors take const parameters.
  const Position& position = Position::FirstPositionInNode(node);
  if (!IsEditablePosition(position))
    return false;
  return SpellChecker::IsSpellCheckingEnabledAt(position);
}

}  // namespace

// static
ColdModeSpellCheckRequester* ColdModeSpellCheckRequester::Create(
    LocalFrame& frame) {
  return new ColdModeSpellCheckRequester(frame);
}

DEFINE_TRACE(ColdModeSpellCheckRequester) {
  visitor->Trace(frame_);
  visitor->Trace(next_node_);
  visitor->Trace(current_root_editable_);
  visitor->Trace(current_chunk_start_);
}

ColdModeSpellCheckRequester::ColdModeSpellCheckRequester(LocalFrame& frame)
    : frame_(frame),
      last_checked_dom_tree_version_(0),
      needs_more_invocation_for_testing_(false) {}

bool ColdModeSpellCheckRequester::FullDocumentChecked() const {
  if (needs_more_invocation_for_testing_) {
    needs_more_invocation_for_testing_ = false;
    return false;
  }
  return !next_node_;
}

SpellCheckRequester& ColdModeSpellCheckRequester::GetSpellCheckRequester()
    const {
  return GetFrame().GetSpellChecker().GetSpellCheckRequester();
}

void ColdModeSpellCheckRequester::Invoke(IdleDeadline* deadline) {
  TRACE_EVENT0("blink", "ColdModeSpellCheckRequester::invoke");

  Node* body = GetFrame().GetDocument()->body();
  if (!body) {
    ResetCheckingProgress();
    last_checked_dom_tree_version_ = GetFrame().GetDocument()->DomTreeVersion();
    return;
  }

  // TODO(xiaochengh): Figure out if this has any performance impact.
  GetFrame().GetDocument()->UpdateStyleAndLayout();

  if (last_checked_dom_tree_version_ !=
      GetFrame().GetDocument()->DomTreeVersion())
    ResetCheckingProgress();

  while (next_node_ && deadline->timeRemaining() > 0)
    Step();
  last_checked_dom_tree_version_ = GetFrame().GetDocument()->DomTreeVersion();
}

void ColdModeSpellCheckRequester::ResetCheckingProgress() {
  next_node_ = GetFrame().GetDocument()->body();
  current_root_editable_ = nullptr;
  current_full_length_ = kInvalidLength;
  current_chunk_index_ = kInvalidChunkIndex;
  current_chunk_start_ = Position();
}

void ColdModeSpellCheckRequester::Step() {
  if (!next_node_)
    return;

  if (!current_root_editable_) {
    SearchForNextRootEditable();
    return;
  }

  if (current_full_length_ == kInvalidLength) {
    InitializeForCurrentRootEditable();
    return;
  }

  DCHECK(current_chunk_index_ != kInvalidChunkIndex);
  RequestCheckingForNextChunk();
}

void ColdModeSpellCheckRequester::SearchForNextRootEditable() {
  // TODO(xiaochengh): Figure out if such small steps, which result in frequent
  // calls of |timeRemaining()|, have any performance impact. We might not want
  // to check remaining time so frequently in a page with millions of nodes.

  if (ShouldCheckNode(*next_node_)) {
    current_root_editable_ = ToElement(next_node_);
    return;
  }

  next_node_ =
      FlatTreeTraversal::Next(*next_node_, GetFrame().GetDocument()->body());
}

void ColdModeSpellCheckRequester::InitializeForCurrentRootEditable() {
  const EphemeralRange& full_range =
      EphemeralRange::RangeOfContents(*current_root_editable_);
  current_full_length_ = TextIterator::RangeLength(full_range.StartPosition(),
                                                   full_range.EndPosition());

  current_chunk_index_ = 0;
  current_chunk_start_ = full_range.StartPosition();
}

void ColdModeSpellCheckRequester::RequestCheckingForNextChunk() {
  // Check the full content if it is short.
  if (current_full_length_ <= kColdModeChunkSize) {
    GetSpellCheckRequester().RequestCheckingFor(
        EphemeralRange::RangeOfContents(*current_root_editable_));
    FinishCheckingCurrentRootEditable();
    return;
  }

  const Position& chunk_end =
      CalculateCharacterSubrange(
          EphemeralRange(current_chunk_start_,
                         Position::LastPositionInNode(current_root_editable_)),
          0, kColdModeChunkSize)
          .EndPosition();
  if (chunk_end <= current_chunk_start_) {
    FinishCheckingCurrentRootEditable();
    return;
  }
  const EphemeralRange chunk_range(current_chunk_start_, chunk_end);
  const EphemeralRange& check_range = ExpandEndToSentenceBoundary(chunk_range);
  GetSpellCheckRequester().RequestCheckingFor(check_range,
                                              current_chunk_index_);

  current_chunk_start_ = check_range.EndPosition();
  ++current_chunk_index_;

  if (current_chunk_index_ * kColdModeChunkSize >= current_full_length_)
    FinishCheckingCurrentRootEditable();
}

void ColdModeSpellCheckRequester::FinishCheckingCurrentRootEditable() {
  DCHECK_EQ(next_node_, current_root_editable_);
  next_node_ = FlatTreeTraversal::NextSkippingChildren(
      *next_node_, GetFrame().GetDocument()->body());

  current_root_editable_ = nullptr;
  current_full_length_ = kInvalidLength;
  current_chunk_index_ = kInvalidChunkIndex;
  current_chunk_start_ = Position();
}

}  // namespace blink
