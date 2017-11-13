// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlAnimationEventListener_h
#define MediaControlAnimationEventListener_h

#include "core/dom/events/EventListener.h"
#include "modules/ModulesExport.h"
#include "platform/heap/GarbageCollected.h"

namespace blink {

class Element;
class ExecutionContext;
class Event;

// Listens for animationend and animationiteration DOM events on a HTML element
// provided by the loading panel. When the events are called it calls the
// OnAnimation* methods on the loading panel.
//
// This exists because we need to know when the animation ends so we can reset
// the element and we also need to keep track of how many iterations the
// animation has gone through so we can nicely stop the animation at the end of
// the current one.
class MODULES_EXPORT MediaControlAnimationEventListener final
    : public EventListener {
 public:
  // To use this class you need to use Observer as a mixin and return an element
  // to watch. You then instanitate a MediaControlAnimationEventListener from
  // the class pointing to your observer. The listener will call the On*
  // functions when events are triggered.
  class MODULES_EXPORT Observer : public GarbageCollectedMixin {
   public:
    // The watched element has completed an iteration of an animation.
    virtual void OnAnimationIteration() = 0;

    // The watched element has finished all iterations of an animation.
    virtual void OnAnimationEnd() = 0;

    // This is the element to watch for animation events.
    virtual Element& WatchedAnimationElement() const = 0;

    void Trace(blink::Visitor*) override;
  };

  explicit MediaControlAnimationEventListener(Observer*);
  void Detach();

  bool operator==(const EventListener& other) const override;

  void Trace(Visitor*) override;

 private:
  void handleEvent(ExecutionContext*, Event*);

  Member<Observer> observer_;
};

}  // namespace blink

#endif  // MediaControlAnimationEventListener_h
