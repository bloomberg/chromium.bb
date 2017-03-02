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
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree::beforeNode(input),
            flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree(document().body(), 1),
            flatIterator.computePosition());
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree(text, 3), flatIterator.computePosition());
}

TEST_F(PositionIteratorTest, decrementWithSelectElement) {
  setBodyContent("123<select><option>1</option><option>2</option></select>");
  Element* const select = document().querySelector("select");
  Node* text = select->previousSibling();

  // Decrement until start of "123" from SELECT on DOM tree
  PositionIterator domIterator(Position::lastPositionInNode(document().body()));
  EXPECT_EQ(Position::lastPositionInNode(document().body()),
            domIterator.computePosition());
  domIterator.decrement();
  EXPECT_EQ(Position::afterNode(select), domIterator.computePosition());
  domIterator.decrement();
  EXPECT_EQ(Position::afterNode(select), domIterator.computePosition())
      << "This is redundant result, we should not have. see "
         "http://crbug.com/697283";
  domIterator.decrement();
  EXPECT_EQ(Position::beforeNode(select), domIterator.computePosition());
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
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree::afterNode(select),
            flatIterator.computePosition())
      << "This is redundant result, we should not have. see "
         "http://crbug.com/697283";
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree::beforeNode(select),
            flatIterator.computePosition());
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
  Node* const text = textarea->previousSibling();

  // Decrement until end of "123" from after TEXTAREA on DOM tree
  PositionIterator domIterator(Position::lastPositionInNode(document().body()));
  EXPECT_EQ(Position::lastPositionInNode(document().body()),
            domIterator.computePosition());
  domIterator.decrement();
  EXPECT_EQ(Position::afterNode(textarea), domIterator.computePosition());
  domIterator.decrement();
  EXPECT_EQ(Position::beforeNode(textarea), domIterator.computePosition());
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
  flatIterator.decrement();
  EXPECT_EQ(PositionInFlatTree::beforeNode(textarea),
            flatIterator.computePosition());
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
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree::beforeNode(input),
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
  Node* const text = select->nextSibling();

  // Increment until start of "123" from SELECT on DOM tree
  PositionIterator domIterator(
      Position::firstPositionInNode(document().body()));
  EXPECT_EQ(Position(document().body(), 0), domIterator.computePosition());
  domIterator.increment();
  EXPECT_EQ(Position::beforeNode(select), domIterator.computePosition());
  domIterator.increment();
  EXPECT_EQ(Position::afterNode(select), domIterator.computePosition());
  domIterator.increment();
  EXPECT_EQ(Position::afterNode(select), domIterator.computePosition())
      << "This is redundant result, we should not have. see "
         "http://crbug.com/697283";
  domIterator.increment();
  EXPECT_EQ(Position(document().body(), 1), domIterator.computePosition());
  domIterator.increment();
  EXPECT_EQ(Position(text, 0), domIterator.computePosition());

  // Increment until start of "123" from SELECT on flat tree
  PositionIteratorInFlatTree flatIterator(
      PositionInFlatTree::firstPositionInNode(document().body()));
  EXPECT_EQ(PositionInFlatTree(document().body(), 0),
            flatIterator.computePosition());
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree::beforeNode(select),
            flatIterator.computePosition());
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree::afterNode(select),
            flatIterator.computePosition());
  flatIterator.increment();
  EXPECT_EQ(PositionInFlatTree::afterNode(select),
            flatIterator.computePosition())
      << "This is redundant result, we should not have. see "
         "http://crbug.com/697283";
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

  // Increment until start of "123" from TEXTAREA on DOM tree
  PositionIterator domIterator(
      Position::firstPositionInNode(document().body()));
  EXPECT_EQ(Position(document().body(), 0), domIterator.computePosition());
  domIterator.increment();
  EXPECT_EQ(Position::beforeNode(textarea), domIterator.computePosition());
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
  EXPECT_EQ(PositionInFlatTree::beforeNode(textarea),
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
