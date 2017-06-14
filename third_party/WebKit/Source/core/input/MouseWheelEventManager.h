// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MouseWheelEventManager_h
#define MouseWheelEventManager_h

#include "platform/heap/Handle.h"
#include "platform/heap/Visitor.h"
#include "public/platform/WebInputEventResult.h"

namespace blink {

class LocalFrame;
class ScrollManager;
class WebMouseWheelEvent;

class MouseWheelEventManager final
    : public GarbageCollectedFinalized<MouseWheelEventManager> {
  WTF_MAKE_NONCOPYABLE(MouseWheelEventManager);

 public:
  explicit MouseWheelEventManager(LocalFrame&, ScrollManager&);
  DECLARE_TRACE();

  WebInputEventResult HandleWheelEvent(const WebMouseWheelEvent&);

 private:
  const Member<LocalFrame> frame_;
  Member<ScrollManager> scroll_manager_;
};

}  // namespace blink

#endif  // MouseWheelEventManager_h
