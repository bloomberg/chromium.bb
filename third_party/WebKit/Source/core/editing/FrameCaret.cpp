/*
 * Copyright (C) 2004, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#include "core/editing/FrameCaret.h"

#include "base/location.h"
#include "core/editing/CaretDisplayItemClient.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/PositionWithAffinity.h"
#include "core/editing/SelectionEditor.h"
#include "core/editing/VisiblePosition.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/html/forms/TextControlElement.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutEmbeddedContent.h"
#include "core/layout/LayoutTheme.h"
#include "core/page/Page.h"
#include "public/platform/TaskType.h"

namespace blink {

FrameCaret::FrameCaret(LocalFrame& frame,
                       const SelectionEditor& selection_editor)
    : selection_editor_(&selection_editor),
      frame_(frame),
      display_item_client_(new CaretDisplayItemClient()),
      caret_visibility_(CaretVisibility::kHidden),
      caret_blink_timer_(new TaskRunnerTimer<FrameCaret>(
          frame.GetTaskRunner(TaskType::kUnspecedTimer),
          this,
          &FrameCaret::CaretBlinkTimerFired)),
      should_paint_caret_(true),
      is_caret_blinking_suspended_(false),
      should_show_block_cursor_(false) {}

FrameCaret::~FrameCaret() = default;

void FrameCaret::Trace(blink::Visitor* visitor) {
  visitor->Trace(selection_editor_);
  visitor->Trace(frame_);
}

const DisplayItemClient& FrameCaret::GetDisplayItemClient() const {
  return *display_item_client_;
}

const PositionWithAffinity FrameCaret::CaretPosition() const {
  const VisibleSelection& selection =
      selection_editor_->ComputeVisibleSelectionInDOMTree();
  if (!selection.IsCaret())
    return PositionWithAffinity();
  return PositionWithAffinity(selection.Start(), selection.Affinity());
}

bool FrameCaret::IsActive() const {
  return CaretPosition().IsNotNull();
}

void FrameCaret::UpdateAppearance() {
  DCHECK_GE(frame_->GetDocument()->Lifecycle().GetState(),
            DocumentLifecycle::kLayoutClean);
  // Paint a block cursor instead of a caret in overtype mode unless the caret
  // is at the end of a line (in this case the FrameSelection will paint a
  // blinking caret as usual).
  const bool paint_block_cursor =
      should_show_block_cursor_ && IsActive() &&
      !IsLogicalEndOfLine(CreateVisiblePosition(CaretPosition()));

  bool should_blink = !paint_block_cursor && ShouldBlinkCaret();
  if (!should_blink) {
    StopCaretBlinkTimer();
    return;
  }
  // Start blinking with a black caret. Be sure not to restart if we're
  // already blinking in the right location.
  StartBlinkCaret();
}

void FrameCaret::StopCaretBlinkTimer() {
  if (caret_blink_timer_->IsActive() || should_paint_caret_)
    ScheduleVisualUpdateForPaintInvalidationIfNeeded();
  should_paint_caret_ = false;
  caret_blink_timer_->Stop();
}

void FrameCaret::StartBlinkCaret() {
  // Start blinking with a black caret. Be sure not to restart if we're
  // already blinking in the right location.
  if (caret_blink_timer_->IsActive())
    return;

  TimeDelta blink_interval = LayoutTheme::GetTheme().CaretBlinkInterval();
  if (!blink_interval.is_zero())
    caret_blink_timer_->StartRepeating(blink_interval, FROM_HERE);

  should_paint_caret_ = true;
  ScheduleVisualUpdateForPaintInvalidationIfNeeded();
}

void FrameCaret::SetCaretVisibility(CaretVisibility visibility) {
  if (caret_visibility_ == visibility)
    return;

  caret_visibility_ = visibility;

  if (visibility == CaretVisibility::kHidden)
    StopCaretBlinkTimer();
  ScheduleVisualUpdateForPaintInvalidationIfNeeded();
}

void FrameCaret::ClearPreviousVisualRect(const LayoutBlock& block) {
  display_item_client_->ClearPreviousVisualRect(block);
}

void FrameCaret::LayoutBlockWillBeDestroyed(const LayoutBlock& block) {
  display_item_client_->LayoutBlockWillBeDestroyed(block);
}

void FrameCaret::UpdateStyleAndLayoutIfNeeded() {
  DCHECK_GE(frame_->GetDocument()->Lifecycle().GetState(),
            DocumentLifecycle::kLayoutClean);
  UpdateAppearance();
  bool should_paint_caret =
      should_paint_caret_ && IsActive() &&
      caret_visibility_ == CaretVisibility::kVisible &&
      IsEditablePosition(
          selection_editor_->ComputeVisibleSelectionInDOMTree().Start());

  display_item_client_->UpdateStyleAndLayoutIfNeeded(
      should_paint_caret ? CaretPosition() : PositionWithAffinity());
}

void FrameCaret::InvalidatePaint(const LayoutBlock& block,
                                 const PaintInvalidatorContext& context) {
  display_item_client_->InvalidatePaint(block, context);
}

bool FrameCaret::CaretPositionIsValidForDocument(
    const Document& document) const {
  if (!IsActive())
    return true;

  return CaretPosition().GetDocument() == document &&
         !CaretPosition().IsOrphan();
}

static IntRect AbsoluteBoundsForLocalRect(Node* node, const LayoutRect& rect) {
  LayoutBlock* caret_painter = CaretDisplayItemClient::CaretLayoutBlock(node);
  if (!caret_painter)
    return IntRect();

  LayoutRect local_rect(rect);
  return caret_painter->LocalToAbsoluteQuad(FloatRect(local_rect))
      .EnclosingBoundingBox();
}

IntRect FrameCaret::AbsoluteCaretBounds() const {
  DCHECK_NE(frame_->GetDocument()->Lifecycle().GetState(),
            DocumentLifecycle::kInPrePaint);
  DCHECK(!frame_->GetDocument()->NeedsLayoutTreeUpdate());
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      frame_->GetDocument()->Lifecycle());

  Node* const caret_node = CaretPosition().AnchorNode();
  if (!IsActive())
    return AbsoluteBoundsForLocalRect(caret_node, LayoutRect());
  return AbsoluteBoundsForLocalRect(
      caret_node,
      CaretDisplayItemClient::ComputeCaretRect(
          CreateVisiblePosition(CaretPosition()).ToPositionWithAffinity()));
}

void FrameCaret::SetShouldShowBlockCursor(bool should_show_block_cursor) {
  should_show_block_cursor_ = should_show_block_cursor;
  ScheduleVisualUpdateForPaintInvalidationIfNeeded();
}

bool FrameCaret::ShouldPaintCaret(const LayoutBlock& block) const {
  return display_item_client_->ShouldPaintCaret(block);
}

void FrameCaret::PaintCaret(GraphicsContext& context,
                            const LayoutPoint& paint_offset) const {
  display_item_client_->PaintCaret(context, paint_offset, DisplayItem::kCaret);
}

bool FrameCaret::ShouldBlinkCaret() const {
  if (caret_visibility_ != CaretVisibility::kVisible || !IsActive())
    return false;

  Element* root = RootEditableElementOf(CaretPosition().GetPosition());
  if (!root)
    return false;

  Element* focused_element = root->GetDocument().FocusedElement();
  if (!focused_element)
    return false;

  return frame_->Selection().SelectionHasFocus();
}

void FrameCaret::CaretBlinkTimerFired(TimerBase*) {
  DCHECK_EQ(caret_visibility_, CaretVisibility::kVisible);
  if (IsCaretBlinkingSuspended() && should_paint_caret_)
    return;
  should_paint_caret_ = !should_paint_caret_;
  ScheduleVisualUpdateForPaintInvalidationIfNeeded();
}

void FrameCaret::ScheduleVisualUpdateForPaintInvalidationIfNeeded() {
  if (LocalFrameView* frame_view = frame_->View())
    frame_view->ScheduleVisualUpdateForPaintInvalidationIfNeeded();
}

void FrameCaret::RecreateCaretBlinkTimerForTesting(
    scoped_refptr<WebTaskRunner> task_runner) {
  caret_blink_timer_.reset(new TaskRunnerTimer<FrameCaret>(
      std::move(task_runner), this, &FrameCaret::CaretBlinkTimerFired));
}

}  // namespace blink
