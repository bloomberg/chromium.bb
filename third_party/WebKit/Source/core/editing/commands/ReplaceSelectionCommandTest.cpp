// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/commands/ReplaceSelectionCommand.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/HTMLNames.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/ParserContentPolicy.h"
#include "core/editing/EditingTestBase.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/Position.h"
#include "core/editing/VisibleSelection.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLDocument.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <memory>

namespace blink {

class ReplaceSelectionCommandTest : public EditingTestBase {
};

// This is a regression test for https://crbug.com/121163
TEST_F(ReplaceSelectionCommandTest, styleTagsInPastedHeadIncludedInContent)
{
    document().setDesignMode("on");
    dummyPageHolder().frame().selection().setSelection(
        VisibleSelection(Position(document().body(), 0)));

    DocumentFragment* fragment = document().createDocumentFragment();
    fragment->parseHTML(
        "<head><style>foo { bar: baz; }</style></head>"
        "<body><p>Text</p></body>",
        document().documentElement(),
        DisallowScriptingAndPluginContent);

    ReplaceSelectionCommand::CommandOptions options = 0;
    ReplaceSelectionCommand* command =
        ReplaceSelectionCommand::create(document(), fragment, options);
    EXPECT_TRUE(command->apply())
        << "the replace command should have succeeded";

    EXPECT_EQ(
        "<head><style>foo { bar: baz; }</style></head>"
        "<body><p>Text</p></body>",
        document().body()->innerHTML())
        << "the STYLE and P elements should have been pasted into the body "
        << "of the document";
}

} // namespace blink
