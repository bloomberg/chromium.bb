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
  static UndoStep* Create(Document*,
                          const VisibleSelection&,
                          const VisibleSelection&,
                          InputEvent::InputType);

  void Unapply();
  void Reapply();
  InputEvent::InputType GetInputType() const;
  void Append(SimpleEditCommand*);
  void Append(UndoStep*);

  const VisibleSelection& StartingSelection() const {
    return starting_selection_;
  }
  const VisibleSelection& EndingSelection() const { return ending_selection_; }
  void SetStartingSelection(const VisibleSelection&);
  void SetEndingSelection(const VisibleSelection&);
  Element* StartingRootEditableElement() const {
    return starting_root_editable_element_.Get();
  }
  Element* EndingRootEditableElement() const {
    return ending_root_editable_element_.Get();
  }

  uint64_t SequenceNumber() const { return sequence_number_; }

  DECLARE_TRACE();

 private:
  UndoStep(Document*,
           const VisibleSelection& starting_selection,
           const VisibleSelection& ending_selection,
           InputEvent::InputType);

  Member<Document> document_;
  VisibleSelection starting_selection_;
  VisibleSelection ending_selection_;
  HeapVector<Member<SimpleEditCommand>> commands_;
  Member<Element> starting_root_editable_element_;
  Member<Element> ending_root_editable_element_;
  InputEvent::InputType input_type_;
  const uint64_t sequence_number_;
};

}  // namespace blink

#endif
