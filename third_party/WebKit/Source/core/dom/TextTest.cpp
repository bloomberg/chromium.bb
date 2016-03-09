// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Text.h"

#include "core/dom/Range.h"
#include "core/editing/EditingTestBase.h"
#include "core/html/HTMLPreElement.h"
#include "core/layout/LayoutText.h"

namespace blink {

// TODO(xiaochengh): Use a new testing base class.
class TextTest : public EditingTestBase {
};

TEST_F(TextTest, RemoveFirstLetterPseudoElementWhenNoLetter)
{
    document().documentElement()->setInnerHTML("<style>*::first-letter{font:icon;}</style><pre>AB\n</pre>", ASSERT_NO_EXCEPTION);
    updateLayoutAndStyleForPainting();

    RefPtrWillBeRawPtr<Element> pre = document().querySelector("pre", ASSERT_NO_EXCEPTION);
    Text* text = toText(pre->firstChild());

    RefPtrWillBeRawPtr<Range> range = Range::create(document(), text, 0, text, 2);
    range->deleteContents(ASSERT_NO_EXCEPTION);
    updateLayoutAndStyleForPainting();

    EXPECT_FALSE(text->layoutObject()->isTextFragment());
}

} // namespace blink
