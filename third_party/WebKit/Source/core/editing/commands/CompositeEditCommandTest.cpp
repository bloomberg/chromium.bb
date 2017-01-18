// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/commands/CompositeEditCommand.h"

#include "core/css/StylePropertySet.h"
#include "core/dom/Document.h"
#include "core/editing/EditingTestBase.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/LocalFrame.h"

namespace blink {

namespace {

class SampleCommand final : public CompositeEditCommand {
 public:
  SampleCommand(Document&);

  void insertNodeBefore(
      Node*,
      Node* refChild,
      EditingState*,
      ShouldAssumeContentIsAlwaysEditable = DoNotAssumeContentIsAlwaysEditable);

  // CompositeEditCommand member implementations
  void doApply(EditingState*) final {}
  InputEvent::InputType inputType() const final {
    return InputEvent::InputType::None;
  }
};

SampleCommand::SampleCommand(Document& document)
    : CompositeEditCommand(document) {}

void SampleCommand::insertNodeBefore(
    Node* insertChild,
    Node* refChild,
    EditingState* editingState,
    ShouldAssumeContentIsAlwaysEditable shouldAssumeContentIsAlwaysEditable) {
  CompositeEditCommand::insertNodeBefore(insertChild, refChild, editingState,
                                         shouldAssumeContentIsAlwaysEditable);
}

}  // namespace

class CompositeEditCommandTest : public EditingTestBase {};

TEST_F(CompositeEditCommandTest, insertNodeBefore) {
  setBodyContent("<div contenteditable><b></b></div>");
  SampleCommand& sample = *new SampleCommand(document());
  Node* insertChild = document().createTextNode("foo");
  Element* refChild = document().querySelector("b");
  Element* div = document().querySelector("div");

  EditingState editingState;
  sample.insertNodeBefore(insertChild, refChild, &editingState);
  EXPECT_FALSE(editingState.isAborted());
  EXPECT_EQ("foo<b></b>", div->innerHTML());
}

TEST_F(CompositeEditCommandTest, insertNodeBeforeInUneditable) {
  setBodyContent("<div><b></b></div>");
  SampleCommand& sample = *new SampleCommand(document());
  Node* insertChild = document().createTextNode("foo");
  Element* refChild = document().querySelector("b");

  EditingState editingState;
  sample.insertNodeBefore(insertChild, refChild, &editingState);
  EXPECT_TRUE(editingState.isAborted());
}

TEST_F(CompositeEditCommandTest, insertNodeBeforeDisconnectedNode) {
  setBodyContent("<div><b></b></div>");
  SampleCommand& sample = *new SampleCommand(document());
  Node* insertChild = document().createTextNode("foo");
  Element* refChild = document().querySelector("b");
  Element* div = document().querySelector("div");
  div->remove();

  EditingState editingState;
  sample.insertNodeBefore(insertChild, refChild, &editingState);
  EXPECT_FALSE(editingState.isAborted());
  EXPECT_EQ("<b></b>", div->innerHTML())
      << "InsertNodeBeforeCommand does nothing for disconnected node";
}

TEST_F(CompositeEditCommandTest, insertNodeBeforeWithDirtyLayoutTree) {
  setBodyContent("<div><b></b></div>");
  SampleCommand& sample = *new SampleCommand(document());
  Node* insertChild = document().createTextNode("foo");
  Element* refChild = document().querySelector("b");
  Element* div = document().querySelector("div");
  div->setAttribute(HTMLNames::contenteditableAttr, "true");

  EditingState editingState;
  sample.insertNodeBefore(insertChild, refChild, &editingState);
  EXPECT_FALSE(editingState.isAborted());
  EXPECT_EQ("foo<b></b>", div->innerHTML());
}

}  // namespace blink
