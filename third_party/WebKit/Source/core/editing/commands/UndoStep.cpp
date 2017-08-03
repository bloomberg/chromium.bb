// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/commands/UndoStep.h"

#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/commands/EditCommand.h"

namespace blink {

namespace {
uint64_t g_current_sequence_number = 0;
}

UndoStep* UndoStep::Create(Document* document,
                           const SelectionForUndoStep& starting_selection,
                           const SelectionForUndoStep& ending_selection,
                           InputEvent::InputType input_type) {
  return new UndoStep(document, starting_selection, ending_selection,
                      input_type);
}

UndoStep::UndoStep(Document* document,
                   const SelectionForUndoStep& starting_selection,
                   const SelectionForUndoStep& ending_selection,
                   InputEvent::InputType input_type)
    : document_(document),
      starting_selection_(starting_selection),
      ending_selection_(ending_selection),
      starting_root_editable_element_(
          RootEditableElementOf(starting_selection.Base())),
      ending_root_editable_element_(
          RootEditableElementOf(ending_selection.Base())),
      input_type_(input_type),
      sequence_number_(++g_current_sequence_number) {}

void UndoStep::Unapply() {
  DCHECK(document_);
  LocalFrame* frame = document_->GetFrame();
  DCHECK(frame);

  // Changes to the document may have been made since the last editing operation
  // that require a layout, as in <rdar://problem/5658603>. Low level
  // operations, like RemoveNodeCommand, don't require a layout because the high
  // level operations that use them perform one if one is necessary (like for
  // the creation of VisiblePositions).
  document_->UpdateStyleAndLayoutIgnorePendingStylesheets();

  {
    size_t size = commands_.size();
    for (size_t i = size; i; --i)
      commands_[i - 1]->DoUnapply();
  }

  frame->GetEditor().UnappliedEditing(this);
}

void UndoStep::Reapply() {
  DCHECK(document_);
  LocalFrame* frame = document_->GetFrame();
  DCHECK(frame);

  // Changes to the document may have been made since the last editing operation
  // that require a layout, as in <rdar://problem/5658603>. Low level
  // operations, like RemoveNodeCommand, don't require a layout because the high
  // level operations that use them perform one if one is necessary (like for
  // the creation of VisiblePositions).
  document_->UpdateStyleAndLayoutIgnorePendingStylesheets();

  {
    for (const auto& command : commands_)
      command->DoReapply();
  }

  frame->GetEditor().ReappliedEditing(this);
}

InputEvent::InputType UndoStep::GetInputType() const {
  return input_type_;
}

void UndoStep::Append(SimpleEditCommand* command) {
  commands_.push_back(command);
}

void UndoStep::Append(UndoStep* undo_step) {
  commands_.AppendVector(undo_step->commands_);
}

void UndoStep::SetStartingSelection(const SelectionForUndoStep& selection) {
  starting_selection_ = selection;
  starting_root_editable_element_ = RootEditableElementOf(selection.Base());
}

void UndoStep::SetEndingSelection(const SelectionForUndoStep& selection) {
  ending_selection_ = selection;
  ending_root_editable_element_ = RootEditableElementOf(selection.Base());
}

DEFINE_TRACE(UndoStep) {
  visitor->Trace(document_);
  visitor->Trace(starting_selection_);
  visitor->Trace(ending_selection_);
  visitor->Trace(commands_);
  visitor->Trace(starting_root_editable_element_);
  visitor->Trace(ending_root_editable_element_);
}

}  // namespace blink
