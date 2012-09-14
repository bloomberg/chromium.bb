// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "ThrottledTextureUploader.h"

#include "Extensions3DChromium.h"
#include "FakeWebGraphicsContext3D.h"
#include "GraphicsContext3D.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <wtf/RefPtr.h>

using namespace cc;
using namespace WebKit;

namespace {

class FakeWebGraphicsContext3DWithQueryTesting : public FakeWebGraphicsContext3D {
public:
    FakeWebGraphicsContext3DWithQueryTesting() : m_resultAvailable(0)
    {
    }

    virtual void getQueryObjectuivEXT(WebGLId, GC3Denum type, GC3Duint* value)
    {
        switch (type) {
        case Extensions3DChromium::QUERY_RESULT_AVAILABLE_EXT:
            *value = m_resultAvailable;
            break;
        default:
            *value = 0;
            break;
        }
    }

    void setResultAvailable(unsigned resultAvailable) { m_resultAvailable = resultAvailable; }

private:
    unsigned m_resultAvailable;
};

TEST(ThrottledTextureUploaderTest, IsBusy)
{
    OwnPtr<FakeWebGraphicsContext3DWithQueryTesting> fakeContext(adoptPtr(new FakeWebGraphicsContext3DWithQueryTesting));
    OwnPtr<ThrottledTextureUploader> uploader = ThrottledTextureUploader::create(fakeContext.get(), 2);

    fakeContext->setResultAvailable(0);
    EXPECT_FALSE(uploader->isBusy());
    uploader->beginUploads();
    uploader->endUploads();
    EXPECT_FALSE(uploader->isBusy());
    uploader->beginUploads();
    uploader->endUploads();
    EXPECT_TRUE(uploader->isBusy());

    fakeContext->setResultAvailable(1);
    EXPECT_FALSE(uploader->isBusy());
    uploader->beginUploads();
    uploader->endUploads();
    EXPECT_FALSE(uploader->isBusy());
    uploader->beginUploads();
    uploader->endUploads();
    EXPECT_FALSE(uploader->isBusy());
    uploader->beginUploads();
    uploader->endUploads();
}

} // namespace
