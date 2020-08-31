// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_POSITION_H_
#define EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_POSITION_H_

#include "gin/wrappable.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_tree_id.h"

namespace gin {
class Arguments;
}

namespace extensions {

// A class that wraps an ui::AXPosition to make available in javascript.
class AutomationPosition final : public gin::Wrappable<AutomationPosition> {
 public:
  AutomationPosition(const ui::AXNode& node, int offset, bool is_upstream);
  ~AutomationPosition() override;

  static gin::WrapperInfo kWrapperInfo;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

 private:
  std::string GetTreeID(gin::Arguments* arguments);
  int GetAnchorID(gin::Arguments* arguments);
  int GetChildIndex(gin::Arguments* arguments);
  int GetTextOffset(gin::Arguments* arguments);
  std::string GetAffinity(gin::Arguments* arguments);
  bool IsNullPosition(gin::Arguments* arguments);
  bool IsTreePosition(gin::Arguments* arguments);
  bool IsTextPosition(gin::Arguments* arguments);
  bool IsLeafTextPosition(gin::Arguments* arguments);
  bool AtStartOfAnchor(gin::Arguments* arguments);
  bool AtEndOfAnchor(gin::Arguments* arguments);
  bool AtStartOfWord(gin::Arguments* arguments);
  bool AtEndOfWord(gin::Arguments* arguments);
  bool AtStartOfLine(gin::Arguments* arguments);
  bool AtEndOfLine(gin::Arguments* arguments);
  bool AtStartOfParagraph(gin::Arguments* arguments);
  bool AtEndOfParagraph(gin::Arguments* arguments);
  bool AtStartOfPage(gin::Arguments* arguments);
  bool AtEndOfPage(gin::Arguments* arguments);
  bool AtStartOfFormat(gin::Arguments* arguments);
  bool AtEndOfFormat(gin::Arguments* arguments);
  bool AtStartOfDocument(gin::Arguments* arguments);
  bool AtEndOfDocument(gin::Arguments* arguments);
  void AsTreePosition(gin::Arguments* arguments);
  void AsTextPosition(gin::Arguments* arguments);
  void AsLeafTextPosition(gin::Arguments* arguments);
  void MoveToPositionAtStartOfAnchor(gin::Arguments* arguments);
  void MoveToPositionAtEndOfAnchor(gin::Arguments* arguments);
  void MoveToPositionAtStartOfDocument(gin::Arguments* arguments);
  void MoveToPositionAtEndOfDocument(gin::Arguments* arguments);
  void MoveToParentPosition(gin::Arguments* arguments);
  void MoveToNextLeafTreePosition(gin::Arguments* arguments);
  void MoveToPreviousLeafTreePosition(gin::Arguments* arguments);
  void MoveToNextLeafTextPosition(gin::Arguments* arguments);
  void MoveToPreviousLeafTextPosition(gin::Arguments* arguments);
  void MoveToNextCharacterPosition(gin::Arguments* arguments);
  void MoveToPreviousCharacterPosition(gin::Arguments* arguments);
  void MoveToNextWordStartPosition(gin::Arguments* arguments);
  void MoveToPreviousWordStartPosition(gin::Arguments* arguments);
  void MoveToNextWordEndPosition(gin::Arguments* arguments);
  void MoveToPreviousWordEndPosition(gin::Arguments* arguments);
  void MoveToNextLineStartPosition(gin::Arguments* arguments);
  void MoveToPreviousLineStartPosition(gin::Arguments* arguments);
  void MoveToNextLineEndPosition(gin::Arguments* arguments);
  void MoveToPreviousLineEndPosition(gin::Arguments* arguments);
  void MoveToPreviousFormatStartPosition(gin::Arguments* arguments);
  void MoveToNextFormatEndPosition(gin::Arguments* arguments);
  void MoveToNextParagraphStartPosition(gin::Arguments* arguments);
  void MoveToPreviousParagraphStartPosition(gin::Arguments* arguments);
  void MoveToNextParagraphEndPosition(gin::Arguments* arguments);
  void MoveToPreviousParagraphEndPosition(gin::Arguments* arguments);
  void MoveToNextPageStartPosition(gin::Arguments* arguments);
  void MoveToPreviousPageStartPosition(gin::Arguments* arguments);
  void MoveToNextPageEndPosition(gin::Arguments* arguments);
  void MoveToPreviousPageEndPosition(gin::Arguments* arguments);
  void MoveToNextAnchorPosition(gin::Arguments* arguments);
  void MoveToPreviousAnchorPosition(gin::Arguments* arguments);
  int MaxTextOffset(gin::Arguments* arguments);
  bool IsInLineBreak(gin::Arguments* arguments);
  bool IsInTextObject(gin::Arguments* arguments);
  bool IsInWhiteSpace(gin::Arguments* arguments);
  bool IsValid(gin::Arguments* arguments);
  base::string16 GetText(gin::Arguments* arguments);

  ui::AXNodePosition::AXPositionInstance position_;

  DISALLOW_COPY_AND_ASSIGN(AutomationPosition);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_POSITION_H_
