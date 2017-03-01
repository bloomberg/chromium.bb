// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/PositionIterator.h"

#include "core/dom/shadow/FlatTreeTraversal.h"
#include "core/editing/EditingTestBase.h"

namespace blink {

class PositionIteratorTest : public EditingTestBase {};

// For http://crbug.com/695317
TEST_F(PositionIteratorTest, decrementWithInputElement) {
  setBodyContent("123<input value='abc'>");
  Element* const input = document().querySelector("input");
  Node* const innerEditor = FlatTreeTraversal::firstChild(*input);
  Node* const text = input->previousSibling();

  // Decrement until start of "123" from INPUT on DOM tree
  PositionIterator domIterator(Position::lastPositionInNode(document().body()));
  EXPECT_EQ(Position::lastPositionInNode(document().body()),
            domIterator.computePosition());
  domIterator.decrement();
  EXPECT_EQ(Position::afterNode(input), domIterator.computePosition());
  domIterator.decrement();
  EXPECT_EQ(Position::beforeNode(input), domIterator.computePosition());
  domIterator.decrement();
  EXPECT_EQ(Position(document().body(), 1), domIterator.computePosition());
  domIterator.decrement();
  EXPECT_EQ(Position(text, 3), domIterator.computePosition());

  // Decrement until start of "123" from INPUT on flat tree
  PositionIteratorInFlatTree flatIterator(
      PositionInFlatTree::lastPositionInNode(document().body()));
  EXPECT_EQ(PositionInFlatTree::lastPositionInNode(document().body()),
            flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree::afterNode(input),
            flatIterator.computePosition());
  // TODO(yosin): We should not traverse inside INPUT
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree::lastPositionInNode(innerEditor),
            flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree(innerEditor->firstChild(), 3),
            flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree(innerEditor->firstChild(), 2),
            flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree(innerEditor->firstChild(), 1),
            flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree(innerEditor->firstChild(), 0),
            flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree(innerEditor, 0), flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree(input, 0), flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree(document().body(), 1),
            flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree(text, 3), flatIterator.computePosition());
}

TEST_F(PositionIteratorTest, decrementWithSelectElement) {
  setBodyContent("123<select><option>1</option><option>2</option></select>");
  Element* const select = document().querySelector("select");
  Node* const option1 = select->firstChild();
  Node* const option2 = select->lastChild();
  Node* text = select->previousSibling();

  // Decrement until start of "123" from SELECT on DOM tree
  PositionIterator domIterator(Position::lastPositionInNode(document().body()));
  EXPECT_EQ(Position::lastPositionInNode(document().body()),
            domIterator.computePosition());
  domIterator.decrement();
  EXPECT_EQ(Position::afterNode(select), domIterator.computePosition());
  // TODO(yosin): We should not traverse inside SELECT and OPTION
  domIterator.decrement();
  EXPECT_EQ(Position::lastPositionInNode(option2),
            domIterator.computePosition());
  domIterator.decrement();
  EXPECT_EQ(Position(option2->firstChild(), 1), domIterator.computePosition());
  domIterator.decrement();
  EXPECT_EQ(Position(option2, 0), domIterator.computePosition());
  domIterator.decrement();
  EXPECT_EQ(Position(select, 1), domIterator.computePosition());
  domIterator.decrement();
  EXPECT_EQ(Position::lastPositionInNode(option1),
            domIterator.computePosition());
  domIterator.decrement();
  EXPECT_EQ(Position(option1->firstChild(), 1), domIterator.computePosition());
  domIterator.decrement();
  EXPECT_EQ(Position(option1, 0), domIterator.computePosition());
  domIterator.decrement();
  EXPECT_EQ(Position(select, 0), domIterator.computePosition());
  domIterator.decrement();
  EXPECT_EQ(Position(document().body(), 1), domIterator.computePosition());
  domIterator.decrement();
  EXPECT_EQ(Position(text, 3), domIterator.computePosition());

  // Decrement until start of "123" from SELECT on flat tree
  PositionIteratorInFlatTree flatIterator(
      PositionInFlatTree::lastPositionInNode(document().body()));
  EXPECT_EQ(PositionInFlatTree::lastPositionInNode(document().body()),
            flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree::afterNode(select),
            flatIterator.computePosition());
  // TODO(yosin): We should not traverse inside SELECT and OPTION
  // Traverse |option2|
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree::lastPositionInNode(option2),
            flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree(FlatTreeTraversal::firstChild(*option2), 1),
            flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree(option2, 0), flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree(select, 1), flatIterator.computePosition());
  // Traverse |option1|
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree::lastPositionInNode(option1),
            flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree(FlatTreeTraversal::firstChild(*option1), 1),
            flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree(option1, 0), flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree(select, 0), flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree(document().body(), 1),
            flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree(text, 3), flatIterator.computePosition());
}

