// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/commands/UndoStep.h"

#include "core/editing/Editor.h"
#include "core/editing/commands/EditCommand.h"

namespace blink {

namespace {
uint64_t s_currentSequenceNumber = 0;
}

UndoStep* UndoStep::create(Document* document,
                           const VisibleSelection& startingSelection,
                           const VisibleSelection& endingSelection,
                           InputEvent::InputType inputType) {
  return new UndoStep(document, startingSelection, endingSelection, inputType);
}

UndoStep::UndoStep(Document* document,
                   const VisibleSelection& startingSelection,
                   const VisibleSelection& endingSelection,
                   InputEvent::InputType inputType)
    : m_document(document),
      m_startingSelection(startingSelection),
      m_endingSelection(endingSelection),
      m_startingRootEditableElement(startingSelection.rootEditableElement()),
      m_endingRootEditableElement(endingSelection.rootEditableElement()),
      m_inputType(inputType),
      m_sequenceNumber(++s_currentSequenceNumber) {}

void UndoStep::unapply() {
  DCHECK(m_document);
  LocalFrame* frame = m_document->frame();
  DCHECK(frame);

  // Changes to the document may have been made since the last editing operation
  // that require a layout, as in <rdar://problem/5658603>. Low level
  // operations, like RemoveNodeCommand, don't require a layout because the high
  // level operations that use them perform one if one is necessary (like for
  // the creation of VisiblePositions).
  m_document->updateStyleAndLayoutIgnorePendingStylesheets();

  {
    size_t size = m_commands.size();
    for (size_t i = size; i; --i)
      m_commands[i - 1]->doUnapply();
  }

  frame->editor().unappliedEditing(this);
}

void UndoStep::reapply() {
  DCHECK(m_document);
  LocalFrame* frame = m_document->frame();
  DCHECK(frame);

  // Changes to the document may have been made since the last editing operation
  // that require a layout, as in <rdar://problem/5658603>. Low level
  // operations, like RemoveNodeCommand, don't require a layout because the high
  // level operations that use them perform one if one is necessary (like for
  // the creation of VisiblePositions).
  m_document->updateStyleAndLayoutIgnorePendingStylesheets();

  {
    for (const auto& command : m_commands)
      command->doReapply();
  }

  frame->editor().reappliedEditing(this);
}

InputEvent::InputType UndoStep::inputType() const {
  return m_inputType;
}

void UndoStep::append(SimpleEditCommand* command) {
  m_commands.push_back(command);
}

void UndoStep::append(UndoStep* undoStep) {
  m_commands.appendVector(undoStep->m_commands);
}

void UndoStep::setStartingSelection(const VisibleSelection& selection) {
  m_startingSelection = selection;
  m_startingRootEditableElement = selection.rootEditableElement();
}

void UndoStep::setEndingSelection(const VisibleSelection& selection) {
  m_endingSelection = selection;
  m_endingRootEditableElement = selection.rootEditableElement();
}

DEFINE_TRACE(UndoStep) {
  visitor->trace(m_document);
  visitor->trace(m_startingSelection);
  visitor->trace(m_endingSelection);
  visitor->trace(m_commands);
  visitor->trace(m_startingRootEditableElement);
  visitor->trace(m_endingRootEditableElement);
}

}  // namespace blink
