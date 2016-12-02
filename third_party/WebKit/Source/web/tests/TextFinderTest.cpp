// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/TextFinder.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Document.h"
#include "core/dom/NodeList.h"
#include "core/dom/Range.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLElement.h"
#include "core/layout/TextAutosizer.h"
#include "core/page/Page.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/web/WebDocument.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/FindInPageCoordinates.h"
#include "web/WebLocalFrameImpl.h"
#include "web/tests/FrameTestHelpers.h"

using blink::testing::runPendingTasks;

namespace blink {

class TextFinderTest : public ::testing::Test {
 protected:
  TextFinderTest() {
    m_webViewHelper.initialize();
    WebLocalFrameImpl& frameImpl = *m_webViewHelper.webView()->mainFrameImpl();
    frameImpl.viewImpl()->resize(WebSize(640, 480));
    frameImpl.viewImpl()->updateAllLifecyclePhases();
    m_document = static_cast<Document*>(frameImpl.document());
    m_textFinder = &frameImpl.ensureTextFinder();
  }

  Document& document() const;
  TextFinder& textFinder() const;

  static WebFloatRect findInPageRect(Node* startContainer,
                                     int startOffset,
                                     Node* endContainer,
                                     int endOffset);

 private:
  FrameTestHelpers::WebViewHelper m_webViewHelper;
  Persistent<Document> m_document;
  Persistent<TextFinder> m_textFinder;
};

Document& TextFinderTest::document() const {
  return *m_document;
}

TextFinder& TextFinderTest::textFinder() const {
  return *m_textFinder;
}

WebFloatRect TextFinderTest::findInPageRect(Node* startContainer,
                                            int startOffset,
                                            Node* endContainer,
                                            int endOffset) {
  Range* range = Range::create(startContainer->document(), startContainer,
                               startOffset, endContainer, endOffset);
  return WebFloatRect(findInPageRectFromRange(range));
}

TEST_F(TextFinderTest, FindTextSimple) {
  document().body()->setInnerHTML("XXXXFindMeYYYYfindmeZZZZ");
  document().updateStyleAndLayout();
  Node* textNode = document().body()->firstChild();

  int identifier = 0;
  WebString searchText(String("FindMe"));
  WebFindOptions findOptions;  // Default.
  bool wrapWithinFrame = true;

  ASSERT_TRUE(
      textFinder().find(identifier, searchText, findOptions, wrapWithinFrame));
  Range* activeMatch = textFinder().activeMatch();
  ASSERT_TRUE(activeMatch);
  EXPECT_EQ(textNode, activeMatch->startContainer());
  EXPECT_EQ(4, activeMatch->startOffset());
  EXPECT_EQ(textNode, activeMatch->endContainer());
  EXPECT_EQ(10, activeMatch->endOffset());

  findOptions.findNext = true;
  ASSERT_TRUE(
      textFinder().find(identifier, searchText, findOptions, wrapWithinFrame));
  activeMatch = textFinder().activeMatch();
  ASSERT_TRUE(activeMatch);
  EXPECT_EQ(textNode, activeMatch->startContainer());
  EXPECT_EQ(14, activeMatch->startOffset());
  EXPECT_EQ(textNode, activeMatch->endContainer());
  EXPECT_EQ(20, activeMatch->endOffset());

  // Should wrap to the first match.
  ASSERT_TRUE(
      textFinder().find(identifier, searchText, findOptions, wrapWithinFrame));
  activeMatch = textFinder().activeMatch();
  ASSERT_TRUE(activeMatch);
  EXPECT_EQ(textNode, activeMatch->startContainer());
  EXPECT_EQ(4, activeMatch->startOffset());
  EXPECT_EQ(textNode, activeMatch->endContainer());
  EXPECT_EQ(10, activeMatch->endOffset());

  // Search in the reverse order.
  identifier = 1;
  findOptions = WebFindOptions();
  findOptions.forward = false;

  ASSERT_TRUE(
      textFinder().find(identifier, searchText, findOptions, wrapWithinFrame));
  activeMatch = textFinder().activeMatch();
  ASSERT_TRUE(activeMatch);
  EXPECT_EQ(textNode, activeMatch->startContainer());
  EXPECT_EQ(14, activeMatch->startOffset());
  EXPECT_EQ(textNode, activeMatch->endContainer());
  EXPECT_EQ(20, activeMatch->endOffset());

  findOptions.findNext = true;
  ASSERT_TRUE(
      textFinder().find(identifier, searchText, findOptions, wrapWithinFrame));
  activeMatch = textFinder().activeMatch();
  ASSERT_TRUE(activeMatch);
  EXPECT_EQ(textNode, activeMatch->startContainer());
  EXPECT_EQ(4, activeMatch->startOffset());
  EXPECT_EQ(textNode, activeMatch->endContainer());
  EXPECT_EQ(10, activeMatch->endOffset());

  // Wrap to the first match (last occurence in the document).
  ASSERT_TRUE(
      textFinder().find(identifier, searchText, findOptions, wrapWithinFrame));
  activeMatch = textFinder().activeMatch();
  ASSERT_TRUE(activeMatch);
  EXPECT_EQ(textNode, activeMatch->startContainer());
  EXPECT_EQ(14, activeMatch->startOffset());
  EXPECT_EQ(textNode, activeMatch->endContainer());
  EXPECT_EQ(20, activeMatch->endOffset());
}

TEST_F(TextFinderTest, FindTextAutosizing) {
  document().body()->setInnerHTML("XXXXFindMeYYYYfindmeZZZZ");
  document().updateStyleAndLayout();

  int identifier = 0;
  WebString searchText(String("FindMe"));
  WebFindOptions findOptions;  // Default.
  bool wrapWithinFrame = true;

  // Set viewport scale to 20 in order to simulate zoom-in
  VisualViewport& visualViewport =
      document().page()->frameHost().visualViewport();
  visualViewport.setScale(20);

  // Enforce autosizing
  document().settings()->setTextAutosizingEnabled(true);
  document().settings()->setTextAutosizingWindowSizeOverride(IntSize(20, 20));
  document().textAutosizer()->updatePageInfo();
  document().updateStyleAndLayout();

  // In case of autosizing, scale _should_ change
  ASSERT_TRUE(
      textFinder().find(identifier, searchText, findOptions, wrapWithinFrame));
  ASSERT_TRUE(textFinder().activeMatch());
  ASSERT_EQ(1, visualViewport.scale());  // in this case to 1

  // Disable autosizing and reset scale to 20
  visualViewport.setScale(20);
  document().settings()->setTextAutosizingEnabled(false);
  document().textAutosizer()->updatePageInfo();
  document().updateStyleAndLayout();

  ASSERT_TRUE(
      textFinder().find(identifier, searchText, findOptions, wrapWithinFrame));
  ASSERT_TRUE(textFinder().activeMatch());
  ASSERT_EQ(20, visualViewport.scale());
}

TEST_F(TextFinderTest, FindTextNotFound) {
  document().body()->setInnerHTML("XXXXFindMeYYYYfindmeZZZZ");
  document().updateStyleAndLayout();

  int identifier = 0;
  WebString searchText(String("Boo"));
  WebFindOptions findOptions;  // Default.
  bool wrapWithinFrame = true;

  EXPECT_FALSE(
      textFinder().find(identifier, searchText, findOptions, wrapWithinFrame));
  EXPECT_FALSE(textFinder().activeMatch());
}

TEST_F(TextFinderTest, FindTextInShadowDOM) {
  document().body()->setInnerHTML("<b>FOO</b><i>foo</i>");
  ShadowRoot* shadowRoot = document().body()->createShadowRootInternal(
      ShadowRootType::V0, ASSERT_NO_EXCEPTION);
  shadowRoot->setInnerHTML(
      "<content select=\"i\"></content><u>Foo</u><content></content>");
  Node* textInBElement = document().body()->firstChild()->firstChild();
  Node* textInIElement = document().body()->lastChild()->firstChild();
  Node* textInUElement = shadowRoot->childNodes()->item(1)->firstChild();
  document().updateStyleAndLayout();

  int identifier = 0;
  WebString searchText(String("foo"));
  WebFindOptions findOptions;  // Default.
  bool wrapWithinFrame = true;

  // TextIterator currently returns the matches in the flat treeorder, so
  // in this case the matches will be returned in the order of
  // <i> -> <u> -> <b>.
  ASSERT_TRUE(
      textFinder().find(identifier, searchText, findOptions, wrapWithinFrame));
  Range* activeMatch = textFinder().activeMatch();
  ASSERT_TRUE(activeMatch);
  EXPECT_EQ(textInIElement, activeMatch->startContainer());
  EXPECT_EQ(0, activeMatch->startOffset());
  EXPECT_EQ(textInIElement, activeMatch->endContainer());
  EXPECT_EQ(3, activeMatch->endOffset());

  findOptions.findNext = true;
  ASSERT_TRUE(
      textFinder().find(identifier, searchText, findOptions, wrapWithinFrame));
  activeMatch = textFinder().activeMatch();
  ASSERT_TRUE(activeMatch);
  EXPECT_EQ(textInUElement, activeMatch->startContainer());
  EXPECT_EQ(0, activeMatch->startOffset());
  EXPECT_EQ(textInUElement, activeMatch->endContainer());
  EXPECT_EQ(3, activeMatch->endOffset());

  ASSERT_TRUE(
      textFinder().find(identifier, searchText, findOptions, wrapWithinFrame));
  activeMatch = textFinder().activeMatch();
  ASSERT_TRUE(activeMatch);
  EXPECT_EQ(textInBElement, activeMatch->startContainer());
  EXPECT_EQ(0, activeMatch->startOffset());
  EXPECT_EQ(textInBElement, activeMatch->endContainer());
  EXPECT_EQ(3, activeMatch->endOffset());

  // Should wrap to the first match.
  ASSERT_TRUE(
      textFinder().find(identifier, searchText, findOptions, wrapWithinFrame));
  activeMatch = textFinder().activeMatch();
  ASSERT_TRUE(activeMatch);
  EXPECT_EQ(textInIElement, activeMatch->startContainer());
  EXPECT_EQ(0, activeMatch->startOffset());
  EXPECT_EQ(textInIElement, activeMatch->endContainer());
  EXPECT_EQ(3, activeMatch->endOffset());

  // Fresh search in the reverse order.
  identifier = 1;
  findOptions = WebFindOptions();
  findOptions.forward = false;

  ASSERT_TRUE(
      textFinder().find(identifier, searchText, findOptions, wrapWithinFrame));
  activeMatch = textFinder().activeMatch();
  ASSERT_TRUE(activeMatch);
  EXPECT_EQ(textInBElement, activeMatch->startContainer());
  EXPECT_EQ(0, activeMatch->startOffset());
  EXPECT_EQ(textInBElement, activeMatch->endContainer());
  EXPECT_EQ(3, activeMatch->endOffset());

  findOptions.findNext = true;
  ASSERT_TRUE(
      textFinder().find(identifier, searchText, findOptions, wrapWithinFrame));
  activeMatch = textFinder().activeMatch();
  ASSERT_TRUE(activeMatch);
  EXPECT_EQ(textInUElement, activeMatch->startContainer());
  EXPECT_EQ(0, activeMatch->startOffset());
  EXPECT_EQ(textInUElement, activeMatch->endContainer());
  EXPECT_EQ(3, activeMatch->endOffset());

  ASSERT_TRUE(
      textFinder().find(identifier, searchText, findOptions, wrapWithinFrame));
  activeMatch = textFinder().activeMatch();
  ASSERT_TRUE(activeMatch);
  EXPECT_EQ(textInIElement, activeMatch->startContainer());
  EXPECT_EQ(0, activeMatch->startOffset());
  EXPECT_EQ(textInIElement, activeMatch->endContainer());
  EXPECT_EQ(3, activeMatch->endOffset());

  // And wrap.
  ASSERT_TRUE(
      textFinder().find(identifier, searchText, findOptions, wrapWithinFrame));
  activeMatch = textFinder().activeMatch();
  ASSERT_TRUE(activeMatch);
  EXPECT_EQ(textInBElement, activeMatch->startContainer());
  EXPECT_EQ(0, activeMatch->startOffset());
  EXPECT_EQ(textInBElement, activeMatch->endContainer());
  EXPECT_EQ(3, activeMatch->endOffset());
}

TEST_F(TextFinderTest, ScopeTextMatchesSimple) {
  document().body()->setInnerHTML("XXXXFindMeYYYYfindmeZZZZ");
  document().updateStyleAndLayout();

  Node* textNode = document().body()->firstChild();

  int identifier = 0;
  WebString searchText(String("FindMe"));
  WebFindOptions findOptions;  // Default.

  textFinder().resetMatchCount();
  textFinder().startScopingStringMatches(identifier, searchText, findOptions);
  while (textFinder().scopingInProgress())
    runPendingTasks();

  EXPECT_EQ(2, textFinder().totalMatchCount());
  WebVector<WebFloatRect> matchRects;
  textFinder().findMatchRects(matchRects);
  ASSERT_EQ(2u, matchRects.size());
  EXPECT_EQ(findInPageRect(textNode, 4, textNode, 10), matchRects[0]);
  EXPECT_EQ(findInPageRect(textNode, 14, textNode, 20), matchRects[1]);
}

TEST_F(TextFinderTest, ScopeTextMatchesRepeated) {
  document().body()->setInnerHTML("XXXXFindMeYYYYfindmeZZZZ");
  document().updateStyleAndLayout();

  Node* textNode = document().body()->firstChild();

  int identifier = 0;
  WebString searchText1(String("XFindMe"));
  WebString searchText2(String("FindMe"));
  WebFindOptions findOptions;  // Default.

  textFinder().resetMatchCount();
  textFinder().startScopingStringMatches(identifier, searchText1, findOptions);
  textFinder().startScopingStringMatches(identifier, searchText2, findOptions);
  while (textFinder().scopingInProgress())
    runPendingTasks();

  // Only searchText2 should be highlighted.
  EXPECT_EQ(2, textFinder().totalMatchCount());
  WebVector<WebFloatRect> matchRects;
  textFinder().findMatchRects(matchRects);
  ASSERT_EQ(2u, matchRects.size());
  EXPECT_EQ(findInPageRect(textNode, 4, textNode, 10), matchRects[0]);
  EXPECT_EQ(findInPageRect(textNode, 14, textNode, 20), matchRects[1]);
}

TEST_F(TextFinderTest, ScopeTextMatchesWithShadowDOM) {
  document().body()->setInnerHTML("<b>FOO</b><i>foo</i>");
  ShadowRoot* shadowRoot = document().body()->createShadowRootInternal(
      ShadowRootType::V0, ASSERT_NO_EXCEPTION);
  shadowRoot->setInnerHTML(
      "<content select=\"i\"></content><u>Foo</u><content></content>");
  Node* textInBElement = document().body()->firstChild()->firstChild();
  Node* textInIElement = document().body()->lastChild()->firstChild();
  Node* textInUElement = shadowRoot->childNodes()->item(1)->firstChild();
  document().updateStyleAndLayout();

  int identifier = 0;
  WebString searchText(String("fOO"));
  WebFindOptions findOptions;  // Default.

  textFinder().resetMatchCount();
  textFinder().startScopingStringMatches(identifier, searchText, findOptions);
  while (textFinder().scopingInProgress())
    runPendingTasks();

  // TextIterator currently returns the matches in the flat tree order,
  // so in this case the matches will be returned in the order of
  // <i> -> <u> -> <b>.
  EXPECT_EQ(3, textFinder().totalMatchCount());
  WebVector<WebFloatRect> matchRects;
  textFinder().findMatchRects(matchRects);
  ASSERT_EQ(3u, matchRects.size());
  EXPECT_EQ(findInPageRect(textInIElement, 0, textInIElement, 3),
            matchRects[0]);
  EXPECT_EQ(findInPageRect(textInUElement, 0, textInUElement, 3),
            matchRects[1]);
  EXPECT_EQ(findInPageRect(textInBElement, 0, textInBElement, 3),
            matchRects[2]);
}

TEST_F(TextFinderTest, ScopeRepeatPatternTextMatches) {
  document().body()->setInnerHTML("ab ab ab ab ab");
  document().updateStyleAndLayout();

  Node* textNode = document().body()->firstChild();

  int identifier = 0;
  WebString searchText(String("ab ab"));
  WebFindOptions findOptions;  // Default.

  textFinder().resetMatchCount();
  textFinder().startScopingStringMatches(identifier, searchText, findOptions);
  while (textFinder().scopingInProgress())
    runPendingTasks();

  EXPECT_EQ(2, textFinder().totalMatchCount());
  WebVector<WebFloatRect> matchRects;
  textFinder().findMatchRects(matchRects);
  ASSERT_EQ(2u, matchRects.size());
  EXPECT_EQ(findInPageRect(textNode, 0, textNode, 5), matchRects[0]);
  EXPECT_EQ(findInPageRect(textNode, 6, textNode, 11), matchRects[1]);
}

TEST_F(TextFinderTest, OverlappingMatches) {
  document().body()->setInnerHTML("aababaa");
  document().updateStyleAndLayout();

  Node* textNode = document().body()->firstChild();

  int identifier = 0;
  WebString searchText(String("aba"));
  WebFindOptions findOptions;  // Default.

  textFinder().resetMatchCount();
  textFinder().startScopingStringMatches(identifier, searchText, findOptions);
  while (textFinder().scopingInProgress())
    runPendingTasks();

  // We shouldn't find overlapped matches.
  EXPECT_EQ(1, textFinder().totalMatchCount());
  WebVector<WebFloatRect> matchRects;
  textFinder().findMatchRects(matchRects);
  ASSERT_EQ(1u, matchRects.size());
  EXPECT_EQ(findInPageRect(textNode, 1, textNode, 4), matchRects[0]);
}

TEST_F(TextFinderTest, SequentialMatches) {
  document().body()->setInnerHTML("ababab");
  document().updateStyleAndLayout();

  Node* textNode = document().body()->firstChild();

  int identifier = 0;
  WebString searchText(String("ab"));
  WebFindOptions findOptions;  // Default.

  textFinder().resetMatchCount();
  textFinder().startScopingStringMatches(identifier, searchText, findOptions);
  while (textFinder().scopingInProgress())
    runPendingTasks();

  EXPECT_EQ(3, textFinder().totalMatchCount());
  WebVector<WebFloatRect> matchRects;
  textFinder().findMatchRects(matchRects);
  ASSERT_EQ(3u, matchRects.size());
  EXPECT_EQ(findInPageRect(textNode, 0, textNode, 2), matchRects[0]);
  EXPECT_EQ(findInPageRect(textNode, 2, textNode, 4), matchRects[1]);
  EXPECT_EQ(findInPageRect(textNode, 4, textNode, 6), matchRects[2]);
}

TEST_F(TextFinderTest, FindTextJavaScriptUpdatesDOM) {
  document().body()->setInnerHTML("<b>XXXXFindMeYYYY</b><i></i>");
  document().updateStyleAndLayout();

  int identifier = 0;
  WebString searchText(String("FindMe"));
  WebFindOptions findOptions;  // Default.
  bool wrapWithinFrame = true;
  bool activeNow;

  textFinder().resetMatchCount();
  textFinder().startScopingStringMatches(identifier, searchText, findOptions);
  while (textFinder().scopingInProgress())
    runPendingTasks();

  findOptions.findNext = true;
  ASSERT_TRUE(textFinder().find(identifier, searchText, findOptions,
                                wrapWithinFrame, &activeNow));
  EXPECT_TRUE(activeNow);
  ASSERT_TRUE(textFinder().find(identifier, searchText, findOptions,
                                wrapWithinFrame, &activeNow));
  EXPECT_TRUE(activeNow);

  // Add new text to DOM and try FindNext.
  Element* iElement = toElement(document().body()->lastChild());
  ASSERT_TRUE(iElement);
  iElement->setInnerHTML("ZZFindMe");
  document().updateStyleAndLayout();

  ASSERT_TRUE(textFinder().find(identifier, searchText, findOptions,
                                wrapWithinFrame, &activeNow));
  Range* activeMatch = textFinder().activeMatch();
  ASSERT_TRUE(activeMatch);
  EXPECT_FALSE(activeNow);
  EXPECT_EQ(2, activeMatch->startOffset());
  EXPECT_EQ(8, activeMatch->endOffset());

  // Restart full search and check that added text is found.
  findOptions.findNext = false;
  textFinder().resetMatchCount();
  textFinder().cancelPendingScopingEffort();
  textFinder().startScopingStringMatches(identifier, searchText, findOptions);
  while (textFinder().scopingInProgress())
    runPendingTasks();
  EXPECT_EQ(2, textFinder().totalMatchCount());

  WebVector<WebFloatRect> matchRects;
  textFinder().findMatchRects(matchRects);
  ASSERT_EQ(2u, matchRects.size());
  Node* textInBElement = document().body()->firstChild()->firstChild();
  Node* textInIElement = document().body()->lastChild()->firstChild();
  EXPECT_EQ(findInPageRect(textInBElement, 4, textInBElement, 10),
            matchRects[0]);
  EXPECT_EQ(findInPageRect(textInIElement, 2, textInIElement, 8),
            matchRects[1]);
}

class TextFinderFakeTimerTest : public TextFinderTest {
 protected:
  void SetUp() override {
    s_timeElapsed = 0.0;
    m_originalTimeFunction = setTimeFunctionsForTesting(returnMockTime);
  }

