// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_physical_text_fragment.h"

#include "core/layout/ng/inline/ng_inline_fragment_traversal.h"
#include "core/layout/ng/ng_layout_test.h"
#include "core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

class NGPhysicalTextFragmentTest : public NGLayoutTest {
 public:
  NGPhysicalTextFragmentTest() : NGLayoutTest() {}

 protected:
  Vector<scoped_refptr<const NGPhysicalTextFragment>>
  CollectTextFragmentsInContainer(const char* container_id) {
    const Element* container = GetElementById(container_id);
    DCHECK(container) << container_id;
    const LayoutObject* layout_object = container->GetLayoutObject();
    DCHECK(layout_object) << container;
    DCHECK(layout_object->IsLayoutBlockFlow()) << container;
    const NGPhysicalBoxFragment* root_fragment =
        ToLayoutBlockFlow(layout_object)->CurrentFragment();
    DCHECK(root_fragment) << container;

    Vector<scoped_refptr<const NGPhysicalTextFragment>> result;
    for (const auto& child :
         NGInlineFragmentTraversal::DescendantsOf(*root_fragment)) {
      if (child.fragment->IsText())
        result.push_back(ToNGPhysicalTextFragment(child.fragment.get()));
    }
    return result;
  }
};

TEST_F(NGPhysicalTextFragmentTest, NormalTextIsNotAnonymousText) {
  SetBodyInnerHTML("<div id=div>text</div>");

  auto text_fragments = CollectTextFragmentsInContainer("div");
  ASSERT_EQ(1u, text_fragments.size());

  const NGPhysicalTextFragment& text = *text_fragments[0];
  EXPECT_FALSE(text.IsAnonymousText());
}

TEST_F(NGPhysicalTextFragmentTest, FirstLetterIsNotAnonymousText) {
  SetBodyInnerHTML(
      "<style>::first-letter {color:red}</style>"
      "<div id=div>text</div>");

  auto text_fragments = CollectTextFragmentsInContainer("div");
  ASSERT_EQ(2u, text_fragments.size());

  const NGPhysicalTextFragment& first_letter = *text_fragments[0];
  const NGPhysicalTextFragment& remaining_text = *text_fragments[1];
  EXPECT_FALSE(first_letter.IsAnonymousText());
  EXPECT_FALSE(remaining_text.IsAnonymousText());
}

TEST_F(NGPhysicalTextFragmentTest, BeforeAndAfterAreAnonymousText) {
  SetBodyInnerHTML(
      "<style>::before{content:'x'} ::after{content:'x'}</style>"
      "<div id=div>text</div>");

  auto text_fragments = CollectTextFragmentsInContainer("div");
  ASSERT_EQ(3u, text_fragments.size());

  const NGPhysicalTextFragment& before = *text_fragments[0];
  const NGPhysicalTextFragment& text = *text_fragments[1];
  const NGPhysicalTextFragment& after = *text_fragments[2];
  EXPECT_TRUE(before.IsAnonymousText());
  EXPECT_FALSE(text.IsAnonymousText());
  EXPECT_TRUE(after.IsAnonymousText());
}

TEST_F(NGPhysicalTextFragmentTest, ListMarkerIsAnonymousText) {
  SetBodyInnerHTML(
      "<ol style='list-style-position:inside'>"
      "<li id=list>text</li>"
      "</ol>");

  auto text_fragments = CollectTextFragmentsInContainer("list");
  ASSERT_EQ(2u, text_fragments.size());

  const NGPhysicalTextFragment& marker = *text_fragments[0];
  const NGPhysicalTextFragment& text = *text_fragments[1];
  EXPECT_TRUE(marker.IsAnonymousText());
  EXPECT_FALSE(text.IsAnonymousText());
}

TEST_F(NGPhysicalTextFragmentTest, QuotationMarksAreAnonymousText) {
  SetBodyInnerHTML("<div id=div><q>text</q></div>");

  auto text_fragments = CollectTextFragmentsInContainer("div");
  ASSERT_EQ(3u, text_fragments.size());

  const NGPhysicalTextFragment& open_quote = *text_fragments[0];
  const NGPhysicalTextFragment& text = *text_fragments[1];
  const NGPhysicalTextFragment& closed_quote = *text_fragments[2];
  EXPECT_TRUE(open_quote.IsAnonymousText());
  EXPECT_FALSE(text.IsAnonymousText());
  EXPECT_TRUE(closed_quote.IsAnonymousText());
}

}  // namespace blink
