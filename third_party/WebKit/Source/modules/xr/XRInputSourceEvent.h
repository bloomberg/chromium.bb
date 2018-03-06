// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XRInputSourceEvent_h
#define XRInputSourceEvent_h

#include "modules/EventModules.h"
#include "modules/xr/XRInputSource.h"
#include "modules/xr/XRInputSourceEventInit.h"
#include "modules/xr/XRPresentationFrame.h"

namespace blink {

class XRInputSourceEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static XRInputSourceEvent* Create() { return new XRInputSourceEvent; }
  static XRInputSourceEvent* Create(const AtomicString& type,
                                    XRPresentationFrame* frame,
                                    XRInputSource* input_source) {
    return new XRInputSourceEvent(type, frame, input_source);
  }

  static XRInputSourceEvent* Create(const AtomicString& type,
                                    const XRInputSourceEventInit& initializer) {
    return new XRInputSourceEvent(type, initializer);
  }

  ~XRInputSourceEvent() override;

  XRPresentationFrame* frame() const { return frame_.Get(); }
  XRInputSource* inputSource() const { return input_source_.Get(); }

  const AtomicString& InterfaceName() const override;

  virtual void Trace(blink::Visitor*);

 private:
  XRInputSourceEvent();
  XRInputSourceEvent(const AtomicString& type,
                     XRPresentationFrame*,
                     XRInputSource*);
  XRInputSourceEvent(const AtomicString&, const XRInputSourceEventInit&);

  Member<XRPresentationFrame> frame_;
  Member<XRInputSource> input_source_;
};

}  // namespace blink

#endif  // XRInputSourceEvent_h
