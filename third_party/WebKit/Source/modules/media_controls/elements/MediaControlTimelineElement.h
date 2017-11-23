// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlTimelineElement_h
#define MediaControlTimelineElement_h

#include "modules/ModulesExport.h"
#include "modules/media_controls/elements/MediaControlSliderElement.h"
#include "modules/media_controls/elements/MediaControlTimelineMetrics.h"

namespace blink {

class Event;
class MediaControlsImpl;

class MediaControlTimelineElement final : public MediaControlSliderElement {
 public:
  MODULES_EXPORT explicit MediaControlTimelineElement(MediaControlsImpl&);

  // MediaControlInputElement overrides.
  bool WillRespondToMouseClickEvents() override;

  // FIXME: An "earliest possible position" will be needed once that concept
  // is supported by HTMLMediaElement, see https://crbug.com/137275
  void SetPosition(double);
  void SetDuration(double);

  void OnPlaying();

  void OnMediaKeyboardEvent(Event* event) { DefaultEventHandler(event); }

  void RenderBarSegments();

 protected:
  const char* GetNameForHistograms() const override;

 private:
  void DefaultEventHandler(Event*) override;
  bool KeepEventInNode(Event*) override;

  MediaControlTimelineMetrics metrics_;
};

}  // namespace blink

#endif  // MediaControlTimelineElement
