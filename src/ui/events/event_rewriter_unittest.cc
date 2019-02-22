// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_rewriter.h"

#include <list>
#include <map>
#include <set>
#include <utility>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/test_event_processor.h"

namespace ui {

namespace {

// TestEventRewriteProcessor is set up with a sequence of event types,
// and fails if the events received via OnEventFromSource() do not match
// this sequence. These expected event types are consumed on receipt.
class TestEventRewriteProcessor : public test::TestEventProcessor {
 public:
  TestEventRewriteProcessor() {}
  ~TestEventRewriteProcessor() override { CheckAllReceived(); }

  void AddExpectedEvent(EventType type) { expected_events_.push_back(type); }
  // Test that all expected events have been received.
  void CheckAllReceived() { EXPECT_TRUE(expected_events_.empty()); }

  // EventProcessor:
  EventDispatchDetails OnEventFromSource(Event* event) override {
    EXPECT_FALSE(expected_events_.empty());
    EXPECT_EQ(expected_events_.front(), event->type());
    expected_events_.pop_front();
    return EventDispatchDetails();
  }

 private:
  std::list<EventType> expected_events_;
  DISALLOW_COPY_AND_ASSIGN(TestEventRewriteProcessor);
};

std::unique_ptr<Event> CreateEventForType(EventType type) {
  switch (type) {
    case ET_CANCEL_MODE:
      return std::make_unique<CancelModeEvent>();
    case ET_MOUSE_DRAGGED:
    case ET_MOUSE_PRESSED:
    case ET_MOUSE_RELEASED:
      return std::make_unique<MouseEvent>(type, gfx::Point(), gfx::Point(),
                                          base::TimeTicks::Now(), 0, 0);
    case ET_KEY_PRESSED:
    case ET_KEY_RELEASED:
      return std::make_unique<KeyEvent>(type, ui::VKEY_TAB, DomCode::NONE, 0);
    case ET_SCROLL_FLING_CANCEL:
    case ET_SCROLL_FLING_START:
      return std::make_unique<ScrollEvent>(
          type, gfx::Point(), base::TimeTicks::Now(), 0, 0, 0, 0, 0, 0);
    default:
      NOTREACHED() << type;
      return nullptr;
  }
}

// Trivial EventSource that does nothing but send events.
class TestEventRewriteSource : public EventSource {
 public:
  explicit TestEventRewriteSource(EventProcessor* processor)
      : processor_(processor) {}
  EventProcessor* GetEventSink() override { return processor_; }
  void Send(EventType type) {
    auto event = CreateEventForType(type);
    SendEventToSink(event.get());
  }

 private:
  EventProcessor* processor_;
};

// This EventRewriter always returns the same status, and if rewriting, the
// same event type; it is used to test simple rewriting, and rewriter addition,
// removal, and sequencing. Consequently EVENT_REWRITE_DISPATCH_ANOTHER is not
// supported here (calls to NextDispatchEvent() would continue indefinitely).
class TestConstantEventRewriter : public EventRewriter {
 public:
  TestConstantEventRewriter(EventRewriteStatus status, EventType type)
      : status_(status), type_(type) {
    CHECK_NE(EVENT_REWRITE_DISPATCH_ANOTHER, status);
  }

  EventRewriteStatus RewriteEvent(
      const Event& event,
      std::unique_ptr<Event>* rewritten_event) override {
    if (status_ == EVENT_REWRITE_REWRITTEN)
      *rewritten_event = CreateEventForType(type_);
    return status_;
  }
  EventRewriteStatus NextDispatchEvent(
      const Event& last_event,
      std::unique_ptr<Event>* new_event) override {
    NOTREACHED();
    return status_;
  }

 private:
  EventRewriteStatus status_;
  EventType type_;
};

// This EventRewriter runs a simple state machine; it is used to test
// EVENT_REWRITE_DISPATCH_ANOTHER.
class TestStateMachineEventRewriter : public EventRewriter {
 public:
  TestStateMachineEventRewriter() : last_rewritten_event_(0), state_(0) {}
  void AddRule(int from_state, EventType from_type,
               int to_state, EventType to_type, EventRewriteStatus to_status) {
    RewriteResult r = {to_state, to_type, to_status};
    rules_.insert(std::pair<RewriteCase, RewriteResult>(
        RewriteCase(from_state, from_type), r));
  }
  EventRewriteStatus RewriteEvent(
      const Event& event,
      std::unique_ptr<Event>* rewritten_event) override {
    auto find = rules_.find(RewriteCase(state_, event.type()));
    if (find == rules_.end())
      return EVENT_REWRITE_CONTINUE;
    if ((find->second.status == EVENT_REWRITE_REWRITTEN) ||
        (find->second.status == EVENT_REWRITE_DISPATCH_ANOTHER)) {
      *rewritten_event = CreateEventForType(find->second.type);
      last_rewritten_event_ = rewritten_event->get();
    } else {
      last_rewritten_event_ = nullptr;
    }
    state_ = find->second.state;
    return find->second.status;
  }
  EventRewriteStatus NextDispatchEvent(
      const Event& last_event,
      std::unique_ptr<Event>* new_event) override {
    EXPECT_TRUE(last_rewritten_event_);
    EXPECT_EQ(last_rewritten_event_, &last_event);
    EXPECT_FALSE(new_event->get() && new_event->get() == &last_event);
    return RewriteEvent(last_event, new_event);
  }

