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
class HTMLDivElement;
class MediaControlsImpl;

class MediaControlTimelineElement : public MediaControlSliderElement {
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

  // Inform the timeline that the Media Controls have been shown or hidden.
  void OnControlsShown();
  void OnControlsHidden();

  virtual void Trace(blink::Visitor*);

 protected:
  const char* GetNameForHistograms() const override;

 private:
  void DefaultEventHandler(Event*) override;
  bool KeepEventInNode(Event*) override;

  // Checks if we can begin or end a scrubbing event. If the event is a pointer
  // event then it needs to start and end with valid pointer events. If the
  // event is a pointer event followed by a touch event then it can only be
  // ended when the touch has ended.
  bool BeginScrubbingEvent(Event&);
  bool EndScrubbingEvent(Event&);

  MediaControlTimelineMetrics metrics_;

  bool is_touching_ = false;

  bool controls_hidden_ = false;
};

}  // namespace blink

#endif  // MediaControlTimelineElement
