// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EventHandlingUtil_h
#define EventHandlingUtil_h

#include "core/frame/LocalFrame.h"
#include "core/layout/HitTestResult.h"
#include "core/page/EventWithHitTestResults.h"
#include "platform/geometry/LayoutPoint.h"
#include "public/platform/WebInputEventResult.h"

namespace blink {

class LocalFrame;
class ScrollableArea;
class PaintLayer;

namespace EventHandlingUtil {

HitTestResult HitTestResultInFrame(
    LocalFrame*,
    const LayoutPoint&,
    HitTestRequest::HitTestRequestType hit_type = HitTestRequest::kReadOnly |
                                                  HitTestRequest::kActive);

WebInputEventResult MergeEventResult(WebInputEventResult result_a,
                                     WebInputEventResult result_b);
WebInputEventResult ToWebInputEventResult(DispatchEventResult);

PaintLayer* LayerForNode(Node*);
ScrollableArea* AssociatedScrollableArea(const PaintLayer*);

bool IsInDocument(EventTarget*);

ContainerNode* ParentForClickEvent(const Node&);

LayoutPoint ContentPointFromRootFrame(LocalFrame*,
                                      const IntPoint& point_in_root_frame);

MouseEventWithHitTestResults PerformMouseEventHitTest(LocalFrame*,
                                                      const HitTestRequest&,
                                                      const WebMouseEvent&);

LocalFrame* SubframeForHitTestResult(const MouseEventWithHitTestResults&);

LocalFrame* SubframeForTargetNode(Node*);

class PointerEventTarget {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  DEFINE_INLINE_TRACE() {
    visitor->Trace(target_node);
    visitor->Trace(target_frame);
  }

  Member<Node> target_node;
  Member<LocalFrame> target_frame;
  String region;
};

}  // namespace EventHandlingUtil

}  // namespace blink

WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::EventHandlingUtil::PointerEventTarget);

#endif  // EventHandlingUtil_h
