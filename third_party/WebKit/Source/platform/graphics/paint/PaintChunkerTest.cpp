// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/PaintChunker.h"

#include "platform/RuntimeEnabledFeatures.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using testing::ElementsAre;

namespace blink {
namespace {

static PaintProperties samplePaintProperties() { return PaintProperties(); }

class PaintChunkerTest : public testing::Test {
protected:
    void SetUp() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(true);
    }

    void TearDown() override
    {
        m_featuresBackup.restore();
    }

private:
    RuntimeEnabledFeatures::Backup m_featuresBackup;
};

TEST_F(PaintChunkerTest, Empty)
{
    Vector<PaintChunk> chunks = PaintChunker().releasePaintChunks();
    ASSERT_TRUE(chunks.isEmpty());
}

TEST_F(PaintChunkerTest, SingleNonEmptyRange)
{
    PaintChunker chunker;
    chunker.updateCurrentPaintProperties(samplePaintProperties());
    chunker.incrementDisplayItemIndex();
    chunker.incrementDisplayItemIndex();
    Vector<PaintChunk> chunks = chunker.releasePaintChunks();

    EXPECT_THAT(chunks, ElementsAre(
        PaintChunk(0, 2, samplePaintProperties())));
}

TEST_F(PaintChunkerTest, SamePropertiesTwiceCombineIntoOneChunk)
{
    PaintChunker chunker;
    chunker.updateCurrentPaintProperties(samplePaintProperties());
    chunker.incrementDisplayItemIndex();
    chunker.incrementDisplayItemIndex();
    chunker.updateCurrentPaintProperties(samplePaintProperties());
    chunker.incrementDisplayItemIndex();
    Vector<PaintChunk> chunks = chunker.releasePaintChunks();

    EXPECT_THAT(chunks, ElementsAre(
        PaintChunk(0, 3, samplePaintProperties())));
}

TEST_F(PaintChunkerTest, CanRewindDisplayItemIndex)
{
    PaintChunker chunker;
    chunker.updateCurrentPaintProperties(samplePaintProperties());
    chunker.incrementDisplayItemIndex();
    chunker.incrementDisplayItemIndex();
    chunker.decrementDisplayItemIndex();
    chunker.incrementDisplayItemIndex();
    Vector<PaintChunk> chunks = chunker.releasePaintChunks();

    EXPECT_THAT(chunks, ElementsAre(
        PaintChunk(0, 2, samplePaintProperties())));
}

// TODO(jbroman): Add more tests one it is possible for there to be two distinct
// PaintProperties.

} // namespace
} // namespace blink
