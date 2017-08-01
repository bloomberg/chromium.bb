/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef ScriptedAnimationController_h
#define ScriptedAnimationController_h

#include "core/CoreExport.h"
#include "core/dom/FrameRequestCallbackCollection.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/ListHashSet.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/StringImpl.h"

namespace blink {

class Document;
class Event;
class EventTarget;
class FrameRequestCallback;
class MediaQueryListListener;

class CORE_EXPORT ScriptedAnimationController
    : public GarbageCollectedFinalized<ScriptedAnimationController> {
 public:
  static ScriptedAnimationController* Create(Document* document) {
    return new ScriptedAnimationController(document);
  }

  DECLARE_TRACE();
  void ClearDocumentPointer() { document_ = nullptr; }

  // Animation frame callbacks are used for requestAnimationFrame().
  typedef int CallbackId;
  CallbackId RegisterCallback(FrameRequestCallback*);
  void CancelCallback(CallbackId);

  // Animation frame events are used for resize events, scroll events, etc.
  void EnqueueEvent(Event*);
  void EnqueuePerFrameEvent(Event*);

  // Animation frame tasks are used for Fullscreen.
  void EnqueueTask(WTF::Closure);

  // Used for the MediaQueryList change event.
  void EnqueueMediaQueryChangeListeners(
      HeapVector<Member<MediaQueryListListener>>&);

  // Invokes callbacks, dispatches events, etc. The order is defined by HTML:
  // https://html.spec.whatwg.org/multipage/webappapis.html#event-loop-processing-model
  void ServiceScriptedAnimations(double monotonic_time_now);

  void Suspend();
  void Resume();

  void DispatchEventsAndCallbacksForPrinting();

 private:
  explicit ScriptedAnimationController(Document*);

  void ScheduleAnimationIfNeeded();

  void RunTasks();
  void DispatchEvents(
      const AtomicString& event_interface_filter = AtomicString());
  void ExecuteCallbacks(double monotonic_time_now);
  void CallMediaQueryListListeners();

  bool HasScheduledItems() const;

  Member<Document> document_;
  FrameRequestCallbackCollection callback_collection_;
  int suspend_count_;
  Vector<WTF::Closure> task_queue_;
  HeapVector<Member<Event>> event_queue_;
  HeapListHashSet<std::pair<Member<const EventTarget>, const StringImpl*>>
      per_frame_events_;
  using MediaQueryListListeners =
      HeapListHashSet<Member<MediaQueryListListener>>;
  MediaQueryListListeners media_query_list_listeners_;
};

}  // namespace blink

#endif  // ScriptedAnimationController_h
