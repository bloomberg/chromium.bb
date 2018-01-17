// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlLoadingPanelElement_h
#define MediaControlLoadingPanelElement_h

#include "modules/ModulesExport.h"
#include "modules/media_controls/elements/MediaControlAnimationEventListener.h"
#include "modules/media_controls/elements/MediaControlDivElement.h"

namespace blink {

class ContainerNode;
class Element;
class HTMLDivElement;
class MediaControlsImpl;

// The loading panel shows the semi-transparent white mask and the transparent
// loading spinner.
class MODULES_EXPORT MediaControlLoadingPanelElement final
    : public MediaControlDivElement,
      public MediaControlAnimationEventListener::Observer {
  USING_GARBAGE_COLLECTED_MIXIN(MediaControlLoadingPanelElement);

 public:
  explicit MediaControlLoadingPanelElement(MediaControlsImpl&);

  // Update the state based on the Media Controls state.
  void UpdateDisplayState();

  // Inform the loading panel that the Media Controls have been hidden.
  void OnControlsHidden();
  // Inform the loading panel that the Media Controls have been shown.
  void OnControlsShown();

  void Trace(Visitor*);

 private:
  friend class MediaControlLoadingPanelElementTest;

  enum State {
    // The loading panel is hidden.
    kHidden,

    // The loading panel is shown and is playing the "spinner" animation.
    kPlaying,

    // The loading panel is playing the "cooldown" animation and will hide
    // automatically once it is complete.
    kCoolingDown,
  };

  // These are used by AnimationEventListener to notify of animation events.
  void OnAnimationEnd() override;
  void OnAnimationIteration() override;
  Element& WatchedAnimationElement() const override;

  // Hide the animation and clean up the shadow DOM.
  void HideAnimation();

  // This sets the "animation-iteration-count" CSS property on the mask
  // background elements.
  void SetAnimationIterationCount(const String&);

  // Sets the background color of the spinner to black if there is no poster
  // image or video frame available.
  void SetSpinnerClassIfNecessary();

  // The loading panel is only used once and has a lot of DOM elements so these
  // two functions will populate the shadow DOM or clean it if the panel is
  // hidden.
  void CleanupShadowDOM();
  void PopulateShadowDOM();

  // Cleans up the event listener when this element is removed from the DOM.
  void RemovedFrom(ContainerNode*) override;

  // This counts how many animation iterations the background elements have
  // played.
  int animation_count_ = 0;
  State state_ = State::kHidden;

  // Whether the Media Controls are hidden.
  bool controls_hidden_ = false;

  Member<MediaControlAnimationEventListener> event_listener_;
  Member<HTMLDivElement> spinner_;
  Member<HTMLDivElement> mask1_background_;
  Member<HTMLDivElement> mask2_background_;
};

}  // namespace blink

#endif  // MediaControlLoadingPanelElement_h
