// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XRCanvasInputProvider_h
#define XRCanvasInputProvider_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"
#include "platform/transforms/TransformationMatrix.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class EventListener;
class HTMLCanvasElement;
class MouseEvent;
class XRInputSource;
class XRSession;

class XRCanvasInputProvider
    : public GarbageCollectedFinalized<XRCanvasInputProvider>,
      public TraceWrapperBase {
 public:
  XRCanvasInputProvider(XRSession*, HTMLCanvasElement*);
  virtual ~XRCanvasInputProvider();

  XRSession* session() const { return session_; }

  // Remove all event listeners.
  void Stop();

  bool ShouldProcessEvents();

  void OnClick(MouseEvent*);

  XRInputSource* GetInputSource();

  virtual void Trace(blink::Visitor*);
  virtual void TraceWrappers(const blink::ScriptWrappableVisitor*) const;
  const char* NameInHeapSnapshot() const override {
    return "XRCanvasInputProvider";
  }

 private:
  void UpdateInputSource(MouseEvent*);
  void ClearInputSource();

  const Member<XRSession> session_;
  Member<HTMLCanvasElement> canvas_;
  Member<EventListener> listener_;
  TraceWrapperMember<XRInputSource> input_source_;
};

}  // namespace blink

#endif  // XRCanvasInputProvider_h
