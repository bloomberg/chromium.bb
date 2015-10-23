// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SimTest_h
#define SimTest_h

#include "web/tests/FrameTestHelpers.h"
#include "web/tests/sim/SimCompositor.h"
#include "web/tests/sim/SimNetwork.h"
#include "web/tests/sim/SimWebViewClient.h"
#include <gtest/gtest.h>

namespace blink {

class WebViewImpl;
class Document;

class SimTest : public ::testing::Test {
protected:
    SimTest();
    ~SimTest() override;

    void loadURL(const String& url);

    Document& document();
    WebViewImpl& webView();
    SimCompositor& compositor();

private:
    SimNetwork m_network;
    SimCompositor m_compositor;
    SimWebViewClient m_webViewClient;
    FrameTestHelpers::WebViewHelper m_webViewHelper;
};

} // namespace blink

#endif
