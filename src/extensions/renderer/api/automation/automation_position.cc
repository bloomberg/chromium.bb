// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/automation/automation_position.h"

#include "gin/arguments.h"
#include "gin/object_template_builder.h"
#include "ui/accessibility/ax_node.h"

namespace extensions {

AutomationPosition::AutomationPosition(const ui::AXNode& node,
                                       int offset,
                                       bool is_upstream) {
  position_ = ui::AXNodePosition::CreatePosition(
      node, offset,
      is_upstream ? ax::mojom::TextAffinity::kUpstream
                  : ax::mojom::TextAffinity::kDownstream);
}

AutomationPosition::~AutomationPosition() = default;

// static
gin::WrapperInfo AutomationPosition::kWrapperInfo = {gin::kEmbedderNativeGin};

gin::ObjectTemplateBuilder AutomationPosition::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return Wrappable<AutomationPosition>::GetObjectTemplateBuilder(isolate)
      .SetProperty("treeID", &AutomationPosition::GetTreeID)
      .SetProperty("anchorID", &AutomationPosition::GetAnchorID)
      .SetProperty("childIndex", &AutomationPosition::GetChildIndex)
      .SetProperty("textOffset", &AutomationPosition::GetTextOffset)
      .SetProperty("affinity", &AutomationPosition::GetAffinity)
      .SetMethod("isNullPosition", &AutomationPosition::IsNullPosition)
      .SetMethod("isTreePosition", &AutomationPosition::IsTreePosition)
      .SetMethod("isTextPosition", &AutomationPosition::IsTextPosition)
      .SetMethod("isLeafTextPosition", &AutomationPosition::IsLeafTextPosition)
      .SetMethod("atStartOfAnchor", &AutomationPosition::AtStartOfAnchor)
      .SetMethod("atEndOfAnchor", &AutomationPosition::AtEndOfAnchor)
      .SetMethod("atStartOfWord", &AutomationPosition::AtStartOfWord)
      .SetMethod("atEndOfWord", &AutomationPosition::AtEndOfWord)
      .SetMethod("atStartOfLine", &AutomationPosition::AtStartOfLine)
      .SetMethod("atEndOfLine", &AutomationPosition::AtEndOfLine)
      .SetMethod("atStartOfParagraph", &AutomationPosition::AtStartOfParagraph)
      .SetMethod("atEndOfParagraph", &AutomationPosition::AtEndOfParagraph)
      .SetMethod("atStartOfPage", &AutomationPosition::AtStartOfPage)
      .SetMethod("atEndOfPage", &AutomationPosition::AtEndOfPage)
      .SetMethod("atStartOfFormat", &AutomationPosition::AtStartOfFormat)
      .SetMethod("atEndOfFormat", &AutomationPosition::AtEndOfFormat)
      .SetMethod("atStartOfDocument", &AutomationPosition::AtStartOfDocument)
      .SetMethod("atEndOfDocument", &AutomationPosition::AtEndOfDocument)
      .SetMethod("asTreePosition", &AutomationPosition::AsTreePosition)
      .SetMethod("asTextPosition", &AutomationPosition::AsTextPosition)
      .SetMethod("asLeafTextPosition", &AutomationPosition::AsLeafTextPosition)
      .SetMethod("moveToPositionAtStartOfAnchor",
                 &AutomationPosition::MoveToPositionAtStartOfAnchor)
      .SetMethod("moveToPositionAtEndOfAnchor",
                 &AutomationPosition::MoveToPositionAtEndOfAnchor)
      .SetMethod("moveToPositionAtStartOfDocument",
                 &AutomationPosition::MoveToPositionAtStartOfDocument)
      .SetMethod("moveToPositionAtEndOfDocument",
                 &AutomationPosition::MoveToPositionAtEndOfDocument)
      .SetMethod("moveToParentPosition",
                 &AutomationPosition::MoveToParentPosition)
      .SetMethod("moveToNextLeafTreePosition",
                 &AutomationPosition::MoveToNextLeafTreePosition)
      .SetMethod("moveToPreviousLeafTreePosition",
                 &AutomationPosition::MoveToPreviousLeafTreePosition)
      .SetMethod("moveToNextLeafTextPosition",
                 &AutomationPosition::MoveToNextLeafTextPosition)
      .SetMethod("moveToPreviousLeafTextPosition",
                 &AutomationPosition::MoveToPreviousLeafTextPosition)
      .SetMethod("moveToNextCharacterPosition",
                 &AutomationPosition::MoveToNextCharacterPosition)
      .SetMethod("moveToPreviousCharacterPosition",
                 &AutomationPosition::MoveToPreviousCharacterPosition)
      .SetMethod("moveToNextWordStartPosition",
                 &AutomationPosition::MoveToNextWordStartPosition)
      .SetMethod("moveToPreviousWordStartPosition",
                 &AutomationPosition::MoveToPreviousWordStartPosition)
      .SetMethod("moveToNextWordEndPosition",
                 &AutomationPosition::MoveToNextWordEndPosition)
      .SetMethod("moveToPreviousWordEndPosition",
                 &AutomationPosition::MoveToPreviousWordEndPosition)
      .SetMethod("moveToNextLineStartPosition",
                 &AutomationPosition::MoveToNextLineStartPosition)
      .SetMethod("moveToPreviousLineStartPosition",
                 &AutomationPosition::MoveToPreviousLineStartPosition)
      .SetMethod("moveToNextLineEndPosition",
                 &AutomationPosition::MoveToNextLineEndPosition)
      .SetMethod("moveToPreviousLineEndPosition",
                 &AutomationPosition::MoveToPreviousLineEndPosition)
      .SetMethod("moveToPreviousFormatStartPosition",
                 &AutomationPosition::MoveToPreviousFormatStartPosition)
      .SetMethod("moveToNextFormatEndPosition",
                 &AutomationPosition::MoveToNextFormatEndPosition)
      .SetMethod("moveToNextParagraphStartPosition",
                 &AutomationPosition::MoveToNextParagraphStartPosition)
      .SetMethod("moveToPreviousParagraphStartPosition",
                 &AutomationPosition::MoveToPreviousParagraphStartPosition)
      .SetMethod("moveToNextParagraphEndPosition",
                 &AutomationPosition::MoveToNextParagraphEndPosition)
      .SetMethod("moveToPreviousParagraphEndPosition",
                 &AutomationPosition::MoveToPreviousParagraphEndPosition)
      .SetMethod("moveToNextPageStartPosition",
                 &AutomationPosition::MoveToNextPageStartPosition)
      .SetMethod("moveToPreviousPageStartPosition",
                 &AutomationPosition::MoveToPreviousPageStartPosition)
      .SetMethod("moveToNextPageEndPosition",
                 &AutomationPosition::MoveToNextPageEndPosition)
      .SetMethod("moveToPreviousPageEndPosition",
                 &AutomationPosition::MoveToPreviousPageEndPosition)
      .SetMethod("moveToNextAnchorPosition",
                 &AutomationPosition::MoveToNextAnchorPosition)
      .SetMethod("moveToPreviousAnchorPosition",
                 &AutomationPosition::MoveToPreviousAnchorPosition)
      .SetMethod("maxTextOffset", &AutomationPosition::MaxTextOffset)
      .SetMethod("isInLineBreak", &AutomationPosition::IsInLineBreak)
      .SetMethod("isInTextObject", &AutomationPosition::IsInTextObject)
      .SetMethod("isInWhiteSpace", &AutomationPosition::IsInWhiteSpace)
      .SetMethod("isValid", &AutomationPosition::IsValid)
      .SetMethod("getText", &AutomationPosition::GetText);
}

