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
  setBodyContent("<div contenteditable></div>");

  // <input><form></form></input>
  Element* input1 = document().createElement("input");
  Element* form = document().createElement("form");
  input1->appendChild(form);

  // <tr><input><header></header></input><rbc></rbc></tr>
  Element* tr = document().createElement("tr");
  Element* input2 = document().createElement("input");
  Element* header = document().createElement("header");
  Element* rbc = document().createElement("rbc");
  input2->appendChild(header);
  tr->appendChild(input2);
  tr->appendChild(rbc);

  Element* div = document().querySelector("div");
  div->appendChild(input1);
  div->appendChild(tr);

  LocalFrame* frame = document().frame();
  frame->selection().setSelection(SelectionInDOMTree::Builder()
                                      .collapse(Position(form, 0))
                                      .extend(Position(header, 0))
                                      .build());

  // Inserting line break should not crash or hit assertion.
  TypingCommand::insertLineBreak(document());
}

}  // namespace blink
