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

#include "core/dom/TaskRunnerHelper.h"
#include "core/editing/CaretDisplayItemClient.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/SelectionEditor.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/TextControlElement.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/api/LayoutPartItem.h"
#include "core/page/Page.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

FrameCaret::FrameCaret(LocalFrame& frame,
                       const SelectionEditor& selectionEditor)
    : m_selectionEditor(&selectionEditor),
      m_frame(frame),
      m_displayItemClient(new CaretDisplayItemClient()),
      m_caretVisibility(CaretVisibility::Hidden),
      m_caretBlinkTimer(new TaskRunnerTimer<FrameCaret>(
          TaskRunnerHelper::get(TaskType::UnspecedTimer, &frame),
          this,
          &FrameCaret::caretBlinkTimerFired)),
      m_shouldPaintCaret(true),
      m_isCaretBlinkingSuspended(false),
      m_shouldShowBlockCursor(false) {}

FrameCaret::~FrameCaret() = default;

DEFINE_TRACE(FrameCaret) {
  visitor->trace(m_selectionEditor);
  visitor->trace(m_frame);
}

const DisplayItemClient& FrameCaret::displayItemClient() const {
  return *m_displayItemClient;
}

const PositionWithAffinity FrameCaret::caretPosition() const {
  const VisibleSelection& selection =
      m_selectionEditor->computeVisibleSelectionInDOMTree();
  if (!selection.isCaret())
    return PositionWithAffinity();
  return PositionWithAffinity(selection.start(), selection.affinity());
}

void FrameCaret::updateAppearance() {
  DCHECK_GE(m_frame->document()->lifecycle().state(),
            DocumentLifecycle::LayoutClean);
  // Paint a block cursor instead of a caret in overtype mode unless the caret
  // is at the end of a line (in this case the FrameSelection will paint a
  // blinking caret as usual).
  const bool paintBlockCursor =
      m_shouldShowBlockCursor && isActive() &&
      !isLogicalEndOfLine(createVisiblePosition(caretPosition()));

  bool shouldBlink = !paintBlockCursor && shouldBlinkCaret();
  if (!shouldBlink) {
    stopCaretBlinkTimer();
    return;
  }
  // Start blinking with a black caret. Be sure not to restart if we're
  // already blinking in the right location.
  startBlinkCaret();
}

void FrameCaret::stopCaretBlinkTimer() {
  if (m_caretBlinkTimer->isActive() || m_shouldPaintCaret)
    scheduleVisualUpdateForPaintInvalidationIfNeeded();
  m_shouldPaintCaret = false;
  m_caretBlinkTimer->stop();
}

void FrameCaret::startBlinkCaret() {
  // Start blinking with a black caret. Be sure not to restart if we're
  // already blinking in the right location.
  if (m_caretBlinkTimer->isActive())
    return;

  if (double blinkInterval = LayoutTheme::theme().caretBlinkInterval())
    m_caretBlinkTimer->startRepeating(blinkInterval, BLINK_FROM_HERE);

  m_shouldPaintCaret = true;
  scheduleVisualUpdateForPaintInvalidationIfNeeded();
}

void FrameCaret::setCaretVisibility(CaretVisibility visibility) {
  if (m_caretVisibility == visibility)
    return;

  m_caretVisibility = visibility;

  if (visibility == CaretVisibility::Hidden)
    stopCaretBlinkTimer();
  scheduleVisualUpdateForPaintInvalidationIfNeeded();
}

void FrameCaret::clearPreviousVisualRect(const LayoutBlock& block) {
  m_displayItemClient->clearPreviousVisualRect(block);
}

void FrameCaret::layoutBlockWillBeDestroyed(const LayoutBlock& block) {
  m_displayItemClient->layoutBlockWillBeDestroyed(block);
}

