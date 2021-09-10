/*
 * Copyright (C) 2006, 2007 Apple, Inc.  All rights reserved.
 * Copyright (C) 2012 Google, Inc.  All rights reserved.
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

#include "third_party/blink/renderer/core/editing/commands/undo_stack.h"

#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/editing/commands/undo_step.h"

namespace blink {

// Arbitrary depth limit for the undo stack, to keep it from using
// unbounded memory. This is the maximum number of distinct undoable
// actions -- unbroken stretches of typed characters are coalesced
// into a single action.
static const size_t kMaximumUndoStackDepth = 1000;

UndoStack::UndoStack() = default;

void UndoStack::RegisterUndoStep(UndoStep* step) {
  EnsureListeningMemoryPressure();
  if (!undo_stack_.IsEmpty())
    DCHECK_GE(step->SequenceNumber(), undo_stack_.back()->SequenceNumber());
  if (undo_stack_.size() == kMaximumUndoStackDepth) {
    // Drop the oldest item off the far end.
    undo_stack_.erase(undo_stack_.begin());
  }
  if (!in_redo_)
    redo_stack_.clear();
  undo_stack_.push_back(step);
}

void UndoStack::RegisterRedoStep(UndoStep* step) {
  EnsureListeningMemoryPressure();
  redo_stack_.push_back(step);
}

bool UndoStack::CanUndo() const {
  return !undo_stack_.IsEmpty();
}

bool UndoStack::CanRedo() const {
  return !redo_stack_.IsEmpty();
}

void UndoStack::Undo() {
  if (!CanUndo())
    return;
  UndoStep* const step = undo_stack_.back();
  undo_stack_.pop_back();
  step->Unapply();
  // unapply will call us back to push this command onto the redo stack.
}

void UndoStack::Redo() {
  if (!CanRedo())
    return;
  UndoStep* const step = redo_stack_.back();
  redo_stack_.pop_back();

  DCHECK(!in_redo_);
  base::AutoReset<bool> redo_scope(&in_redo_, true);
  step->Reapply();
  // reapply will call us back to push this command onto the undo stack.
}

void UndoStack::Clear() {
  undo_stack_.clear();
  redo_stack_.clear();
  StopListeningMemoryPressure();
}

void UndoStack::Trace(Visitor* visitor) const {
  visitor->Trace(undo_stack_);
  visitor->Trace(redo_stack_);
}

UndoStack::UndoStepRange::UndoStepRange(const UndoStepStack& steps)
    : step_stack_(steps) {}

UndoStack::UndoStepRange UndoStack::RedoSteps() const {
  return UndoStepRange(redo_stack_);
}

UndoStack::UndoStepRange UndoStack::UndoSteps() const {
  return UndoStepRange(undo_stack_);
}

void UndoStack::EnsureListeningMemoryPressure() {
  if (is_listen_memory_pressure_)
    return;
  MemoryPressureListenerRegistry::Instance().RegisterClient(this);
  is_listen_memory_pressure_ = true;
}

void UndoStack::StopListeningMemoryPressure() {
  if (!is_listen_memory_pressure_)
    return;
  DCHECK(undo_stack_.IsEmpty());
  DCHECK(redo_stack_.IsEmpty());
  MemoryPressureListenerRegistry::Instance().UnregisterClient(this);
  is_listen_memory_pressure_ = false;
}

void UndoStack::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel) {
  // In design mode, every root editable elements can be reinserted.
  if (!undo_stack_.IsEmpty() &&
      undo_stack_.front()->GetDocument().InDesignMode())
    return;
  if (!redo_stack_.IsEmpty() &&
      redo_stack_.front()->GetDocument().InDesignMode())
    return;

  const auto is_disconnected = [](const UndoStep* undo_step) {
    return !undo_step->IsConnected();
  };

  undo_stack_.erase(
      std::remove_if(undo_stack_.begin(), undo_stack_.end(), is_disconnected),
      undo_stack_.end());

  redo_stack_.erase(
      std::remove_if(redo_stack_.begin(), redo_stack_.end(), is_disconnected),
      redo_stack_.end());
}

}  // namespace blink