  void TearDown() override {
    setTimeFunctionsForTesting(m_originalTimeFunction);
  }

 private:
  static double returnMockTime() {
    s_timeElapsed += 1.0;
    return s_timeElapsed;
  }

  TimeFunction m_originalTimeFunction;
  static double s_timeElapsed;
};

double TextFinderFakeTimerTest::s_timeElapsed;

TEST_F(TextFinderFakeTimerTest, ScopeWithTimeouts) {
  // Make a long string.
  String text(Vector<UChar>(100));
  text.fill('a');
  String searchPattern("abc");
  // Make 4 substrings "abc" in text.
  text.insert(searchPattern, 1);
  text.insert(searchPattern, 10);
  text.insert(searchPattern, 50);
  text.insert(searchPattern, 90);

  document().body()->setInnerHTML(text);
  document().updateStyleAndLayout();

  int identifier = 0;
  WebFindOptions findOptions;  // Default.

  textFinder().resetMatchCount();

  // There will be only one iteration before timeout, because increment
  // of the TimeProxyPlatform timer is greater than timeout threshold.
  textFinder().startScopingStringMatches(identifier, searchPattern,
                                         findOptions);
  while (textFinder().scopingInProgress())
    runPendingTasks();

  EXPECT_EQ(4, textFinder().totalMatchCount());
}

}  // namespace blink
