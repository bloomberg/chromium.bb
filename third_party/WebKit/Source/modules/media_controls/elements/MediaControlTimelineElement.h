// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlTimelineElement_h
#define MediaControlTimelineElement_h

#include "modules/media_controls/elements/MediaControlInputElement.h"
#include "modules/media_controls/elements/MediaControlTimelineMetrics.h"

namespace blink {

class Event;
class MediaControlsImpl;

class MediaControlTimelineElement final : public MediaControlInputElement {
 public:
  explicit MediaControlTimelineElement(MediaControlsImpl&);

  // MediaControlInputElement overrides.
  bool WillRespondToMouseClickEvents() override;

  // FIXME: An "earliest possible position" will be needed once that concept
  // is supported by HTMLMediaElement, see https://crbug.com/137275
  void SetPosition(double);
  void SetDuration(double);

  void OnPlaying();

  void OnMediaKeyboardEvent(Event* event) { DefaultEventHandler(event); }

 protected:
  const char* GetNameForHistograms() const override;

 private:
  void DefaultEventHandler(Event*) override;
  bool KeepEventInNode(Event*) override;

  // Width in CSS pixels * pageZoomFactor (ignores CSS transforms for
  // simplicity; deliberately ignores pinch zoom's pageScaleFactor).
  int TimelineWidth();

  MediaControlTimelineMetrics metrics_;
};

}  // namespace blink

#endif  // MediaControlTimelineElement
