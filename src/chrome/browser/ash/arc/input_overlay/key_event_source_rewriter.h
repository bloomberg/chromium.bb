// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_KEY_EVENT_SOURCE_REWRITER_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_KEY_EVENT_SOURCE_REWRITER_H_

#include "base/scoped_observation.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/event_source.h"

namespace aura {
class Window;
}  // namespace aura

namespace arc {
// KeyEventSourceRewriter forwards the key event from primary root window to the
// extended root window event source when the input-overlay enabled window is on
// the extended display.
class KeyEventSourceRewriter : public ui::EventRewriter {
 public:
  explicit KeyEventSourceRewriter(aura::Window* top_level_window);
  KeyEventSourceRewriter(const KeyEventSourceRewriter&) = delete;
  KeyEventSourceRewriter& operator=(const KeyEventSourceRewriter&) = delete;
  ~KeyEventSourceRewriter() override;

  // ui::EventRewriter:
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

 private:
  aura::Window* top_level_window_;
  base::ScopedObservation<ui::EventSource,
                          ui::EventRewriter,
                          &ui::EventSource::AddEventRewriter,
                          &ui::EventSource::RemoveEventRewriter>
      observation_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_KEY_EVENT_SOURCE_REWRITER_H_
