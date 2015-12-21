// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLLinkElement.h"

#include "core/HTMLNames.h"
#include "core/dom/DOMSettableTokenList.h"
#include "core/dom/Document.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class HTMLLinkElementSizesAttributeTest : public testing::Test {
};

TEST(HTMLLinkElementSizesAttributeTest, parseSizes)
{
    AtomicString sizesAttribute = "32x33";
    Vector<IntSize> sizes;
    HTMLLinkElement::parseSizesAttribute(sizesAttribute, sizes);
    ASSERT_EQ(1U, sizes.size());
    EXPECT_EQ(32, sizes[0].width());
    EXPECT_EQ(33, sizes[0].height());

    UChar attribute[] = {'3', '2', 'x', '3', '3', 0};
    sizesAttribute = attribute;
    sizes.clear();
    HTMLLinkElement::parseSizesAttribute(sizesAttribute, sizes);
    ASSERT_EQ(1U, sizes.size());
    EXPECT_EQ(32, sizes[0].width());
    EXPECT_EQ(33, sizes[0].height());


    sizesAttribute = "   32x33   16X17    128x129   ";
    sizes.clear();
    HTMLLinkElement::parseSizesAttribute(sizesAttribute, sizes);
    ASSERT_EQ(3U, sizes.size());
    EXPECT_EQ(32, sizes[0].width());
    EXPECT_EQ(33, sizes[0].height());
    EXPECT_EQ(16, sizes[1].width());
    EXPECT_EQ(17, sizes[1].height());
    EXPECT_EQ(128, sizes[2].width());
    EXPECT_EQ(129, sizes[2].height());

    sizesAttribute = "any";
    sizes.clear();
    HTMLLinkElement::parseSizesAttribute(sizesAttribute, sizes);
    ASSERT_EQ(0U, sizes.size());

    sizesAttribute = "32x33 32";
    sizes.clear();
    HTMLLinkElement::parseSizesAttribute(sizesAttribute, sizes);
    ASSERT_EQ(0U, sizes.size());

    sizesAttribute = "32x33 32x";
    sizes.clear();
    HTMLLinkElement::parseSizesAttribute(sizesAttribute, sizes);
    ASSERT_EQ(0U, sizes.size());

    sizesAttribute = "32x33 x32";
    sizes.clear();
    HTMLLinkElement::parseSizesAttribute(sizesAttribute, sizes);
    ASSERT_EQ(0U, sizes.size());

    sizesAttribute = "32x33 any";
    sizes.clear();
    HTMLLinkElement::parseSizesAttribute(sizesAttribute, sizes);
    ASSERT_EQ(0U, sizes.size());

    sizesAttribute = "32x33, 64x64";
    sizes.clear();
    HTMLLinkElement::parseSizesAttribute(sizesAttribute, sizes);
    ASSERT_EQ(0U, sizes.size());
}

TEST(HTMLLinkElementSizesAttributeTest, setSizesPropertyValue_updatesAttribute)
{
    RefPtrWillBeRawPtr<Document> document = Document::create();
    RefPtrWillBeRawPtr<HTMLLinkElement> link = HTMLLinkElement::create(*document, /* createdByParser: */ false);
    RefPtrWillBeRawPtr<DOMSettableTokenList> sizes = link->sizes();
    EXPECT_EQ(nullAtom, sizes->value());
    sizes->setValue("   a b  c ");
    EXPECT_EQ("   a b  c ", link->getAttribute(HTMLNames::sizesAttr));
    EXPECT_EQ("   a b  c ", sizes->value());
}

TEST(HTMLLinkElementSizesAttributeTest, setSizesAttribute_updatesSizesPropertyValue)
{
    RefPtrWillBeRawPtr<Document> document = Document::create();
    RefPtrWillBeRawPtr<HTMLLinkElement> link = HTMLLinkElement::create(*document, /* createdByParser: */ false);
    RefPtrWillBeRawPtr<DOMSettableTokenList> sizes = link->sizes();
    EXPECT_EQ(nullAtom, sizes->value());
    link->setAttribute(HTMLNames::sizesAttr, "y  x ");
    EXPECT_EQ("y  x ", sizes->value());
}

} // namespace blink
