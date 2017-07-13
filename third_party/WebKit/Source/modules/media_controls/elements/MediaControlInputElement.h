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

  // Implements MediaControlElementBase.
  void SetOverflowElementIsWanted(bool) final;

  DECLARE_VIRTUAL_TRACE();

 protected:
  MediaControlInputElement(MediaControlsImpl&, MediaControlElementType);

  // Returns a string representation of the media control element.
  // Subclasses should override this method to return the string representation
  // of the overflow button.
  virtual WebLocalizedString::Name GetOverflowStringName() const;

  // Implements MediaControlElementBase.
  void UpdateShownState() final;

  // Updates the value of the Text string shown in the overflow menu.
  void UpdateOverflowString();

 private:
  virtual void UpdateDisplayType();

  bool IsMouseFocusable() const override;
  bool IsMediaControlElement() const final;

  // Returns a string representation of the media control element. Used for
  // the overflow menu.
  String GetOverflowMenuString() const;

  // The copy of this element used for the overflow menu in the media controls.
  // Setting this pointer is optional so it may be null.
  Member<MediaControlInputElement> overflow_element_;

  // The text representation of the button within the overflow menu.
  Member<Text> overflow_menu_text_;

  // Keeps track if the button was created for the purpose of the overflow menu.
  bool is_overflow_element_ = false;
};

}  // namespace blink

#endif  // MediaControlInputElement_h