// For http://crbug.com/695317
TEST_F(PositionIteratorTest, decrementWithTextAreaElement) {
  setBodyContent("123<textarea>456</textarea>");
  Element* const textarea = document().querySelector("textarea");
  Node* const innerEditor = FlatTreeTraversal::firstChild(*textarea);
  Node* const text = textarea->previousSibling();

  // Decrement until end of "123" from after TEXTAREA on DOM tree
  PositionIterator domIterator(Position::lastPositionInNode(document().body()));
  EXPECT_EQ(Position::lastPositionInNode(document().body()),
            domIterator.computePosition());
  domIterator.decrement();
  EXPECT_EQ(Position::afterNode(textarea), domIterator.computePosition());
  // TODO(yosin): We should not traverse inside TEXTAREA
  domIterator.decrement();
  EXPECT_EQ(Position(textarea->firstChild(), 3), domIterator.computePosition());
  domIterator.decrement();
  EXPECT_EQ(Position(textarea, 0), domIterator.computePosition());
  domIterator.decrement();
  EXPECT_EQ(Position(document().body(), 1), domIterator.computePosition());
  domIterator.decrement();
  EXPECT_EQ(Position(text, 3), domIterator.computePosition());

  // Decrement until end of "123" from after TEXTAREA on flat tree
  PositionIteratorInFlatTree flatIterator(
      PositionInFlatTree::lastPositionInNode(document().body()));
  EXPECT_EQ(PositionInFlatTree::lastPositionInNode(document().body()),
            flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree::afterNode(textarea),
            flatIterator.computePosition());
  // TODO(yosin): We should not traverse inside TEXTAREA
  // Traverse |innerEditor|
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree::lastPositionInNode(innerEditor),
            flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree(innerEditor->firstChild(), 3),
            flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree(innerEditor->firstChild(), 2),
            flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree(innerEditor->firstChild(), 1),
            flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree(innerEditor->firstChild(), 0),
            flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree(innerEditor, 0), flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree(textarea, 0), flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree(document().body(), 1),
            flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree(text, 3), flatIterator.computePosition());
}

// For http://crbug.com/695317
TEST_F(PositionIteratorTest, incrementWithInputElement) {
  setBodyContent("<input value='abc'>123");
  Element* const input = document().querySelector("input");
  Node* const innerEditor = FlatTreeTraversal::firstChild(*input);
  Node* const text = input->nextSibling();

  // Increment until start of "123" from INPUT on DOM tree
  PositionIterator domIterator(
      Position::firstPositionInNode(document().body()));
  EXPECT_EQ(Position(document().body(), 0), domIterator.computePosition());
  domIterator.increment();
  EXPECT_EQ(Position::beforeNode(input), domIterator.computePosition());
  domIterator.increment();
  EXPECT_EQ(Position::afterNode(input), domIterator.computePosition());
  domIterator.increment();
  EXPECT_EQ(Position(document().body(), 1), domIterator.computePosition());
  domIterator.increment();
  EXPECT_EQ(Position(text, 0), domIterator.computePosition());

  // Increment until start of "123" from INPUT on flat tree
  PositionIteratorInFlatTree flatIterator(
      PositionInFlatTree::firstPositionInNode(document().body()));
  EXPECT_EQ(PositionInFlatTree(document().body(), 0),
            flatIterator.computePosition());
  // TODO(yosin): We should not traverse inside INPUT
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree(input, 0), flatIterator.computePosition());
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree(innerEditor, 0), flatIterator.computePosition());
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree(innerEditor->firstChild(), 0),
            flatIterator.computePosition());
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree(innerEditor->firstChild(), 1),
            flatIterator.computePosition());
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree(innerEditor->firstChild(), 2),
            flatIterator.computePosition());
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree(innerEditor->firstChild(), 3),
            flatIterator.computePosition());
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree::lastPositionInNode(innerEditor),
            flatIterator.computePosition());
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree::afterNode(input),
            flatIterator.computePosition());
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree(document().body(), 1),
            flatIterator.computePosition());
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree(text, 0), flatIterator.computePosition());
}

