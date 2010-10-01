// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDOMEvent.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDOMEventListener.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDOMMutationEvent.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScriptSource.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"
#include "webkit/glue/web_io_operators.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_shell_request_context.h"
#include "webkit/tools/test_shell/test_shell_test.h"

namespace {

using namespace WebKit;

// This test exercices the event listener API from the WebKit API.
class WebDOMEventListenerTest : public TestShellTest {
 public:
  virtual void SetUp() {
    TestShellTest::SetUp();
    FilePath event_test_file;
    ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &event_test_file));
    event_test_file = event_test_file.Append(FILE_PATH_LITERAL("webkit"))
        .Append(FILE_PATH_LITERAL("data"))
        .Append(FILE_PATH_LITERAL("listener"))
        .Append(FILE_PATH_LITERAL("mutation_event_listener.html"));
    test_shell_->LoadFile(event_test_file);
    test_shell_->WaitTestFinished();
  }

  virtual void TearDown() {
    TestShellTest::TearDown();
  }

  WebFrame* main_frame() const {
    return test_shell_->webView()->mainFrame();
  }

  WebDocument document() const {
    return main_frame()->document();
  }

  void ExecuteScript(const char* code) {
    main_frame()->executeScript(WebScriptSource(WebString::fromUTF8(code)));
  }

  static WebString GetNodeID(const WebNode& node) {
    if (node.nodeType() != WebNode::ElementNode)
      return WebString();
    WebElement element = node.toConst<WebElement>();
    return element.getAttribute("id");
  }
};

class TestWebDOMEventListener : public WebDOMEventListener {
 public:
  TestWebDOMEventListener() {}
  virtual ~TestWebDOMEventListener() {}

  virtual void handleEvent(const WebDOMEvent& event) {
    events_.push_back(event);
  }

  size_t event_count() const { return events_.size(); }

  WebDOMEvent GetEventAt(int index) const { return events_.at(index); }

  void ClearEvents() { events_.clear(); }

 private:
  std::vector<WebDOMEvent> events_;
};

// Tests that the right mutation events are fired when a node is added/removed.
// Note that the DOMSubtreeModified event is fairly vage, it only tells you
// something changed for the target node.
TEST_F(WebDOMEventListenerTest, NodeAddedRemovedMutationEvent) {
  TestWebDOMEventListener event_listener;
  document().addEventListener("DOMSubtreeModified", &event_listener, false);

  // Test adding a node.
  ExecuteScript("addElement('newNode')");
  ASSERT_EQ(1U, event_listener.event_count());
  WebDOMEvent event = event_listener.GetEventAt(0);
  ASSERT_TRUE(event.isMutationEvent());
  // No need to check any of the MutationEvent, WebKit does not set any.
  EXPECT_EQ("DIV", event.target().nodeName());
  EXPECT_EQ("topDiv", GetNodeID(event.target()));
  event_listener.ClearEvents();

  // Test removing a node.
  ExecuteScript("removeNode('div1')");
  ASSERT_EQ(1U, event_listener.event_count());
  event = event_listener.GetEventAt(0);
  ASSERT_TRUE(event.isMutationEvent());
  EXPECT_EQ("DIV", event.target().nodeName());
  EXPECT_EQ("topDiv", GetNodeID(event.target()));
}

// Tests the right mutation event is fired when a text node is modified.
TEST_F(WebDOMEventListenerTest, TextNodeModifiedMutationEvent) {
  TestWebDOMEventListener event_listener;
  document().addEventListener("DOMSubtreeModified", &event_listener, false);
  ExecuteScript("changeText('div2', 'Hello')");
  ASSERT_EQ(1U, event_listener.event_count());
  WebDOMEvent event = event_listener.GetEventAt(0);
  ASSERT_TRUE(event.isMutationEvent());
  ASSERT_EQ(WebNode::TextNode, event.target().nodeType());
}

