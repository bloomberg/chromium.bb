// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebLayerTreeViewTestCommon_h
#define WebLayerTreeViewTestCommon_h

#include "cc/test/fake_output_surface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayerTreeViewClient.h"

namespace WebKit {

class MockWebLayerTreeViewClient : public WebLayerTreeViewClient {
public:
    MOCK_METHOD0(scheduleComposite, void());
    virtual void updateAnimations(double frameBeginTime) OVERRIDE { }
    MOCK_METHOD0(willBeginFrame, void());
    MOCK_METHOD0(didBeginFrame, void());
    virtual void layout() OVERRIDE { }
    virtual void applyScrollAndScale(const WebSize& scrollDelta, float scaleFactor) OVERRIDE { }

    virtual cc::OutputSurface* createOutputSurface() OVERRIDE
    {
      return cc::createFakeOutputSurface().release();
    }
    virtual void didRecreateOutputSurface(bool) OVERRIDE { }

    MOCK_METHOD0(willCommit, void());
    MOCK_METHOD0(didCommit, void());
    virtual void didCommitAndDrawFrame() OVERRIDE { }
    virtual void didCompleteSwapBuffers() OVERRIDE { }
};

}

#endif // WebLayerTreeViewTestCommon_h
