// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_EVENT_REWRITER_CONTINUATION_H_
#define UI_EVENTS_EVENT_REWRITER_CONTINUATION_H_

namespace ui {

struct EventDispatchDetails;

// Used to forward events from an EventRewriter. This is expected to be used
// only by cooperation between EventSource and EventRewriter. The methods are
// implemented by a subclass in EventSource, and called only via the
// corresponding EventRewriter functions (which validate the continuation).
class EventRewriterContinuation {
 public:
  EventRewriterContinuation() = default;
  virtual ~EventRewriterContinuation() = default;

  // Send an event to the sink, via any later rewriters.
  virtual EventDispatchDetails SendEvent(const Event* event)
      WARN_UNUSED_RESULT = 0;

  // Send an event directly to the sink, bypassing any later rewriters.
  virtual EventDispatchDetails SendEventFinally(const Event* event)
      WARN_UNUSED_RESULT = 0;

  // Discard an event, bypassing any later rewriters.
  virtual EventDispatchDetails DiscardEvent() WARN_UNUSED_RESULT = 0;
};

}  // namespace ui

#endif  // UI_EVENTS_EVENT_REWRITER_CONTINUATION_H_
