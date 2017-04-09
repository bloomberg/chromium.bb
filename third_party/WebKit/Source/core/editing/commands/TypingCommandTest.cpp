// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/commands/TypingCommand.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/editing/EditingTestBase.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/Position.h"
#include "core/editing/VisibleSelection.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <memory>

namespace blink {

class TypingCommandTest : public EditingTestBase {};

// This is a regression test for https://crbug.com/585048
TEST_F(TypingCommandTest, insertLineBreakWithIllFormedHTML) {
  SetBodyContent("<div contenteditable></div>");

  // <input><form></form></input>
  Element* input1 = GetDocument().createElement("input");
  Element* form = GetDocument().createElement("form");
  input1->AppendChild(form);

  // <tr><input><header></header></input><rbc></rbc></tr>
  Element* tr = GetDocument().createElement("tr");
  Element* input2 = GetDocument().createElement("input");
  Element* header = GetDocument().createElement("header");
  Element* rbc = GetDocument().createElement("rbc");
  input2->AppendChild(header);
  tr->AppendChild(input2);
  tr->AppendChild(rbc);

  Element* div = GetDocument().QuerySelector("div");
  div->AppendChild(input1);
  div->AppendChild(tr);

  LocalFrame* frame = GetDocument().GetFrame();
  frame->Selection().SetSelection(SelectionInDOMTree::Builder()
                                      .Collapse(Position(form, 0))
                                      .Extend(Position(header, 0))
                                      .Build());

  // Inserting line break should not crash or hit assertion.
  TypingCommand::InsertLineBreak(GetDocument());
}

}  // namespace blink
