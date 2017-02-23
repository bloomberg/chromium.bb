/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef UndoStep_h
#define UndoStep_h

#include "core/editing/VisibleSelection.h"
#include "core/events/InputEvent.h"
#include "platform/heap/Handle.h"

namespace blink {

class SimpleEditCommand;

class UndoStep : public GarbageCollectedFinalized<UndoStep> {
 public:
  static UndoStep* create(Document*,
                          const VisibleSelection&,
                          const VisibleSelection&,
                          InputEvent::InputType);

  void unapply();
  void reapply();
  InputEvent::InputType inputType() const;
  void append(SimpleEditCommand*);
  void append(UndoStep*);

  const VisibleSelection& startingSelection() const {
    return m_startingSelection;
  }
  const VisibleSelection& endingSelection() const { return m_endingSelection; }
  void setStartingSelection(const VisibleSelection&);
  void setEndingSelection(const VisibleSelection&);
  Element* startingRootEditableElement() const {
    return m_startingRootEditableElement.get();
  }
  Element* endingRootEditableElement() const {
    return m_endingRootEditableElement.get();
  }

  uint64_t sequenceNumber() const { return m_sequenceNumber; }

  DECLARE_TRACE();

 private:
  UndoStep(Document*,
           const VisibleSelection& startingSelection,
           const VisibleSelection& endingSelection,
           InputEvent::InputType);

  Member<Document> m_document;
  VisibleSelection m_startingSelection;
  VisibleSelection m_endingSelection;
  HeapVector<Member<SimpleEditCommand>> m_commands;
  Member<Element> m_startingRootEditableElement;
  Member<Element> m_endingRootEditableElement;
  InputEvent::InputType m_inputType;
  const uint64_t m_sequenceNumber;
};

}  // namespace blink

#endif
