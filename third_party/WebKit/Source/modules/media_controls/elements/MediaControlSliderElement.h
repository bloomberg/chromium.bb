// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlSliderElement_h
#define MediaControlSliderElement_h

#include "modules/ModulesExport.h"
#include "modules/media_controls/elements/MediaControlInputElement.h"

namespace blink {

class MediaControlsImpl;

// MediaControlInputElement with additional logic for sliders.
class MODULES_EXPORT MediaControlSliderElement
    : public MediaControlInputElement {
  USING_GARBAGE_COLLECTED_MIXIN(MediaControlSliderElement);

 public:
  DECLARE_VIRTUAL_TRACE();

 protected:
  MediaControlSliderElement(MediaControlsImpl&, MediaControlElementType);

  void SetupBarSegments();
  void SetBeforeSegmentPosition(int left, int width);
  void SetAfterSegmentPosition(int left, int width);

 private:
  Member<HTMLDivElement> segment_highlight_before_;
  Member<HTMLDivElement> segment_highlight_after_;
};

}  // namespace blink

#endif  // MediaControlSliderElement_h