 private:
  typedef std::pair<int, EventType> RewriteCase;
  struct RewriteResult {
    int state;
    EventType type;
    EventRewriteStatus status;
  };
  typedef std::map<RewriteCase, RewriteResult> RewriteRules;
  RewriteRules rules_;
  Event* last_rewritten_event_;
  int state_;
};

}  // namespace

TEST(EventRewriterTest, EventRewriting) {
  // TestEventRewriter r0 always rewrites events to ET_CANCEL_MODE;
  // it is placed at the beginning of the chain and later removed,
  // to verify that rewriter removal works.
  TestConstantEventRewriter r0(EVENT_REWRITE_REWRITTEN, ET_CANCEL_MODE);

  // TestEventRewriter r1 always returns EVENT_REWRITE_CONTINUE;
  // it is placed at the beginning of the chain to verify that a
  // later rewriter sees the events.
  TestConstantEventRewriter r1(EVENT_REWRITE_CONTINUE, ET_UNKNOWN);

  // TestEventRewriter r2 has a state machine, primarily to test
  // |EVENT_REWRITE_DISPATCH_ANOTHER|.
  TestStateMachineEventRewriter r2;

  // TestEventRewriter r3 always rewrites events to ET_CANCEL_MODE;
  // it is placed at the end of the chain to verify that previously
  // rewritten events are not passed further down the chain.
  TestConstantEventRewriter r3(EVENT_REWRITE_REWRITTEN, ET_CANCEL_MODE);

  TestEventRewriteProcessor p;
  TestEventRewriteSource s(&p);
  s.AddEventRewriter(&r0);
  s.AddEventRewriter(&r1);
  s.AddEventRewriter(&r2);

  // These events should be rewritten by r0 to ET_CANCEL_MODE.
  p.AddExpectedEvent(ET_CANCEL_MODE);
  s.Send(ET_MOUSE_DRAGGED);
  p.AddExpectedEvent(ET_CANCEL_MODE);
  s.Send(ET_MOUSE_PRESSED);
  p.CheckAllReceived();

  // Remove r0, and verify that it's gone and that events make it through.
  s.AddEventRewriter(&r3);
  s.RemoveEventRewriter(&r0);
  r2.AddRule(0, ET_SCROLL_FLING_START,
             0, ET_SCROLL_FLING_CANCEL, EVENT_REWRITE_REWRITTEN);
  p.AddExpectedEvent(ET_SCROLL_FLING_CANCEL);
  s.Send(ET_SCROLL_FLING_START);
  p.CheckAllReceived();
  s.RemoveEventRewriter(&r3);

  // Verify EVENT_REWRITE_DISPATCH_ANOTHER using a state machine
  // (that happens to be analogous to sticky keys).
  r2.AddRule(0, ET_KEY_PRESSED,
             1, ET_KEY_PRESSED, EVENT_REWRITE_CONTINUE);
  r2.AddRule(1, ET_MOUSE_PRESSED,
             0, ET_MOUSE_PRESSED, EVENT_REWRITE_CONTINUE);
  r2.AddRule(1, ET_KEY_RELEASED,
             2, ET_KEY_RELEASED, EVENT_REWRITE_DISCARD);
  r2.AddRule(2, ET_MOUSE_RELEASED,
             3, ET_MOUSE_RELEASED, EVENT_REWRITE_DISPATCH_ANOTHER);
  r2.AddRule(3, ET_MOUSE_RELEASED,
             0, ET_KEY_RELEASED, EVENT_REWRITE_REWRITTEN);
  p.AddExpectedEvent(ET_KEY_PRESSED);
  s.Send(ET_KEY_PRESSED);
  s.Send(ET_KEY_RELEASED);
  p.AddExpectedEvent(ET_MOUSE_PRESSED);
  s.Send(ET_MOUSE_PRESSED);

  // Removing rewriters r1 and r3 shouldn't affect r2.
  s.RemoveEventRewriter(&r1);
  s.RemoveEventRewriter(&r3);

  // Continue with the state-based rewriting.
  p.AddExpectedEvent(ET_MOUSE_RELEASED);
  p.AddExpectedEvent(ET_KEY_RELEASED);
  s.Send(ET_MOUSE_RELEASED);
  p.CheckAllReceived();
}

}  // namespace ui
