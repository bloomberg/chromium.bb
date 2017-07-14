// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlInputElement_h
#define MediaControlInputElement_h

#include "core/html/HTMLInputElement.h"
#include "modules/ModulesExport.h"
#include "modules/media_controls/elements/MediaControlElementBase.h"

namespace blink {

class MediaControlsImpl;

// MediaControlElementBase implementation based on an <input> element. Used by
// buttons and sliders.
class MODULES_EXPORT MediaControlInputElement : public HTMLInputElement,
                                                public MediaControlElementBase {
  USING_GARBAGE_COLLECTED_MIXIN(MediaControlInputElement);

 public:
  static bool ShouldRecordDisplayStates(const HTMLMediaElement&);

  // Creates an overflow menu element with the given button as a child.
  HTMLElement* CreateOverflowElement(MediaControlsImpl&,
                                     MediaControlInputElement*);

  // Implements MediaControlElementBase.
  void SetOverflowElementIsWanted(bool) final;
  void MaybeRecordDisplayed() final;

  DECLARE_VIRTUAL_TRACE();

 protected:
  MediaControlInputElement(MediaControlsImpl&, MediaControlElementType);

  // Returns a string that represents the button for metrics purposes. This
  // will be used as a suffix for histograms.
  virtual const char* GetNameForHistograms() const = 0;

  // Returns a string representation of the media control element.
  // Subclasses should override this method to return the string representation
  // of the overflow button.
  virtual WebLocalizedString::Name GetOverflowStringName() const;

  // Implements a default event handler to record interaction on click.
  void DefaultEventHandler(Event*) override;

  // Implements MediaControlElementBase.
  void UpdateShownState() final;

  // Updates the value of the Text string shown in the overflow menu.
  void UpdateOverflowString();

  // Record interaction if it wasn't recorded yet. It is used internally for
  // click events but also by some elements that have complex interaction logic.
  void MaybeRecordInteracted();

  // Returns whether this element is used for the overflow menu.
  bool IsOverflowElement() const;

 private:
  friend class MediaControlInputElementTest;

  virtual void UpdateDisplayType();

  bool IsMouseFocusable() const override;
  bool IsMediaControlElement() const final;

  // Returns a string representation of the media control element. Used for
  // the overflow menu.
  String GetOverflowMenuString() const;

  // Used for histograms, do not reorder.
  enum class CTREvent {
    kDisplayed = 0,
    kInteracted,
    kCount,
  };

  // Records the CTR event.
  void RecordCTREvent(CTREvent);

  // The copy of this element used for the overflow menu in the media controls.
  // Setting this pointer is optional so it may be null.
  Member<MediaControlInputElement> overflow_element_;

  // The text representation of the button within the overflow menu.
  Member<Text> overflow_menu_text_;

  // Keeps track if the button was created for the purpose of the overflow menu.
  bool is_overflow_element_ = false;

  // Keeps track of whether the display/interaction have been recorded for the
  // CTR metrics.
  bool display_recorded_ = false;
  bool interaction_recorded_ = false;
};

}  // namespace blink

#endif  // MediaControlInputElement_h
