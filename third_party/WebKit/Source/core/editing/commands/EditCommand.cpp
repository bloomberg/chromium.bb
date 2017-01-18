/*
 * Copyright (C) 2005, 2006, 2007 Apple, Inc.  All rights reserved.
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

#include "core/editing/commands/EditCommand.h"

#include "core/dom/Document.h"
#include "core/dom/NodeTraversal.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/commands/CompositeEditCommand.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutText.h"

namespace blink {

EditCommand::EditCommand(Document& document)
    : m_document(&document), m_parent(nullptr) {
  DCHECK(m_document);
  DCHECK(m_document->frame());
}

EditCommand::~EditCommand() {}

InputEvent::InputType EditCommand::inputType() const {
  return InputEvent::InputType::None;
}

String EditCommand::textDataForInputEvent() const {
  return nullAtom;
}

// TODO(xiaochengh): Move it to CompositeEditCommand.cpp
void CompositeEditCommand::setStartingSelection(
    const VisibleSelection& selection) {
  for (CompositeEditCommand* command = this;; command = command->parent()) {
    if (UndoStep* undoStep = command->undoStep()) {
      DCHECK(command->isTopLevelCommand());
      undoStep->setStartingSelection(selection);
    }
    command->m_startingSelection = selection;
    if (!command->parent() || command->parent()->isFirstCommand(command))
      break;
  }
}

// TODO(xiaochengh): Move it to CompositeEditCommand.cpp
// TODO(yosin): We will make |SelectionInDOMTree| version of
// |setEndingSelection()| as primary function instead of wrapper, once
// |EditCommand| holds other than |VisibleSelection|.
void CompositeEditCommand::setEndingSelection(
    const SelectionInDOMTree& selection) {
  // TODO(editing-dev): The use of
  // updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  document().updateStyleAndLayoutIgnorePendingStylesheets();
  setEndingVisibleSelection(createVisibleSelection(selection));
}

// TODO(xiaochengh): Move it to CompositeEditCommand.cpp
// TODO(yosin): We will make |SelectionInDOMTree| version of
// |setEndingSelection()| as primary function instead of wrapper.
void CompositeEditCommand::setEndingVisibleSelection(
    const VisibleSelection& selection) {
  for (CompositeEditCommand* command = this; command;
       command = command->parent()) {
    if (UndoStep* undoStep = command->undoStep()) {
      DCHECK(command->isTopLevelCommand());
      undoStep->setEndingSelection(selection);
    }
    command->m_endingSelection = selection;
  }
}

bool EditCommand::isRenderedCharacter(const Position& position) {
  if (position.isNull())
    return false;
  DCHECK(position.isOffsetInAnchor()) << position;
  if (!position.anchorNode()->isTextNode())
    return false;

  LayoutObject* layoutObject = position.anchorNode()->layoutObject();
  if (!layoutObject)
    return false;

  return toLayoutText(layoutObject)
      ->isRenderedCharacter(position.offsetInContainerNode());
}

void EditCommand::setParent(CompositeEditCommand* parent) {
  DCHECK((parent && !m_parent) || (!parent && m_parent));
  DCHECK(!parent || !isCompositeEditCommand() ||
         !toCompositeEditCommand(this)->undoStep());
  m_parent = parent;
}

// TODO(xiaochengh): Move it to CompositeEditCommand.cpp
void CompositeEditCommand::setParent(CompositeEditCommand* parent) {
  EditCommand::setParent(parent);
  if (parent) {
    m_startingSelection = parent->m_endingSelection;
    m_endingSelection = parent->m_endingSelection;
  }
}

void SimpleEditCommand::doReapply() {
  EditingState editingState;
  doApply(&editingState);
}

DEFINE_TRACE(EditCommand) {
  visitor->trace(m_document);
  visitor->trace(m_parent);
}

}  // namespace blink
