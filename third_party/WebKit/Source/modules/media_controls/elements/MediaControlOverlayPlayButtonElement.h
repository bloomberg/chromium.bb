// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlOverlayPlayButtonElement_h
#define MediaControlOverlayPlayButtonElement_h

#include "core/html/HTMLDivElement.h"
#include "modules/ModulesExport.h"
#include "modules/media_controls/elements/MediaControlAnimationEventListener.h"
#include "modules/media_controls/elements/MediaControlInputElement.h"
#include "platform/Timer.h"

namespace WTF {
class AtomicString;
}

namespace blink {

class ContainerNode;
class Event;
class MediaControlsImpl;

class MODULES_EXPORT MediaControlOverlayPlayButtonElement final
    : public MediaControlInputElement {
 public:
  explicit MediaControlOverlayPlayButtonElement(MediaControlsImpl&);

  // MediaControlInputElement overrides.
  void UpdateDisplayType() override;

  WebSize GetSizeOrDefault() const final;

  void Trace(blink::Visitor*) override;

 protected:
  const char* GetNameForHistograms() const override;

 private:
  friend class MediaControlOverlayPlayButtonElementTest;

  // This class is responible for displaying the arrow animation when a jump is
  // triggered by the user.
  class MODULES_EXPORT AnimatedArrow final
      : public HTMLDivElement,
        public MediaControlAnimationEventListener::Observer {
    USING_GARBAGE_COLLECTED_MIXIN(AnimatedArrow);

   public:
    AnimatedArrow(const AtomicString& id, ContainerNode& parent);

    // MediaControlAnimationEventListener::Observer overrides
    void OnAnimationIteration() override;
    void OnAnimationEnd() override{};
    Element& WatchedAnimationElement() const override;

    // Shows the animated arrows for a single animation iteration. If the
    // arrows are already shown it will show them for another animation
    // iteration.
    void Show();

    void Trace(Visitor*);

   private:
    int counter_ = 0;
    bool hidden_ = true;

    Member<Element> last_arrow_;
    Member<Element> svg_container_;
    Member<MediaControlAnimationEventListener> event_listener_;
  };

  void TapTimerFired(TimerBase*);

  void DefaultEventHandler(Event*) override;
  bool KeepEventInNode(Event*) override;

  void MaybePlayPause();
  void MaybeJump(int);

  TaskRunnerTimer<MediaControlOverlayPlayButtonElement> tap_timer_;

  Member<HTMLDivElement> internal_button_;
  Member<AnimatedArrow> left_jump_arrow_;
  Member<AnimatedArrow> right_jump_arrow_;
};

}  // namespace blink

#endif  // MediaControlOverlayPlayButtonElement_h
