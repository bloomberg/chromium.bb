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

using ::testing::ElementsAre;

namespace blink {

class ConsoleCapturingChromeClient : public EmptyChromeClient {
 public:
  ConsoleCapturingChromeClient() : EmptyChromeClient() {}

  // ChromeClient methods:
  void AddMessageToConsole(LocalFrame*,
                           MessageSource message_source,
                           MessageLevel,
                           const String& message,
                           unsigned line_number,
                           const String& source_id,
                           const String& stack_trace) override {
    messages_.push_back(message);
    message_sources_.push_back(message_source);
  }

  // Expose console output.
  const std::vector<String>& Messages() { return messages_; }
  const std::vector<MessageSource>& MessageSources() {
    return message_sources_;
  }

 private:
  std::vector<String> messages_;
  std::vector<MessageSource> message_sources_;
};

class TouchEventTest : public ::testing::Test {
 public:
  TouchEventTest() {
    chrome_client_ = new ConsoleCapturingChromeClient();
    Page::PageClients clients;
    FillWithEmptyClients(clients);
    clients.chrome_client = chrome_client_.Get();
    page_holder_ = DummyPageHolder::Create(IntSize(800, 600), &clients);
  }

  const std::vector<String>& Messages() { return chrome_client_->Messages(); }
  const std::vector<MessageSource>& MessageSources() {
    return chrome_client_->MessageSources();
  }

  LocalDOMWindow& Window() { return *page_holder_->GetFrame().DomWindow(); }

  Document& GetDocument() { return page_holder_->GetDocument(); }

  TouchEvent* EventWithDispatchType(WebInputEvent::DispatchType dispatch_type) {
    WebTouchEvent web_touch_event(WebInputEvent::kTouchStart, 0, 0);
    web_touch_event.dispatch_type = dispatch_type;
    return TouchEvent::Create(WebCoalescedInputEvent(web_touch_event), nullptr,
                              nullptr, nullptr, "touchstart", &Window(),
                              TouchAction::kTouchActionAuto);
  }

 private:
  Persistent<ConsoleCapturingChromeClient> chrome_client_;
  std::unique_ptr<DummyPageHolder> page_holder_;
};

TEST_F(TouchEventTest, PreventDefaultUncancelable) {
  TouchEvent* event = EventWithDispatchType(WebInputEvent::kEventNonBlocking);
  event->SetHandlingPassive(Event::PassiveMode::kNotPassiveDefault);

  EXPECT_THAT(Messages(), ElementsAre());
  event->preventDefault();
  EXPECT_THAT(Messages(),
              ElementsAre("Ignored attempt to cancel a touchstart event with "
                          "cancelable=false, for example because scrolling is "
                          "in progress and cannot be interrupted."));
  EXPECT_THAT(MessageSources(), ElementsAre(kJSMessageSource));

  EXPECT_TRUE(UseCounter::IsCounted(
      GetDocument(), WebFeature::kUncancelableTouchEventPreventDefaulted));
  EXPECT_FALSE(UseCounter::IsCounted(
      GetDocument(),
      WebFeature::
          kUncancelableTouchEventDueToMainThreadResponsivenessPreventDefaulted));
}

TEST_F(TouchEventTest,
       PreventDefaultUncancelableDueToMainThreadResponsiveness) {
  TouchEvent* event = EventWithDispatchType(
      WebInputEvent::kListenersForcedNonBlockingDueToMainThreadResponsiveness);
  event->SetHandlingPassive(Event::PassiveMode::kNotPassiveDefault);

  EXPECT_THAT(Messages(), ElementsAre());
  event->preventDefault();
  EXPECT_THAT(Messages(),
              ElementsAre("Ignored attempt to cancel a touchstart event with "
                          "cancelable=false. This event was forced to be "
                          "non-cancellable because the page was too busy to "
                          "handle the event promptly."));
  EXPECT_THAT(MessageSources(), ElementsAre(kInterventionMessageSource));

  EXPECT_TRUE(UseCounter::IsCounted(
      GetDocument(), WebFeature::kUncancelableTouchEventPreventDefaulted));
  EXPECT_TRUE(UseCounter::IsCounted(
      GetDocument(),
      WebFeature::
          kUncancelableTouchEventDueToMainThreadResponsivenessPreventDefaulted));
}

TEST_F(TouchEventTest,
       PreventDefaultPassiveDueToDocumentLevelScrollerIntervention) {
  TouchEvent* event =
      EventWithDispatchType(WebInputEvent::kListenersNonBlockingPassive);
  event->SetHandlingPassive(Event::PassiveMode::kPassiveForcedDocumentLevel);

  EXPECT_THAT(Messages(), ElementsAre());
  event->preventDefault();
  EXPECT_THAT(
      Messages(),
      ElementsAre("Unable to preventDefault inside passive event listener due "
                  "to target being treated as passive. See "
                  "https://www.chromestatus.com/features/5093566007214080"));
  EXPECT_THAT(MessageSources(), ElementsAre(kInterventionMessageSource));
}

class TouchEventTestNoFrame : public ::testing::Test {};

TEST_F(TouchEventTestNoFrame, PreventDefaultDoesntRequireFrame) {
  TouchEvent::Create()->preventDefault();
}

}  // namespace blink
