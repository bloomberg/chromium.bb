// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/commands/ApplyStyleCommand.h"

#include "core/css/StylePropertySet.h"
#include "core/dom/Document.h"
#include "core/editing/EditingStyle.h"
#include "core/editing/EditingTestBase.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/LocalFrame.h"

namespace blink {

class ApplyStyleCommandTest : public EditingTestBase {};

// This is a regression test for https://crbug.com/675727
TEST_F(ApplyStyleCommandTest, RemoveRedundantBlocksWithStarEditableStyle) {
  // The second <div> below is redundant from Blink's perspective (no siblings
  // && no attributes) and will be removed by
  // |DeleteSelectionCommand::removeRedundantBlocks()|.
  setBodyContent(
      "<div><div>"
      "<div></div>"
      "<ul>"
      "<li>"
      "<div></div>"
      "<input>"
      "<style> * {-webkit-user-modify: read-write;}</style><div></div>"
      "</li>"
      "</ul></div></div>");

  Element* li = document().querySelector("li");

  LocalFrame* frame = document().frame();
  frame->selection().setSelection(
      SelectionInDOMTree::Builder()
          .collapse(Position(li, PositionAnchorType::BeforeAnchor))
          .build());

  MutableStylePropertySet* style =
      MutableStylePropertySet::create(HTMLQuirksMode);
  style->setProperty(CSSPropertyTextAlign, "center");
  ApplyStyleCommand::create(document(), EditingStyle::create(style),
                            InputEvent::InputType::FormatJustifyCenter,
                            ApplyStyleCommand::ForceBlockProperties)
      ->apply();
  // Shouldn't crash.
}

}  // namespace blink
