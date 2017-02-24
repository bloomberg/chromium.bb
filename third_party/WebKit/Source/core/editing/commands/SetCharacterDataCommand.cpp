// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/commands/SetCharacterDataCommand.h"

#include "core/editing/EditingUtilities.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutText.h"

namespace blink {

SetCharacterDataCommand::SetCharacterDataCommand(Text* node,
                                                 unsigned offset,
                                                 unsigned count,
                                                 const String& text)
    : SimpleEditCommand(node->document()),
      m_node(node),
      m_offset(offset),
      m_count(count),
      m_newText(text) {
  DCHECK(m_node);
  DCHECK_LE(m_offset, m_node->length());
  DCHECK_LE(m_offset + m_count, m_node->length());
  // Callers shouldn't be trying to perform no-op replacements
  DCHECK(!(count == 0 && text.length() == 0));
}

void SetCharacterDataCommand::doApply(EditingState*) {
  // TODO(editing-dev): The use of updateStyleAndLayoutTree()
  // needs to be audited.  See http://crbug.com/590369 for more details.
  document().updateStyleAndLayoutTree();
  if (!hasEditableStyle(*m_node))
    return;

  DummyExceptionStateForTesting exceptionState;
  m_previousTextForUndo =
      m_node->substringData(m_offset, m_count, exceptionState);
  if (exceptionState.hadException())
    return;

  const bool passwordEchoEnabled =
      document().settings() && document().settings()->getPasswordEchoEnabled();

  if (passwordEchoEnabled) {
    LayoutText* layoutText = m_node->layoutObject();
    if (layoutText && layoutText->isSecure()) {
      layoutText->momentarilyRevealLastTypedCharacter(m_offset +
                                                      m_newText.length() - 1);
    }
  }

  m_node->replaceData(m_offset, m_count, m_newText,
                      IGNORE_EXCEPTION_FOR_TESTING);
}

void SetCharacterDataCommand::doUnapply() {
  // TODO(editing-dev): The use of updateStyleAndLayoutTree()
  // needs to be audited.  See http://crbug.com/590369 for more details.
  document().updateStyleAndLayoutTree();
  if (!hasEditableStyle(*m_node))
    return;

  m_node->replaceData(m_offset, m_newText.length(), m_previousTextForUndo,
                      IGNORE_EXCEPTION_FOR_TESTING);
}

DEFINE_TRACE(SetCharacterDataCommand) {
  visitor->trace(m_node);
  SimpleEditCommand::trace(visitor);
}

}  // namespace blink
