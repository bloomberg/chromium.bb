// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/tests/sim/SimTest.h"

#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "platform/LayoutTestSupport.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/WebCache.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"

namespace blink {

SimTest::SimTest() : m_webViewClient(m_compositor), m_webFrameClient(*this) {
  Document::setThreadedParsingEnabledForTesting(false);
  // Use the mock theme to get more predictable code paths, this also avoids
  // the OS callbacks in ScrollAnimatorMac which can schedule frames
  // unpredictably since the OS will randomly call into blink for
  // updateScrollerStyleForNewRecommendedScrollerStyle which then does
  // FrameView::scrollbarStyleChanged and will adjust the scrollbar existence
  // in the middle of a test.
  LayoutTestSupport::setMockThemeEnabledForTest(true);
  ScrollbarTheme::setMockScrollbarsEnabled(true);
  m_webViewHelper.initialize(true, &m_webFrameClient, &m_webViewClient);
  m_compositor.setWebViewImpl(webView());
  m_page.setPage(webView().page());
}

SimTest::~SimTest() {
  // Pump the message loop to process the load event.
  testing::runPendingTasks();

  Document::setThreadedParsingEnabledForTesting(true);
  LayoutTestSupport::setMockThemeEnabledForTest(false);
  ScrollbarTheme::setMockScrollbarsEnabled(false);
  WebCache::clear();
}

void SimTest::loadURL(const String& url) {
  WebURLRequest request(KURL(ParsedURLString, url));
  webView().mainFrameImpl()->loadRequest(request);
}

LocalDOMWindow& SimTest::window() {
  return *document().domWindow();
}

SimPage& SimTest::page() {
  return m_page;
}

Document& SimTest::document() {
  return *webView().mainFrameImpl()->frame()->document();
}

WebViewImpl& SimTest::webView() {
  return *m_webViewHelper.webView();
}

WebLocalFrameImpl& SimTest::mainFrame() {
  return *webView().mainFrameImpl();
}

const SimWebViewClient& SimTest::webViewClient() const {
  return m_webViewClient;
}

SimCompositor& SimTest::compositor() {
  return m_compositor;
}

void SimTest::addConsoleMessage(const String& message) {
  m_consoleMessages.push_back(message);
}

}  // namespace blink
