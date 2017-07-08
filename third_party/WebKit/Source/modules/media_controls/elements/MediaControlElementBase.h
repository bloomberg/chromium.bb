// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlElementBase_h
#define MediaControlElementBase_h

#include "core/dom/Element.h"
#include "modules/media_controls/elements/MediaControlElementType.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/Visitor.h"
#include "public/platform/WebLocalizedString.h"

namespace blink {

class Element;
class HTMLElement;
class HTMLMediaElement;
class MediaControlsImpl;

// MediaControlElementBase is the base class for all the media control elements.
// It is sub-classed by MediaControlInputElement and MediaControlDivElement
// which are then used by the final implementations.
class MediaControlElementBase : public GarbageCollectedMixin {
 public:
  // These hold the state about whether this control should be shown if
  // space permits.  These will also show / hide as needed.
  virtual void SetIsWanted(bool);
  bool IsWanted() const;

  // Tell us whether we fit or not.  This will hide / show the control as
  // needed, also.
  void SetDoesFit(bool);

  // Returns the display type of the element that is set at creation.
  MediaControlElementType DisplayType() const;

  // By default, media controls elements are not added to the overflow menu.
  // Controls that can be added to the overflow menu should override this
  // function and return true.
  virtual bool HasOverflowButton() const;

  // If true, shows the overflow menu item if it exists. Hides it if false.
  void ShouldShowButtonInOverflowMenu(bool);

  // Returns a string representation of the media control element. Used for
  // the overflow menu.
  String GetOverflowMenuString() const;

  // Updates the value of the Text string shown in the overflow menu.
  void UpdateOverflowString();

  DECLARE_VIRTUAL_TRACE();

 protected:
  MediaControlElementBase(MediaControlsImpl&,
                          MediaControlElementType,
                          HTMLElement*);

  MediaControlsImpl& GetMediaControls() const;

  HTMLMediaElement& MediaElement() const;

  void SetDisplayType(MediaControlElementType);

  // Represents the overflow menu element for this media control.
  // The Element contains the button that the user can click on, but having
  // the button within an Element enables us to style the overflow menu.
  // Setting this pointer is optional so it may be null.
  Member<Element> overflow_menu_element_;

  // The text representation of the button within the overflow menu.
  Member<Text> overflow_menu_text_;

 private:
  // Hide or show based on our fits / wanted state.  We want to show
  // if and only if we're wanted and we fit.
  void UpdateShownState();

  // Returns a string representation of the media control element.
  // Subclasses should override this method to return the string representation
  // of the overflow button.
  virtual WebLocalizedString::Name GetOverflowStringName() const;

  Member<MediaControlsImpl> media_controls_;
  MediaControlElementType display_type_;
  Member<HTMLElement> element_;
  bool is_wanted_ : 1;
  bool does_fit_ : 1;
};

}  // namespace blink

#endif  // MediaControlElementBase_h
