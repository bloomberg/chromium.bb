// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlInputElement_h
#define MediaControlInputElement_h

#include "core/html/HTMLInputElement.h"
#include "modules/media_controls/elements/MediaControlElementBase.h"

namespace blink {

class MediaControlsImpl;

// MediaControlElementBase implementation based on an <input> element. Used by
// buttons and sliders.
class MediaControlInputElement : public HTMLInputElement,
                                 public MediaControlElementBase {
  USING_GARBAGE_COLLECTED_MIXIN(MediaControlInputElement);

 public:
  // Creates an overflow menu element with the given button as a child.
  HTMLElement* CreateOverflowElement(MediaControlsImpl&,
                                     MediaControlInputElement*);

  DECLARE_VIRTUAL_TRACE();

 protected:
  MediaControlInputElement(MediaControlsImpl&, MediaControlElementType);

 private:
  virtual void UpdateDisplayType();

  bool IsMouseFocusable() const override;
  bool IsMediaControlElement() const final;
};

}  // namespace blink

#endif  // MediaControlInputElement_h
