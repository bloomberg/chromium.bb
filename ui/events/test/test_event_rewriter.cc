// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/test/test_event_rewriter.h"

#include "ui/events/event.h"

namespace ui {
namespace test {

TestEventRewriter::TestEventRewriter() = default;

TestEventRewriter::~TestEventRewriter() = default;

ui::EventRewriteStatus TestEventRewriter::RewriteEvent(
    const ui::Event& event,
    std::unique_ptr<ui::Event>* new_event) {
  ++events_seen_;
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus TestEventRewriter::NextDispatchEvent(
    const ui::Event& last_event,
    std::unique_ptr<ui::Event>* new_event) {
  return ui::EVENT_REWRITE_CONTINUE;
}

}  // namespace test
}  // namespace ui