std::string AutomationPosition::GetTreeID(gin::Arguments* arguments) {
  return position_->tree_id().ToString();
}

int AutomationPosition::GetAnchorID(gin::Arguments* arguments) {
  return position_->anchor_id();
}

int AutomationPosition::GetChildIndex(gin::Arguments* arguments) {
  return position_->child_index();
}

int AutomationPosition::GetTextOffset(gin::Arguments* arguments) {
  return position_->text_offset();
}

std::string AutomationPosition::GetAffinity(gin::Arguments* arguments) {
  return ui::ToString(position_->affinity());
}

bool AutomationPosition::IsNullPosition(gin::Arguments* arguments) {
  return position_->IsNullPosition();
}

bool AutomationPosition::IsTreePosition(gin::Arguments* arguments) {
  return position_->IsTreePosition();
}

bool AutomationPosition::IsTextPosition(gin::Arguments* arguments) {
  return position_->IsTextPosition();
}

bool AutomationPosition::IsLeafTextPosition(gin::Arguments* arguments) {
  return position_->IsLeafTextPosition();
}

bool AutomationPosition::AtStartOfAnchor(gin::Arguments* arguments) {
  return position_->AtStartOfAnchor();
}

bool AutomationPosition::AtEndOfAnchor(gin::Arguments* arguments) {
  return position_->AtEndOfAnchor();
}