void FrameCaret::updateStyleAndLayoutIfNeeded() {
  DCHECK_GE(m_frame->document()->lifecycle().state(),
            DocumentLifecycle::LayoutClean);
  updateAppearance();
  bool shouldPaintCaret =
      m_shouldPaintCaret && isActive() &&
      m_caretVisibility == CaretVisibility::Visible &&
      m_selectionEditor->computeVisibleSelectionInDOMTree().hasEditableStyle();

  m_displayItemClient->updateStyleAndLayoutIfNeeded(
      shouldPaintCaret ? caretPosition() : PositionWithAffinity());
}

void FrameCaret::invalidatePaintIfNeeded(
    const LayoutBlock& block,
    const PaintInvalidatorContext& context) {
  m_displayItemClient->invalidatePaintIfNeeded(block, context);
}

bool FrameCaret::caretPositionIsValidForDocument(
    const Document& document) const {
  if (!isActive())
    return true;

  return caretPosition().document() == document && !caretPosition().isOrphan();
}

static IntRect absoluteBoundsForLocalRect(Node* node, const LayoutRect& rect) {
  LayoutBlock* caretPainter = CaretDisplayItemClient::caretLayoutBlock(node);
  if (!caretPainter)
    return IntRect();

  LayoutRect localRect(rect);
  return caretPainter->localToAbsoluteQuad(FloatRect(localRect))
      .enclosingBoundingBox();
}

IntRect FrameCaret::absoluteCaretBounds() const {
  DCHECK_NE(m_frame->document()->lifecycle().state(),
            DocumentLifecycle::InPaintInvalidation);
  DCHECK(!m_frame->document()->needsLayoutTreeUpdate());
  DocumentLifecycle::DisallowTransitionScope disallowTransition(
      m_frame->document()->lifecycle());

  Node* const caretNode = caretPosition().anchorNode();
  if (!isActive())
    return absoluteBoundsForLocalRect(caretNode, LayoutRect());
  return absoluteBoundsForLocalRect(
      caretNode,
      CaretDisplayItemClient::computeCaretRect(
          createVisiblePosition(caretPosition()).toPositionWithAffinity()));
}

void FrameCaret::setShouldShowBlockCursor(bool shouldShowBlockCursor) {
  m_shouldShowBlockCursor = shouldShowBlockCursor;
  scheduleVisualUpdateForPaintInvalidationIfNeeded();
}

bool FrameCaret::shouldPaintCaret(const LayoutBlock& block) const {
  return m_displayItemClient->shouldPaintCaret(block);
}

void FrameCaret::paintCaret(GraphicsContext& context,
                            const LayoutPoint& paintOffset) const {
  m_displayItemClient->paintCaret(context, paintOffset, DisplayItem::kCaret);
}

bool FrameCaret::shouldBlinkCaret() const {
  if (m_caretVisibility != CaretVisibility::Visible || !isActive())
    return false;

  Element* root = rootEditableElementOf(caretPosition().position());
  if (!root)
    return false;

  Element* focusedElement = root->document().focusedElement();
  if (!focusedElement)
    return false;

  return focusedElement->isShadowIncludingInclusiveAncestorOf(
      caretPosition().anchorNode());
}

void FrameCaret::caretBlinkTimerFired(TimerBase*) {
  DCHECK_EQ(m_caretVisibility, CaretVisibility::Visible);
  if (isCaretBlinkingSuspended() && m_shouldPaintCaret)
    return;
  m_shouldPaintCaret = !m_shouldPaintCaret;
  scheduleVisualUpdateForPaintInvalidationIfNeeded();
}

void FrameCaret::scheduleVisualUpdateForPaintInvalidationIfNeeded() {
  if (FrameView* frameView = m_frame->view())
    frameView->scheduleVisualUpdateForPaintInvalidationIfNeeded();
}

void FrameCaret::recreateCaretBlinkTimerForTesting(
    RefPtr<WebTaskRunner> taskRunner) {
  m_caretBlinkTimer.reset(new TaskRunnerTimer<FrameCaret>(
      std::move(taskRunner), this, &FrameCaret::caretBlinkTimerFired));
}

}  // namespace blink
