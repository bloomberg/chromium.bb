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

#include "third_party/blink/renderer/core/editing/frame_caret.h"

#include "base/location.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/editing/caret_display_item_client.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/local_caret_rect.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/selection_editor.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

FrameCaret::FrameCaret(LocalFrame& frame,
                       const SelectionEditor& selection_editor)
    : selection_editor_(&selection_editor),
      frame_(frame),
      display_item_client_(MakeGarbageCollected<CaretDisplayItemClient>()),
      caret_blink_timer_(frame.GetTaskRunner(TaskType::kInternalDefault),
                         this,
                         &FrameCaret::CaretBlinkTimerFired) {}

FrameCaret::~FrameCaret() = default;

void FrameCaret::Trace(Visitor* visitor) const {
  visitor->Trace(selection_editor_);
  visitor->Trace(frame_);
  visitor->Trace(display_item_client_);
  visitor->Trace(caret_blink_timer_);
}

const PositionWithAffinity FrameCaret::CaretPosition() const {
  const VisibleSelection& selection =
      selection_editor_->ComputeVisibleSelectionInDOMTree();
  if (!selection.IsCaret())
    return PositionWithAffinity();
  DCHECK(selection.Start().IsValidFor(*frame_->GetDocument()));
  return PositionWithAffinity(selection.Start(), selection.Affinity());
}

bool FrameCaret::IsActive() const {
  return CaretPosition().IsNotNull();
}

void FrameCaret::UpdateAppearance() {
  DCHECK_GE(frame_->GetDocument()->Lifecycle().GetState(),
            DocumentLifecycle::kLayoutClean);

  bool new_should_show_caret = ShouldShowCaret();
  if (new_should_show_caret != should_show_caret_) {
    should_show_caret_ = new_should_show_caret;
    ScheduleVisualUpdateForPaintInvalidationIfNeeded();
  }

  if (!should_show_caret_) {
    StopCaretBlinkTimer();
    return;
  }
  // Start blinking with a black caret. Be sure not to restart if we're
  // already blinking in the right location.
  StartBlinkCaret();
}

void FrameCaret::StopCaretBlinkTimer() {
  if (caret_blink_timer_.IsActive() ||
      display_item_client_->IsVisibleIfActive())
    ScheduleVisualUpdateForPaintInvalidationIfNeeded();
  display_item_client_->SetVisibleIfActive(false);
  caret_blink_timer_.Stop();
}

void FrameCaret::StartBlinkCaret() {
  // Start blinking with a black caret. Be sure not to restart if we're
  // already blinking in the right location.
  if (caret_blink_timer_.IsActive())
    return;

  base::TimeDelta blink_interval = LayoutTheme::GetTheme().CaretBlinkInterval();
  if (!blink_interval.is_zero())
    caret_blink_timer_.StartRepeating(blink_interval, FROM_HERE);

  display_item_client_->SetVisibleIfActive(true);
  ScheduleVisualUpdateForPaintInvalidationIfNeeded();
}

void FrameCaret::SetCaretEnabled(bool enabled) {
  if (is_caret_enabled_ == enabled)
    return;

  is_caret_enabled_ = enabled;

  if (!is_caret_enabled_)
    StopCaretBlinkTimer();
  ScheduleVisualUpdateForPaintInvalidationIfNeeded();
}

void FrameCaret::LayoutBlockWillBeDestroyed(const LayoutBlock& block) {
  display_item_client_->LayoutBlockWillBeDestroyed(block);
}

void FrameCaret::UpdateStyleAndLayoutIfNeeded() {
  DCHECK_GE(frame_->GetDocument()->Lifecycle().GetState(),
            DocumentLifecycle::kLayoutClean);
  UpdateAppearance();
  display_item_client_->UpdateStyleAndLayoutIfNeeded(
      should_show_caret_ ? CaretPosition() : PositionWithAffinity());
}

void FrameCaret::InvalidatePaint(const LayoutBlock& block,
                                 const PaintInvalidatorContext& context) {
  display_item_client_->InvalidatePaint(block, context);
}

gfx::Rect FrameCaret::AbsoluteCaretBounds() const {
  DCHECK_NE(frame_->GetDocument()->Lifecycle().GetState(),
            DocumentLifecycle::kInPrePaint);
  DCHECK(!frame_->GetDocument()->NeedsLayoutTreeUpdate());
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      frame_->GetDocument()->Lifecycle());

  return AbsoluteCaretBoundsOf(CaretPosition());
}

bool FrameCaret::ShouldPaintCaret(const LayoutBlock& block) const {
  return display_item_client_->ShouldPaintCaret(block);
}

bool FrameCaret::ShouldPaintCaret(
    const NGPhysicalBoxFragment& box_fragment) const {
  return display_item_client_->ShouldPaintCaret(box_fragment);
}

void FrameCaret::PaintCaret(GraphicsContext& context,
                            const PhysicalOffset& paint_offset) const {
  display_item_client_->PaintCaret(context, paint_offset, DisplayItem::kCaret);
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
      frame_->Selection().IsHandleVisible() && !frame_->Selection().IsHidden())
    display_item_client_->RecordSelection(context, paint_offset);
}

bool FrameCaret::ShouldShowCaret() const {
  // Don't show the caret if it isn't visible or positioned.
  if (!is_caret_enabled_ || !IsActive())
    return false;

  Element* root = RootEditableElementOf(CaretPosition().GetPosition());
  if (root) {
    // Caret is contained in editable content. If there is no focused element,
    // don't show the caret.
    Element* focused_element = root->GetDocument().FocusedElement();
    if (!focused_element)
      return false;
  } else {
    // Caret is not contained in editable content--see if caret browsing is
    // enabled. If it isn't, don't show the caret.
    if (!frame_->IsCaretBrowsingEnabled())
      return false;
  }

  if (!IsEditablePosition(
          selection_editor_->ComputeVisibleSelectionInDOMTree().Start()) &&
      !frame_->IsCaretBrowsingEnabled())
    return false;

  // Only show the caret if the selection has focus.
  return frame_->Selection().SelectionHasFocus();
}

void FrameCaret::CaretBlinkTimerFired(TimerBase*) {
  DCHECK(is_caret_enabled_);
  if (IsCaretBlinkingSuspended() && display_item_client_->IsVisibleIfActive())
    return;
  display_item_client_->SetVisibleIfActive(
      !display_item_client_->IsVisibleIfActive());
  ScheduleVisualUpdateForPaintInvalidationIfNeeded();
}

void FrameCaret::ScheduleVisualUpdateForPaintInvalidationIfNeeded() {
  if (LocalFrameView* frame_view = frame_->View())
    frame_view->ScheduleVisualUpdateForPaintInvalidationIfNeeded();
}

void FrameCaret::RecreateCaretBlinkTimerForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  caret_blink_timer_.MoveToNewTaskRunner(std::move(task_runner));
}

bool FrameCaret::IsVisibleIfActiveForTesting() const {
  return display_item_client_->IsVisibleIfActive();
}

}  // namespace blink
