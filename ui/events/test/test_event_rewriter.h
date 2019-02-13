// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TEST_TEST_EVENT_REWRITER_H_
#define UI_EVENTS_TEST_TEST_EVENT_REWRITER_H_

#include "base/macros.h"
#include "ui/events/event_rewriter.h"

namespace ui {
namespace test {

// Counts number of events observed.
class TestEventRewriter : public ui::EventRewriter {
 public:
  TestEventRewriter();
  ~TestEventRewriter() override;

  void clear_events_seen() { events_seen_ = 0; }
  int events_seen() const { return events_seen_; }

  // ui::EventRewriter:
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

 private:
  int events_seen_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestEventRewriter);
};

}  // namespace test
}  // namespace ui

#endif  // UI_EVENTS_TEST_TEST_EVENT_REWRITER_H_
