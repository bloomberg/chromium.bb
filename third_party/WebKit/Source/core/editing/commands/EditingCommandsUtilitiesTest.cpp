// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/commands/EditingCommandsUtilities.h"

#include "core/dom/StaticNodeList.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/testing/EditingTestBase.h"

namespace blink {

class EditingCommandsUtilitiesTest : public EditingTestBase {};

TEST_F(EditingCommandsUtilitiesTest, AreaIdenticalElements) {
  SetBodyContent(
      "<style>li:nth-child(even) { -webkit-user-modify: read-write; "
      "}</style><ul><li>first item</li><li>second item</li><li "
      "class=foo>third</li><li>fourth</li></ul>");
  StaticElementList* items =
      GetDocument().QuerySelectorAll("li", ASSERT_NO_EXCEPTION);
  DCHECK_EQ(items->length(), 4u);

  EXPECT_FALSE(AreIdenticalElements(*items->item(0)->firstChild(),
                                    *items->item(1)->firstChild()))
      << "Can't merge non-elements.  e.g. Text nodes";

  // Compare a LI and a UL.
  EXPECT_FALSE(
      AreIdenticalElements(*items->item(0), *items->item(0)->parentNode()))
      << "Can't merge different tag names.";

  EXPECT_FALSE(AreIdenticalElements(*items->item(0), *items->item(2)))
      << "Can't merge a element with no attributes and another element with an "
         "attribute.";

  // We can't use contenteditable attribute to make editability difference
  // because the hasEquivalentAttributes check is done earier.
  EXPECT_FALSE(AreIdenticalElements(*items->item(0), *items->item(1)))
      << "Can't merge non-editable nodes.";

  EXPECT_TRUE(AreIdenticalElements(*items->item(1), *items->item(3)));
}

}  // namespace blink