TEST_F(PositionIteratorTest, incrementWithSelectElement) {
  setBodyContent("<select><option>1</option><option>2</option></select>123");
  Element* const select = document().querySelector("select");
  Node* const option1 = select->firstChild();
  Node* const option2 = select->lastChild();
  Node* const text = select->nextSibling();

  // Increment until start of "123" from SELECT on DOM tree
  PositionIterator domIterator(
      Position::firstPositionInNode(document().body()));
  EXPECT_EQ(Position(document().body(), 0), domIterator.computePosition());
  // TODO(yosin): We should not traverse inside SELECT
  domIterator.increment();
  EXPECT_EQ(Position(select, 0), domIterator.computePosition());
  domIterator.increment();
  EXPECT_EQ(Position(option1, 0), domIterator.computePosition());
  domIterator.increment();
  EXPECT_EQ(Position(option1->firstChild(), 0), domIterator.computePosition());
  domIterator.increment();
  EXPECT_EQ(Position::lastPositionInNode(option1),
            domIterator.computePosition());
  domIterator.increment();
  EXPECT_EQ(Position(select, 1), domIterator.computePosition());
  domIterator.increment();
  EXPECT_EQ(Position(option2, 0), domIterator.computePosition());
  domIterator.increment();
  EXPECT_EQ(Position(option2->firstChild(), 0), domIterator.computePosition());
  domIterator.increment();
  EXPECT_EQ(Position::lastPositionInNode(option2),
            domIterator.computePosition());
  domIterator.increment();
  EXPECT_EQ(Position::afterNode(select), domIterator.computePosition());
  domIterator.increment();
  EXPECT_EQ(Position(document().body(), 1), domIterator.computePosition());
  domIterator.increment();
  EXPECT_EQ(Position(text, 0), domIterator.computePosition());

  // Increment until start of "123" from SELECT on flat tree
  PositionIteratorInFlatTree flatIterator(
      PositionInFlatTree::firstPositionInNode(document().body()));
  EXPECT_EQ(PositionInFlatTree(document().body(), 0),
            flatIterator.computePosition());
  // TODO(yosin): We should not traverse inside SELECT
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree(select, 0), flatIterator.computePosition());
  flatIterator.increment();
  // Traverse |option2|
  EXPECT_EQ(PositionInFlatTree(option1, 0), flatIterator.computePosition());
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree(FlatTreeTraversal::firstChild(*option1), 0),
            flatIterator.computePosition());
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree::lastPositionInNode(option1),
            flatIterator.computePosition());
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree(select, 1), flatIterator.computePosition());
  // Traverse |option2|
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree(option2, 0), flatIterator.computePosition());
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree(FlatTreeTraversal::firstChild(*option2), 0),
            flatIterator.computePosition());
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree::lastPositionInNode(option2),
            flatIterator.computePosition());
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree::afterNode(select),
            flatIterator.computePosition());
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree(document().body(), 1),
            flatIterator.computePosition());
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree(text, 0), flatIterator.computePosition());
}

// For http://crbug.com/695317
TEST_F(PositionIteratorTest, incrementWithTextAreaElement) {
  setBodyContent("<textarea>123</textarea>456");
  Element* const textarea = document().querySelector("textarea");
  Node* const text = textarea->nextSibling();
  Node* const innerEditor = FlatTreeTraversal::firstChild(*textarea);

  // Increment until start of "123" from TEXTAREA on DOM tree
  PositionIterator domIterator(
      Position::firstPositionInNode(document().body()));
  EXPECT_EQ(Position(document().body(), 0), domIterator.computePosition());
  domIterator.increment();
  EXPECT_EQ(Position(textarea, 0), domIterator.computePosition());
  domIterator.increment();
  EXPECT_EQ(Position(textarea->firstChild(), 0), domIterator.computePosition());
  domIterator.increment();
  EXPECT_EQ(Position::afterNode(textarea), domIterator.computePosition());
  domIterator.increment();
  EXPECT_EQ(Position(document().body(), 1), domIterator.computePosition());
  domIterator.increment();
  EXPECT_EQ(Position(text, 0), domIterator.computePosition());

  // Increment until start of "123" from TEXTAREA on flat tree
  PositionIteratorInFlatTree flatIterator(
      PositionInFlatTree::firstPositionInNode(document().body()));
  EXPECT_EQ(PositionInFlatTree(document().body(), 0),
            flatIterator.computePosition());
  // TODO(yosin): We should not traverse inside TEXTAREA
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree(textarea, 0), flatIterator.computePosition());
  // Traverse |innerEditor|
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree(innerEditor, 0), flatIterator.computePosition());
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree(innerEditor->firstChild(), 0),
            flatIterator.computePosition());
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree(innerEditor->firstChild(), 1),
            flatIterator.computePosition());
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree(innerEditor->firstChild(), 2),
            flatIterator.computePosition());
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree(innerEditor->firstChild(), 3),
            flatIterator.computePosition());
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree::lastPositionInNode(innerEditor),
            flatIterator.computePosition());
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree::afterNode(textarea),
            flatIterator.computePosition());
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree(document().body(), 1),
            flatIterator.computePosition());
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree(text, 0), flatIterator.computePosition());
}

}  // namespace blink
