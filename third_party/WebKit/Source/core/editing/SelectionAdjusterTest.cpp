// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/SelectionAdjuster.h"

#include "core/editing/EditingTestBase.h"

namespace blink {

namespace {

class MockVisibleSelectionChangeObserver final :
    public NoBaseWillBeGarbageCollectedFinalized<MockVisibleSelectionChangeObserver>,
    public VisibleSelectionChangeObserver {
    WTF_MAKE_NONCOPYABLE(MockVisibleSelectionChangeObserver);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(MockVisibleSelectionChangeObserver);
public:
    MockVisibleSelectionChangeObserver() = default;
    ~MockVisibleSelectionChangeObserver() final = default;

    int callCounter() const { return m_callCounter; }

private:
    // VisibleSelectionChangeObserver interface.
    void didChangeVisibleSelection() final { ++m_callCounter; }

    int m_callCounter = 0;
};

} // namespace

class SelectionAdjusterTest : public EditingTestBase  {
};

TEST_F(SelectionAdjusterTest, adjustSelectionInComposedTree)
{
    setBodyContent("<div id=sample>foo</div>");
    MockVisibleSelectionChangeObserver selectionObserver;
    VisibleSelectionInComposedTree selectionInComposedTree;
    selectionInComposedTree.setChangeObserver(selectionObserver);

    Node* const sample = document().getElementById("sample");
    Node* const foo = sample->firstChild();
    // Select "foo"
    VisibleSelection selection(Position(foo, 0), Position(foo, 3));
    SelectionAdjuster::adjustSelectionInComposedTree(&selectionInComposedTree, selection);
    EXPECT_EQ(PositionInComposedTree(foo, 0), selectionInComposedTree.start());
    EXPECT_EQ(PositionInComposedTree(foo, 3), selectionInComposedTree.end());
    EXPECT_EQ(1, selectionObserver.callCounter()) << "adjustSelectionInComposedTree() should call didChangeVisibleSelection()";
}

TEST_F(SelectionAdjusterTest, adjustSelectionInDOMTree)
{
    setBodyContent("<div id=sample>foo</div>");
    MockVisibleSelectionChangeObserver selectionObserver;
    VisibleSelection selection;
    selection.setChangeObserver(selectionObserver);

    Node* const sample = document().getElementById("sample");
    Node* const foo = sample->firstChild();
    // Select "foo"
    VisibleSelectionInComposedTree selectionInComposedTree(
        PositionInComposedTree(foo, 0),
        PositionInComposedTree(foo, 3));
    SelectionAdjuster::adjustSelectionInDOMTree(&selection, selectionInComposedTree);
    EXPECT_EQ(Position(foo, 0), selection.start());
    EXPECT_EQ(Position(foo, 3), selection.end());
    EXPECT_EQ(1, selectionObserver.callCounter()) << "adjustSelectionInDOMTree() should call didChangeVisibleSelection()";
}

} // namespace blink