bool AutomationPosition::AtStartOfWord(gin::Arguments* arguments) {
  return position_->AtStartOfWord();
}

bool AutomationPosition::AtEndOfWord(gin::Arguments* arguments) {
  return position_->AtEndOfWord();
}

bool AutomationPosition::AtStartOfLine(gin::Arguments* arguments) {
  return position_->AtStartOfLine();
}

bool AutomationPosition::AtEndOfLine(gin::Arguments* arguments) {
  return position_->AtEndOfLine();
}

bool AutomationPosition::AtStartOfParagraph(gin::Arguments* arguments) {
  return position_->AtStartOfParagraph();
}

bool AutomationPosition::AtEndOfParagraph(gin::Arguments* arguments) {
  return position_->AtEndOfParagraph();
}

bool AutomationPosition::AtStartOfPage(gin::Arguments* arguments) {
  return position_->AtStartOfPage();
}

bool AutomationPosition::AtEndOfPage(gin::Arguments* arguments) {
  return position_->AtEndOfPage();
}

bool AutomationPosition::AtStartOfFormat(gin::Arguments* arguments) {
  return position_->AtStartOfFormat();
}

bool AutomationPosition::AtEndOfFormat(gin::Arguments* arguments) {
  return position_->AtEndOfFormat();
}

bool AutomationPosition::AtStartOfDocument(gin::Arguments* arguments) {
  return position_->AtStartOfDocument();
}

bool AutomationPosition::AtEndOfDocument(gin::Arguments* arguments) {
  return position_->AtEndOfDocument();
}

void AutomationPosition::AsTreePosition(gin::Arguments* arguments) {
  position_ = position_->AsTreePosition();
}

void AutomationPosition::AsTextPosition(gin::Arguments* arguments) {
  position_ = position_->AsTextPosition();
}

void AutomationPosition::AsLeafTextPosition(gin::Arguments* arguments) {
  position_ = position_->AsLeafTextPosition();
}

void AutomationPosition::MoveToPositionAtStartOfAnchor(
    gin::Arguments* arguments) {
  position_ = position_->CreatePositionAtStartOfAnchor();
}

void AutomationPosition::MoveToPositionAtEndOfAnchor(
    gin::Arguments* arguments) {
  position_ = position_->CreatePositionAtEndOfAnchor();
}

void AutomationPosition::MoveToPositionAtStartOfDocument(
    gin::Arguments* arguments) {
  position_ = position_->CreatePositionAtStartOfDocument();
}

void AutomationPosition::MoveToPositionAtEndOfDocument(
    gin::Arguments* arguments) {
  position_ = position_->CreatePositionAtEndOfDocument();
}

void AutomationPosition::MoveToParentPosition(gin::Arguments* arguments) {
  position_ = position_->CreateParentPosition();
}

void AutomationPosition::MoveToNextLeafTreePosition(gin::Arguments* arguments) {
  position_ = position_->CreateNextLeafTreePosition();
}

void AutomationPosition::MoveToPreviousLeafTreePosition(
    gin::Arguments* arguments) {
  position_ = position_->CreatePreviousLeafTreePosition();
}

void AutomationPosition::MoveToNextLeafTextPosition(gin::Arguments* arguments) {
  position_ = position_->CreateNextLeafTextPosition();
}

void AutomationPosition::MoveToPreviousLeafTextPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreatePreviousLeafTextPosition();
}

void AutomationPosition::MoveToNextCharacterPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreateNextCharacterPosition(
      ui::AXBoundaryBehavior::CrossBoundary);
}

