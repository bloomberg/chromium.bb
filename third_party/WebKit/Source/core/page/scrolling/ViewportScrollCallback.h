// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ViewportScrollCallback_h
#define ViewportScrollCallback_h

#include "core/page/scrolling/ScrollStateCallback.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollTypes.h"

namespace blink {

class FloatSize;
class ScrollableArea;
class ScrollState;
class TopControls;
class OverscrollController;
class RootFrameViewport;

// ViewportScrollCallback is a ScrollStateCallback, meaning that it's applied
// during the applyScroll step of ScrollCustomization. It implements viewport
// actions like moving top controls and showing overscroll glow as well as
// scrolling the Element.
//
// ScrollCustomization generally relies on using the nativeApplyScroll to
// scroll the element; however, the rootScroller may need to execute actions
// both before and after the native scroll which is currently unsupported.
// Because of this, the ViewportScrollCallback can scroll the Element directly.
// This is accomplished by passing the ScrollableArea directly using
// setScroller() which RootScrollerController will call to set the appropriate
// ScrollableArea to use.
class ViewportScrollCallback : public ScrollStateCallback {
 public:
  // The TopControls and OverscrollController are given to the
  // ViewportScrollCallback but are not owned or kept alive by it.
  static ViewportScrollCallback* create(
      TopControls* topControls,
      OverscrollController* overscrollController,
      RootFrameViewport& rootFrameViewport) {
    return new ViewportScrollCallback(topControls, overscrollController,
                                      rootFrameViewport);
  }

  virtual ~ViewportScrollCallback();

  void handleEvent(ScrollState*) override;
  void setScroller(ScrollableArea*);

  DECLARE_VIRTUAL_TRACE();

 private:
  // ViewportScrollCallback does not assume ownership of TopControls or of
  // OverscrollController.
  ViewportScrollCallback(TopControls*,
                         OverscrollController*,
                         RootFrameViewport&);

  bool shouldScrollTopControls(const FloatSize&, ScrollGranularity) const;
  bool scrollTopControls(ScrollState&);

  ScrollResult performNativeScroll(ScrollState&);

  WeakMember<TopControls> m_topControls;
  WeakMember<OverscrollController> m_overscrollController;
  WeakMember<RootFrameViewport> m_rootFrameViewport;
};

}  // namespace blink

#endif  // ViewportScrollCallback_h
