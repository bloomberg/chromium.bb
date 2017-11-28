// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MouseWheelEventManager_h
#define MouseWheelEventManager_h

#include "base/macros.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Visitor.h"
#include "public/platform/WebInputEventResult.h"

namespace blink {

class Document;
class LocalFrame;
class LocalFrameView;
class Node;
class WebMouseWheelEvent;

class MouseWheelEventManager final
    : public GarbageCollectedFinalized<MouseWheelEventManager> {
 public:
  explicit MouseWheelEventManager(LocalFrame&);
  void Trace(blink::Visitor*);

  void Clear();

  WebInputEventResult HandleWheelEvent(const WebMouseWheelEvent&);

  void ElementRemoved(Node* target);

 private:
  Node* FindTargetNode(const WebMouseWheelEvent&,
                       const Document*,
                       const LocalFrameView*);

  const Member<LocalFrame> frame_;
  Member<Node> wheel_target_;

  DISALLOW_COPY_AND_ASSIGN(MouseWheelEventManager);
};

}  // namespace blink

#endif  // MouseWheelEventManager_h