void AutomationPosition::MoveToPreviousCharacterPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreatePreviousCharacterPosition(
      ui::AXBoundaryBehavior::CrossBoundary);
}

void AutomationPosition::MoveToNextWordStartPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreateNextWordStartPosition(
      ui::AXBoundaryBehavior::CrossBoundary);
}

void AutomationPosition::MoveToPreviousWordStartPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreatePreviousWordStartPosition(
      ui::AXBoundaryBehavior::CrossBoundary);
}

void AutomationPosition::MoveToNextWordEndPosition(gin::Arguments* arguments) {
  position_ = position_->CreateNextWordEndPosition(
      ui::AXBoundaryBehavior::CrossBoundary);
}

void AutomationPosition::MoveToPreviousWordEndPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreatePreviousWordEndPosition(
      ui::AXBoundaryBehavior::CrossBoundary);
}

void AutomationPosition::MoveToNextLineStartPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreateNextLineStartPosition(
      ui::AXBoundaryBehavior::CrossBoundary);
}

void AutomationPosition::MoveToPreviousLineStartPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreatePreviousLineStartPosition(
      ui::AXBoundaryBehavior::CrossBoundary);
}

void AutomationPosition::MoveToNextLineEndPosition(gin::Arguments* arguments) {
  position_ = position_->CreateNextLineEndPosition(
      ui::AXBoundaryBehavior::CrossBoundary);
}

void AutomationPosition::MoveToPreviousLineEndPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreatePreviousLineEndPosition(
      ui::AXBoundaryBehavior::CrossBoundary);
}

void AutomationPosition::MoveToPreviousFormatStartPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreatePreviousFormatStartPosition(
      ui::AXBoundaryBehavior::CrossBoundary);
}

void AutomationPosition::MoveToNextFormatEndPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreateNextFormatEndPosition(
      ui::AXBoundaryBehavior::CrossBoundary);
}

void AutomationPosition::MoveToNextParagraphStartPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreateNextParagraphStartPosition(
      ui::AXBoundaryBehavior::CrossBoundary);
}

void AutomationPosition::MoveToPreviousParagraphStartPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreatePreviousParagraphStartPosition(
      ui::AXBoundaryBehavior::CrossBoundary);
}

void AutomationPosition::MoveToNextParagraphEndPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreateNextParagraphEndPosition(
      ui::AXBoundaryBehavior::CrossBoundary);
}

void AutomationPosition::MoveToPreviousParagraphEndPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreatePreviousParagraphEndPosition(
      ui::AXBoundaryBehavior::CrossBoundary);
}

void AutomationPosition::MoveToNextPageStartPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreateNextPageStartPosition(
      ui::AXBoundaryBehavior::CrossBoundary);
}

void AutomationPosition::MoveToPreviousPageStartPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreatePreviousPageStartPosition(
      ui::AXBoundaryBehavior::CrossBoundary);
}

void AutomationPosition::MoveToNextPageEndPosition(gin::Arguments* arguments) {
  position_ = position_->CreateNextPageEndPosition(
      ui::AXBoundaryBehavior::CrossBoundary);
}

void AutomationPosition::MoveToPreviousPageEndPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreatePreviousPageEndPosition(
      ui::AXBoundaryBehavior::CrossBoundary);
}

void AutomationPosition::MoveToNextAnchorPosition(gin::Arguments* arguments) {
  position_ = position_->CreateNextAnchorPosition();
}

void AutomationPosition::MoveToPreviousAnchorPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreatePreviousAnchorPosition();
}

int AutomationPosition::MaxTextOffset(gin::Arguments* arguments) {
  return position_->MaxTextOffset();
}

bool AutomationPosition::IsInLineBreak(gin::Arguments* arguments) {
  return position_->IsInLineBreak();
}

bool AutomationPosition::IsInTextObject(gin::Arguments* arguments) {
  return position_->IsInTextObject();
}

bool AutomationPosition::IsInWhiteSpace(gin::Arguments* arguments) {
  return position_->IsInWhiteSpace();
}

bool AutomationPosition::IsValid(gin::Arguments* arguments) {
  return position_->IsValid();
}

base::string16 AutomationPosition::GetText(gin::Arguments* arguments) {
  return position_->GetText();
}

}  // namespace extensions
