// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/events/TouchEvent.h"

#include "core/frame/FrameConsole.h"
#include "core/frame/UseCounter.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace blink {

class ConsoleCapturingChromeClient : public EmptyChromeClient {
 public:
  ConsoleCapturingChromeClient() : EmptyChromeClient() {}

  // ChromeClient methods:
  void addMessageToConsole(LocalFrame*,
                           MessageSource messageSource,
                           MessageLevel,
                           const String& message,
                           unsigned lineNumber,
                           const String& sourceID,
                           const String& stackTrace) override {
    m_messages.push_back(message);
    m_messageSources.push_back(messageSource);
  }

  // Expose console output.
  const std::vector<String>& messages() { return m_messages; }
  const std::vector<MessageSource>& messageSources() {
    return m_messageSources;
  }

 private:
  std::vector<String> m_messages;
  std::vector<MessageSource> m_messageSources;
};

class TouchEventTest : public testing::Test {
 public:
  TouchEventTest() {
    m_chromeClient = new ConsoleCapturingChromeClient();
    Page::PageClients clients;
    fillWithEmptyClients(clients);
    clients.chromeClient = m_chromeClient.get();
    m_pageHolder = DummyPageHolder::create(IntSize(800, 600), &clients);
  }

  const std::vector<String>& messages() { return m_chromeClient->messages(); }
  const std::vector<MessageSource>& messageSources() {
    return m_chromeClient->messageSources();
  }

  LocalDOMWindow& window() { return *m_pageHolder->frame().domWindow(); }

  Document& document() { return m_pageHolder->document(); }

  TouchEvent* eventWithDispatchType(WebInputEvent::DispatchType dispatchType) {
    WebTouchEvent webTouchEvent(WebInputEvent::TouchStart, 0, 0);
    webTouchEvent.dispatchType = dispatchType;
    return TouchEvent::create(webTouchEvent, nullptr, nullptr, nullptr,
                              "touchstart", &window(), TouchActionAuto);
  }

 private:
  Persistent<ConsoleCapturingChromeClient> m_chromeClient;
  std::unique_ptr<DummyPageHolder> m_pageHolder;
};

TEST_F(TouchEventTest, PreventDefaultUncancelable) {
  TouchEvent* event = eventWithDispatchType(WebInputEvent::EventNonBlocking);
  event->setHandlingPassive(Event::PassiveMode::NotPassiveDefault);

  EXPECT_THAT(messages(), ElementsAre());
  event->preventDefault();
  EXPECT_THAT(messages(),
              ElementsAre("Ignored attempt to cancel a touchstart event with "
                          "cancelable=false, for example because scrolling is "
                          "in progress and cannot be interrupted."));
  EXPECT_THAT(messageSources(), ElementsAre(JSMessageSource));

  EXPECT_TRUE(UseCounter::isCounted(
      document(), UseCounter::UncancellableTouchEventPreventDefaulted));
  EXPECT_FALSE(UseCounter::isCounted(
      document(),
      UseCounter::
          UncancellableTouchEventDueToMainThreadResponsivenessPreventDefaulted));
}

TEST_F(TouchEventTest,
       PreventDefaultUncancelableDueToMainThreadResponsiveness) {
  TouchEvent* event = eventWithDispatchType(
      WebInputEvent::ListenersForcedNonBlockingDueToMainThreadResponsiveness);
  event->setHandlingPassive(Event::PassiveMode::NotPassiveDefault);

  EXPECT_THAT(messages(), ElementsAre());
  event->preventDefault();
  EXPECT_THAT(messages(),
              ElementsAre("Ignored attempt to cancel a touchstart event with "
                          "cancelable=false. This event was forced to be "
                          "non-cancellable because the page was too busy to "
                          "handle the event promptly."));
  EXPECT_THAT(messageSources(), ElementsAre(InterventionMessageSource));

  EXPECT_TRUE(UseCounter::isCounted(
      document(), UseCounter::UncancellableTouchEventPreventDefaulted));
  EXPECT_TRUE(UseCounter::isCounted(
      document(),
      UseCounter::
          UncancellableTouchEventDueToMainThreadResponsivenessPreventDefaulted));
}

TEST_F(TouchEventTest,
       PreventDefaultPassiveDueToDocumentLevelScrollerIntervention) {
  TouchEvent* event =
      eventWithDispatchType(WebInputEvent::ListenersNonBlockingPassive);
  event->setHandlingPassive(Event::PassiveMode::PassiveForcedDocumentLevel);

  EXPECT_THAT(messages(), ElementsAre());
  event->preventDefault();
  EXPECT_THAT(
      messages(),
      ElementsAre("Unable to preventDefault inside passive event listener due "
                  "to target being treated as passive. See "
                  "https://www.chromestatus.com/features/5093566007214080"));
  EXPECT_THAT(messageSources(), ElementsAre(InterventionMessageSource));
}

class TouchEventTestNoFrame : public testing::Test {};

TEST_F(TouchEventTestNoFrame, PreventDefaultDoesntRequireFrame) {
  TouchEvent::create()->preventDefault();
}

}  // namespace blink
