/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef NavigatorGamepad_h
#define NavigatorGamepad_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/Navigator.h"
#include "core/frame/PlatformEventController.h"
#include "modules/ModulesExport.h"
#include "platform/AsyncMethodRunner.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Document;
class Gamepad;
class GamepadList;
class Navigator;

class MODULES_EXPORT NavigatorGamepad final
    : public GarbageCollectedFinalized<NavigatorGamepad>,
      public Supplement<Navigator>,
      public DOMWindowClient,
      public PlatformEventController,
      public LocalDOMWindow::EventListenerObserver {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorGamepad);

 public:
  static NavigatorGamepad* From(Document&);
  static NavigatorGamepad& From(Navigator&);
  ~NavigatorGamepad() override;

  static GamepadList* getGamepads(Navigator&);
  GamepadList* Gamepads();

  virtual void Trace(blink::Visitor*);

 private:
  explicit NavigatorGamepad(Navigator&);

  static const char* SupplementName();

  void DispatchOneEvent();
  void DidRemoveGamepadEventListeners();
  bool StartUpdatingIfAttached();
  void SampleAndCheckConnectedGamepads();
  bool CheckConnectedGamepads(GamepadList*, GamepadList*);

  // PageVisibilityObserver
  void PageVisibilityChanged() override;

  // PlatformEventController
  void RegisterWithDispatcher() override;
  void UnregisterWithDispatcher() override;
  bool HasLastData() override;
  void DidUpdateData() override;

  // LocalDOMWindow::EventListenerObserver
  void DidAddEventListener(LocalDOMWindow*, const AtomicString&) override;
  void DidRemoveEventListener(LocalDOMWindow*, const AtomicString&) override;
  void DidRemoveAllEventListeners(LocalDOMWindow*) override;

  Member<GamepadList> gamepads_;
  Member<GamepadList> gamepads_back_;
  HeapDeque<Member<Gamepad>> pending_events_;
  Member<AsyncMethodRunner<NavigatorGamepad>> dispatch_one_event_runner_;
};

}  // namespace blink

#endif  // NavigatorGamepad_h
