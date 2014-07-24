// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "platform/graphics/RecordingImageBufferSurface.h"

#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace blink;
using testing::Test;

class RecordingImageBufferSurfaceTest : public Test {
protected:
    RecordingImageBufferSurfaceTest()
    {
        m_testSurface = adoptPtr(new RecordingImageBufferSurface(IntSize(10, 10)));
    }

    void testEmptyPicture()
    {
        m_testSurface->initializeCurrentFrame();
        RefPtr<SkPicture> picture = m_testSurface->getPicture();
        EXPECT_TRUE((bool)picture.get());
        expectDisplayListEnabled(true);
    }

    void testNoFallbackWithClear()
    {
        m_testSurface->initializeCurrentFrame();
        m_testSurface->didClearCanvas();
        m_testSurface->getPicture();
        expectDisplayListEnabled(true);
    }
private:
    void expectDisplayListEnabled(bool displayListEnabled)
    {
        EXPECT_EQ(displayListEnabled, (bool)m_testSurface->m_currentFrame.get());
        EXPECT_EQ(!displayListEnabled, (bool)m_testSurface->m_rasterCanvas.get());
    }

    OwnPtr<RecordingImageBufferSurface> m_testSurface;
};

namespace {

TEST_F(RecordingImageBufferSurfaceTest, testEmptyPicture)
{
    testEmptyPicture();
}

TEST_F(RecordingImageBufferSurfaceTest, testNoFallbackWithClear)
{
    testNoFallbackWithClear();
}


} // namespace
