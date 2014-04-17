// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/fonts/GlyphPageTreeNode.h"

#include "platform/fonts/SegmentedFontData.h"
#include "platform/fonts/SimpleFontData.h"
#include <gtest/gtest.h>

namespace WebCore {

class TestSimpleFontData : public SimpleFontData {
public:
    static PassRefPtr<TestSimpleFontData> create(UChar32 from, UChar32 to)
    {
        return adoptRef(new TestSimpleFontData(from, to));
    }

private:
    TestSimpleFontData(UChar32 from, UChar32 to)
        : SimpleFontData(nullptr, 10, false, false)
        , m_from(from)
        , m_to(to)
    {
    }

    bool fillGlyphPage(GlyphPage* pageToFill, unsigned offset, unsigned length, UChar* buffer, unsigned bufferLength) const OVERRIDE
    {
        const Glyph kGlyph = 1;
        String bufferString(buffer, bufferLength);
        unsigned bufferIndex = 0;
        bool hasGlyphs = false;
        for (unsigned i = 0; i < length; i++) {
            UChar32 c = bufferString.characterStartingAt(bufferIndex);
            bufferIndex += U16_LENGTH(c);
            if (m_from <= c && c <= m_to) {
                pageToFill->setGlyphDataForIndex(offset + i, kGlyph, this);
                hasGlyphs = true;
            }
        }
        return hasGlyphs;
    }

    UChar32 m_from;
    UChar32 m_to;
};

TEST(GlyphPageTreeNode, rootChild)
{
    const unsigned kPageNumber = 0;
    size_t pageCountBeforeTest = GlyphPageTreeNode::treeGlyphPageCount();

    RefPtr<TestSimpleFontData> data = TestSimpleFontData::create('a', 'z');
    GlyphPageTreeNode* node = GlyphPageTreeNode::getRootChild(data.get(), kPageNumber);
    EXPECT_EQ(pageCountBeforeTest + 1, GlyphPageTreeNode::treeGlyphPageCount());
    EXPECT_TRUE(node->page()->glyphAt('a'));
    EXPECT_FALSE(node->page()->glyphAt('A'));
    EXPECT_FALSE(node->isSystemFallback());
    EXPECT_EQ(1u, node->level());
    EXPECT_EQ(node, node->page()->owner());

    GlyphPageTreeNode::pruneTreeFontData(data.get());
    EXPECT_EQ(pageCountBeforeTest, GlyphPageTreeNode::treeGlyphPageCount());
}

TEST(GlyphPageTreeNode, level2)
{
    const unsigned kPageNumber = 0;
    size_t pageCountBeforeTest = GlyphPageTreeNode::treeGlyphPageCount();

    RefPtr<TestSimpleFontData> dataAtoC = TestSimpleFontData::create('A', 'C');
    RefPtr<TestSimpleFontData> dataCtoE = TestSimpleFontData::create('C', 'E');
    GlyphPageTreeNode* node1 = GlyphPageTreeNode::getRootChild(dataAtoC.get(), kPageNumber);
    GlyphPageTreeNode* node2 = node1->getChild(dataCtoE.get(), kPageNumber);
    EXPECT_EQ(pageCountBeforeTest + 3, GlyphPageTreeNode::treeGlyphPageCount());

    EXPECT_EQ(2u, node2->level());
    EXPECT_EQ(dataAtoC, node2->page()->fontDataForCharacter('A'));
    EXPECT_EQ(dataAtoC, node2->page()->fontDataForCharacter('C'));
    EXPECT_EQ(dataCtoE, node2->page()->fontDataForCharacter('E'));

    GlyphPageTreeNode::pruneTreeFontData(dataAtoC.get());
    GlyphPageTreeNode::pruneTreeFontData(dataCtoE.get());
    EXPECT_EQ(pageCountBeforeTest, GlyphPageTreeNode::treeGlyphPageCount());
}

TEST(GlyphPageTreeNode, segmentedData)
{
    const unsigned kPageNumber = 0;
    size_t pageCountBeforeTest = GlyphPageTreeNode::treeGlyphPageCount();

    RefPtr<TestSimpleFontData> dataBtoC = TestSimpleFontData::create('B', 'C');
    RefPtr<TestSimpleFontData> dataCtoE = TestSimpleFontData::create('C', 'E');
    RefPtr<SegmentedFontData> segmentedData = SegmentedFontData::create();
    segmentedData->appendRange(FontDataRange('A', 'C', dataBtoC));
    segmentedData->appendRange(FontDataRange('C', 'D', dataCtoE));
    GlyphPageTreeNode* node = GlyphPageTreeNode::getRootChild(segmentedData.get(), kPageNumber);

    EXPECT_EQ(0, node->page()->fontDataForCharacter('A'));
    EXPECT_EQ(dataBtoC, node->page()->fontDataForCharacter('B'));
    EXPECT_EQ(dataBtoC, node->page()->fontDataForCharacter('C'));
    EXPECT_EQ(dataCtoE, node->page()->fontDataForCharacter('D'));
    EXPECT_EQ(0, node->page()->fontDataForCharacter('E'));

    GlyphPageTreeNode::pruneTreeCustomFontData(segmentedData.get());
    EXPECT_EQ(pageCountBeforeTest, GlyphPageTreeNode::treeGlyphPageCount());
}

TEST(GlyphPageTreeNode, outsideBMP)
{
    const unsigned kPageNumber = 0x1f300 / GlyphPage::size;
    size_t pageCountBeforeTest = GlyphPageTreeNode::treeGlyphPageCount();

    RefPtr<TestSimpleFontData> data = TestSimpleFontData::create(0x1f310, 0x1f320);
    GlyphPageTreeNode* node = GlyphPageTreeNode::getRootChild(data.get(), kPageNumber);
    EXPECT_EQ(pageCountBeforeTest + 1, GlyphPageTreeNode::treeGlyphPageCount());
    EXPECT_FALSE(node->page()->glyphForCharacter(0x1f30f));
    EXPECT_TRUE(node->page()->glyphForCharacter(0x1f310));
    EXPECT_TRUE(node->page()->glyphForCharacter(0x1f320));
    EXPECT_FALSE(node->page()->glyphForCharacter(0x1f321));

    GlyphPageTreeNode::pruneTreeFontData(data.get());
    EXPECT_EQ(pageCountBeforeTest, GlyphPageTreeNode::treeGlyphPageCount());
}

} // namespace WebCore
