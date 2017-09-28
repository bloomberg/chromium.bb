// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlSliderElement_h
#define MediaControlSliderElement_h

#include "modules/ModulesExport.h"
#include "modules/media_controls/elements/MediaControlInputElement.h"

namespace blink {

class MediaControlsImpl;
class ResizeObserver;

// MediaControlInputElement with additional logic for sliders.
class MODULES_EXPORT MediaControlSliderElement
    : public MediaControlInputElement {
  USING_GARBAGE_COLLECTED_MIXIN(MediaControlSliderElement);

 public:
  DECLARE_VIRTUAL_TRACE();

  // Stores the position of the segment in proportion from 0.0 to 1.0.
  struct Position {
    Position(double left, double width) : left(left), width(width){};
    double left;
    double width;
  };

 protected:
  class MediaControlSliderElementResizeObserverDelegate;

  MediaControlSliderElement(MediaControlsImpl&, MediaControlElementType);

  void SetupBarSegments();
  void SetBeforeSegmentPosition(Position);
  void SetAfterSegmentPosition(Position);

  void NotifyElementSizeChanged();

  // Width in CSS pixels * pageZoomFactor (ignores CSS transforms for
  // simplicity; deliberately ignores pinch zoom's pageScaleFactor).
  int Width();

 private:
  Position before_segment_position_;
  Position after_segment_position_;

  Member<HTMLDivElement> segment_highlight_before_;
  Member<HTMLDivElement> segment_highlight_after_;

  Member<ResizeObserver> resize_observer_;
};

}  // namespace blink

#endif  // MediaControlSliderElement_h
