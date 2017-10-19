// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlElementBase_h
#define MediaControlElementBase_h

#include "core/dom/Element.h"
#include "modules/ModulesExport.h"
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
class MODULES_EXPORT MediaControlElementBase : public GarbageCollectedMixin {
 public:
  // These hold the state about whether this control should be shown if
  // space permits.  These will also show / hide as needed.
  void SetIsWanted(bool);
  bool IsWanted() const;

  // Tell us whether we fit or not.  This will hide / show the control as
  // needed, also.
  void SetDoesFit(bool);
  bool DoesFit() const;

  // Similar to SetIsWanted() for the overflow element associated to the current
  // element. Will be a no-op if the element does not have an associated
  // overflow element.
  virtual void SetOverflowElementIsWanted(bool) = 0;

  // Called when recording the display state of the media control element should
  // happen. It will be a no-op if the element isn't displayed in the controls.
  virtual void MaybeRecordDisplayed() = 0;

  // Returns the display type of the element that is set at creation.
  MediaControlElementType DisplayType() const;

  // By default, media controls elements are not added to the overflow menu.
  // Controls that can be added to the overflow menu should override this
  // function and return true.
  virtual bool HasOverflowButton() const;

  virtual void Trace(blink::Visitor*);

 protected:
  MediaControlElementBase(MediaControlsImpl&,
                          MediaControlElementType,
                          HTMLElement*);

  // Hide or show based on our fits / wanted state.  We want to show
  // if and only if we're wanted and we fit.
  virtual void UpdateShownState();

  MediaControlsImpl& GetMediaControls() const;

  HTMLMediaElement& MediaElement() const;

  void SetDisplayType(MediaControlElementType);

 private:
  Member<MediaControlsImpl> media_controls_;
  MediaControlElementType display_type_;
  Member<HTMLElement> element_;
  bool is_wanted_ : 1;
  bool does_fit_ : 1;
};

}  // namespace blink

#endif  // MediaControlElementBase_h
