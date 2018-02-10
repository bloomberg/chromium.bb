// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorVR_h
#define NavigatorVR_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/Navigator.h"
#include "core/page/FocusChangedObserver.h"
#include "modules/ModulesExport.h"
#include "modules/vr/VRDisplay.h"
#include "modules/vr/VRDisplayEvent.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class Document;
class Navigator;
class XR;
class VRController;

class MODULES_EXPORT NavigatorVR final
    : public GarbageCollectedFinalized<NavigatorVR>,
      public Supplement<Navigator>,
      public LocalDOMWindow::EventListenerObserver,
      public FocusChangedObserver {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorVR);
  WTF_MAKE_NONCOPYABLE(NavigatorVR);

 public:
  static const char kSupplementName[];

  static NavigatorVR* From(Document&);
  static NavigatorVR& From(Navigator&);
  ~NavigatorVR() override;

  // XR API
  // TODO(offenwanger) Should eventually move this out into it's own separate
  // Navigator supplement.
  static XR* xr(Navigator&);
  XR* xr();

  // Legacy API
  static ScriptPromise getVRDisplays(ScriptState*, Navigator&);
  ScriptPromise getVRDisplays(ScriptState*);

  VRController* Controller();
  Document* GetDocument();
  bool IsFocused() { return focused_; }

  // Queues up event to be fired soon.
  void EnqueueVREvent(VRDisplayEvent*);

  // Dispatches an event immediately.
  void DispatchVREvent(VRDisplayEvent*);

  // Inherited from FocusChangedObserver.
  void FocusedFrameChanged() override;

  // Inherited from LocalDOMWindow::EventListenerObserver.
  void DidAddEventListener(LocalDOMWindow*, const AtomicString&) override;
  void DidRemoveEventListener(LocalDOMWindow*, const AtomicString&) override;
  void DidRemoveAllEventListeners(LocalDOMWindow*) override;

  void Trace(blink::Visitor*) override;

 private:
  friend class VRDisplay;
  friend class VRGetDevicesCallback;

  explicit NavigatorVR(Navigator&);

  void FireVRDisplayPresentChange(VRDisplay*);

  Member<XR> xr_;
  Member<VRController> controller_;

  // Whether this page is listening for vrdisplayactivate event.
  bool listening_for_activate_ = false;
  bool focused_ = false;

  // Metrics data - indicates whether we've already measured this data so we
  // don't do it every frame.
  bool did_log_getVRDisplays_ = false;
};

}  // namespace blink

#endif  // NavigatorVR_h
