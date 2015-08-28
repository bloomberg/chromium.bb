// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/dom/Document.h"
#include "web/WebLocalFrameImpl.h"
#include "web/tests/FrameTestHelpers.h"
#include "web/tests/sim/SimLayerTreeView.h"
#include "web/tests/sim/SimNetwork.h"
#include "web/tests/sim/SimRequest.h"
#include "web/tests/sim/SimWebViewClient.h"
#include <gtest/gtest.h>

namespace blink {

class DocumentLoadingRenderingTest : public ::testing::Test {
    void SetUp() override
    {
        Document::setThreadedParsingEnabledForUnitTestsOnly(false);
    }

    void TearDown() override
    {
        Document::setThreadedParsingEnabledForUnitTestsOnly(true);
    }
};

TEST_F(DocumentLoadingRenderingTest, ShouldResumeCommitsAfterBodyParsedWithoutSheets)
{
    SimLayerTreeView layerTreeView;
    SimWebViewClient webViewClient(layerTreeView);
    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initialize(true, nullptr, &webViewClient);

    SimNetwork network;
    SimRequest mainResource("https://example.com/test.html", "text/html");

    WebURLRequest request;
    request.initialize();
    request.setURL(KURL(ParsedURLString, "https://example.com/test.html"));

    webViewHelper.webViewImpl()->mainFrameImpl()->loadRequest(request);

    mainResource.start();

    // Still in the head, should not resume commits.
    mainResource.write("<!DOCTYPE html>");
    EXPECT_TRUE(layerTreeView.deferCommits());
    mainResource.write("<title>Test</title><style>div { color red; }</style>");
    EXPECT_TRUE(layerTreeView.deferCommits());

    // Implicitly inserts the body. Since there's no loading stylesheets we
    // should resume commits.
    mainResource.write("<p>Hello World</p>");
    EXPECT_FALSE(layerTreeView.deferCommits());

    // Finish the load, should stay resumed.
    mainResource.finish();
    EXPECT_FALSE(layerTreeView.deferCommits());
}

} // namespace blink