// Tests the right mutation events are fired when an attribute is added/removed.
TEST_F(WebDOMEventListenerTest, AttributeMutationEvent) {
  TestWebDOMEventListener event_listener;
  document().addEventListener("DOMSubtreeModified", &event_listener, false);
  ExecuteScript("document.getElementById('div2').setAttribute('myAttr',"
                "'some value')");
  ASSERT_EQ(1U, event_listener.event_count());
  WebDOMEvent event = event_listener.GetEventAt(0);
  ASSERT_TRUE(event.isMutationEvent());
  EXPECT_EQ("DIV", event.target().nodeName());
  EXPECT_EQ("div2", GetNodeID(event.target()));
  event_listener.ClearEvents();

  ExecuteScript("document.getElementById('div2').removeAttribute('myAttr')");
  ASSERT_EQ(1U, event_listener.event_count());
  event = event_listener.GetEventAt(0);
  ASSERT_TRUE(event.isMutationEvent());
  EXPECT_EQ("DIV", event.target().nodeName());
  EXPECT_EQ("div2", GetNodeID(event.target()));
}

// Tests destroying WebDOMEventListener and triggering events, we shouldn't
// crash.
TEST_F(WebDOMEventListenerTest, FireEventDeletedListener) {
  TestWebDOMEventListener*  event_listener = new TestWebDOMEventListener();
  document().addEventListener("DOMSubtreeModified", event_listener, false);
  delete event_listener;
  ExecuteScript("addElement('newNode')");  // That should fire an event.
}

// Tests registering several events on the same WebDOMEventListener and
// triggering events.
TEST_F(WebDOMEventListenerTest, SameListenerMultipleEvents) {
  TestWebDOMEventListener event_listener;
  const WebString kDOMSubtreeModifiedType("DOMSubtreeModified");
  const WebString kDOMNodeRemovedType("DOMNodeRemoved");
  document().addEventListener(kDOMSubtreeModifiedType, &event_listener, false);
  WebElement div1_elem = document().getElementById("div1");
  div1_elem.addEventListener(kDOMNodeRemovedType, &event_listener, false);

  // Trigger a DOMSubtreeModified event by adding a node.
  ExecuteScript("addElement('newNode')");
  ASSERT_EQ(1U, event_listener.event_count());
  WebDOMEvent event = event_listener.GetEventAt(0);
  ASSERT_TRUE(event.isMutationEvent());
  EXPECT_EQ("DIV", event.target().nodeName());
  EXPECT_EQ("topDiv", GetNodeID(event.target()));
  event_listener.ClearEvents();

  // Trigger for both event listener by removing the div1 node.
  ExecuteScript("removeNode('div1')");
  ASSERT_EQ(2U, event_listener.event_count());
  // Not sure if the order of the events is important. Assuming no specific
  // order.
  WebString type1 = event_listener.GetEventAt(0).type();
  WebString type2 = event_listener.GetEventAt(1).type();
  EXPECT_TRUE(type1 == kDOMSubtreeModifiedType || type1 == kDOMNodeRemovedType);
  EXPECT_TRUE(type2 == kDOMSubtreeModifiedType || type2 == kDOMNodeRemovedType);
  EXPECT_TRUE(type1 != type2);
}

// Tests removing event listeners.
TEST_F(WebDOMEventListenerTest, RemoveEventListener) {
  TestWebDOMEventListener event_listener;
  const WebString kDOMSubtreeModifiedType("DOMSubtreeModified");
  // Adding twice the same listener for the same event, should be supported.
  document().addEventListener(kDOMSubtreeModifiedType, &event_listener, false);
  document().addEventListener(kDOMSubtreeModifiedType, &event_listener, false);

  // Add a node, that should trigger 2 events.
  ExecuteScript("addElement('newNode')");
  EXPECT_EQ(2U, event_listener.event_count());
  event_listener.ClearEvents();

  // Remove one listener and trigger an event again.
  document().removeEventListener(
      kDOMSubtreeModifiedType, &event_listener, false);
  ExecuteScript("addElement('newerNode')");
  EXPECT_EQ(1U, event_listener.event_count());
  event_listener.ClearEvents();

  // Remove the last listener and trigger yet another event.
  document().removeEventListener(
      kDOMSubtreeModifiedType, &event_listener, false);
  ExecuteScript("addElement('newererNode')");
  EXPECT_EQ(0U, event_listener.event_count());
}

}  // namespace
