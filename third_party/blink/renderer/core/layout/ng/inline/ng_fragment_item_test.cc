// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

using testing::ElementsAre;

namespace blink {

class NGFragmentItemTest : public NGLayoutTest,
                           ScopedLayoutNGFragmentItemForTest {
 public:
  NGFragmentItemTest() : ScopedLayoutNGFragmentItemForTest(true) {}

  Vector<const NGFragmentItem*> ItemsForAsVector(
      const LayoutObject& layout_object) {
    const auto items = NGFragmentItem::ItemsFor(layout_object);
    Vector<const NGFragmentItem*> list;
    for (const NGFragmentItem& item : items) {
      EXPECT_EQ(item.GetLayoutObject(), &layout_object);
      list.push_back(&item);
    }
    return list;
  }
};

TEST_F(NGFragmentItemTest, BasicText) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0;
      font-family: Ahem;
      font-size: 10px;
      line-height: 1;
    }
    div {
      width: 10ch;
    }
    </style>
    <div id="container">
      1234567 98765
    </div>
  )HTML");

  LayoutBlockFlow* container =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  LayoutText* layout_text = ToLayoutText(container->FirstChild());
  const NGPhysicalBoxFragment* box = container->CurrentFragment();
  EXPECT_NE(box, nullptr);
  const NGFragmentItems* items = box->Items();
  EXPECT_NE(items, nullptr);
  EXPECT_EQ(items->Items().size(), 4u);

  const NGFragmentItem& text1 = *items->Items()[1];
  EXPECT_EQ(text1.Type(), NGFragmentItem::kText);
  EXPECT_EQ(text1.GetLayoutObject(), layout_text);
  EXPECT_EQ(text1.Offset(), PhysicalOffset());

  const NGFragmentItem& text2 = *items->Items()[3];
  EXPECT_EQ(text2.Type(), NGFragmentItem::kText);
  EXPECT_EQ(text2.GetLayoutObject(), layout_text);
  EXPECT_EQ(text2.Offset(), PhysicalOffset(0, 10));

  Vector<const NGFragmentItem*> items_for_text = ItemsForAsVector(*layout_text);
  EXPECT_THAT(items_for_text, ElementsAre(&text1, &text2));
}

TEST_F(NGFragmentItemTest, ForLayoutObject) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #container {
      font-family: monospace;
      width: 5ch;
    }
    #span {
      background: gray;
    }
    </style>
    <div id="container">
      0123
      <span id="span">1234 5678</span>
      6789
    </div>
  )HTML");

  const LayoutObject* span = GetLayoutObjectByElementId("span");
  ASSERT_NE(span, nullptr);
  Vector<const NGFragmentItem*> items_for_span = ItemsForAsVector(*span);
  EXPECT_EQ(items_for_span.size(), 2u);
}

}  // namespace blink
