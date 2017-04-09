// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SimTest_h
#define SimTest_h

#include <gtest/gtest.h>
#include "web/tests/FrameTestHelpers.h"
#include "web/tests/sim/SimCompositor.h"
#include "web/tests/sim/SimNetwork.h"
#include "web/tests/sim/SimPage.h"
#include "web/tests/sim/SimWebFrameClient.h"
#include "web/tests/sim/SimWebViewClient.h"

namespace blink {

class WebViewImpl;
class WebLocalFrameImpl;
class Document;
class LocalDOMWindow;

class SimTest : public ::testing::Test {
 protected:
  SimTest();
  ~SimTest() override;

  void LoadURL(const String& url);

  LocalDOMWindow& Window();
  SimPage& Page();
  Document& GetDocument();
  WebViewImpl& WebView();
  WebLocalFrameImpl& MainFrame();
  const SimWebViewClient& WebViewClient() const;
  SimCompositor& Compositor();

  Vector<String>& ConsoleMessages() { return console_messages_; }

 private:
  friend class SimWebFrameClient;

  void AddConsoleMessage(const String&);

  SimNetwork network_;
  SimCompositor compositor_;
  SimWebViewClient web_view_client_;
  SimWebFrameClient web_frame_client_;
  SimPage page_;
  FrameTestHelpers::WebViewHelper web_view_helper_;

  Vector<String> console_messages_;
};

}  // namespace blink

#endif
