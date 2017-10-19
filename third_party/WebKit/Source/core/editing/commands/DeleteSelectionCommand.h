/*
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef DeleteSelectionCommand_h
#define DeleteSelectionCommand_h

#include "core/editing/VisibleSelection.h"
#include "core/editing/commands/CompositeEditCommand.h"

namespace blink {

class EditingStyle;
class HTMLTableRowElement;

class CORE_EXPORT DeleteSelectionCommand final : public CompositeEditCommand {
 public:
  static DeleteSelectionCommand* Create(
      Document& document,
      bool smart_delete = false,
      bool merge_blocks_after_delete = true,
      bool expand_for_special_elements = false,
      bool sanitize_markup = true,
      InputEvent::InputType input_type = InputEvent::InputType::kNone,
      const Position& reference_move_position = Position()) {
    return new DeleteSelectionCommand(
        document, smart_delete, merge_blocks_after_delete,
        expand_for_special_elements, sanitize_markup, input_type,
        reference_move_position);
  }
  static DeleteSelectionCommand* Create(
      const VisibleSelection& selection,
      bool smart_delete = false,
      bool merge_blocks_after_delete = true,
      bool expand_for_special_elements = false,
      bool sanitize_markup = true,
      InputEvent::InputType input_type = InputEvent::InputType::kNone) {
    return new DeleteSelectionCommand(
        selection, smart_delete, merge_blocks_after_delete,
        expand_for_special_elements, sanitize_markup, input_type);
  }

  virtual void Trace(blink::Visitor*);

 private:
  DeleteSelectionCommand(Document&,
                         bool smart_delete,
                         bool merge_blocks_after_delete,
                         bool expand_for_special_elements,
                         bool santize_markup,
                         InputEvent::InputType,
                         const Position& reference_move_position);
  DeleteSelectionCommand(const VisibleSelection&,
                         bool smart_delete,
                         bool merge_blocks_after_delete,
                         bool expand_for_special_elements,
                         bool sanitize_markup,
                         InputEvent::InputType);

  void DoApply(EditingState*) override;
  InputEvent::InputType GetInputType() const override;

  bool PreservesTypingStyle() const override;

  void InitializeStartEnd(Position&, Position&);
  void SetStartingSelectionOnSmartDelete(const Position&, const Position&);
  void InitializePositionData(EditingState*);
  void SaveTypingStyleState();
  bool HandleSpecialCaseBRDelete(EditingState*);
  void HandleGeneralDelete(EditingState*);
  void FixupWhitespace();
  void MergeParagraphs(EditingState*);
  void RemovePreviouslySelectedEmptyTableRows(EditingState*);
  void CalculateTypingStyleAfterDelete();
  void ClearTransientState();
  void MakeStylingElementsDirectChildrenOfEditableRootToPreventStyleLoss(
      EditingState*);
  void RemoveNode(Node*,
                  EditingState*,
                  ShouldAssumeContentIsAlwaysEditable =
                      kDoNotAssumeContentIsAlwaysEditable) override;
  void DeleteTextFromNode(Text*, unsigned, unsigned) override;
  void RemoveRedundantBlocks(EditingState*);

  bool has_selection_to_delete_;
  bool smart_delete_;
  bool merge_blocks_after_delete_;
  bool need_placeholder_;
  bool expand_for_special_elements_;
  bool prune_start_block_if_necessary_;
  bool starts_at_empty_line_;
  bool sanitize_markup_;
  InputEvent::InputType input_type_;

  // This data is transient and should be cleared at the end of the doApply
  // function.
  VisibleSelection selection_to_delete_;
  Position upstream_start_;
  Position downstream_start_;
  Position upstream_end_;
  Position downstream_end_;
  Position ending_position_;
  Position leading_whitespace_;
  Position trailing_whitespace_;
  Position reference_move_position_;
  Member<Node> start_block_;
  Member<Node> end_block_;
  Member<EditingStyle> typing_style_;
  Member<EditingStyle> delete_into_blockquote_style_;
  Member<Element> start_root_;
  Member<Element> end_root_;
  Member<HTMLTableRowElement> start_table_row_;
  Member<HTMLTableRowElement> end_table_row_;
  Member<Node> temporary_placeholder_;
};

}  // namespace blink

#endif  // DeleteSelectionCommand_h
