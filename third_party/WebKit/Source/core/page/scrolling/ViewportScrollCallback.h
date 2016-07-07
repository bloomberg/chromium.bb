// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ViewportScrollCallback_h
#define ViewportScrollCallback_h

#include "core/page/scrolling/ScrollStateCallback.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollTypes.h"

namespace blink {

class ScrollableArea;
class ScrollState;

// ViewportScrollCallback is an interface that's used by the
// RootScrollerController to apply scrolls to the designated rootScroller
// element. It is a ScrollStateCallback, meaning that it's applied during the
// applyScroll step of ScrollCustomization and child classes must implement
// handleEvent in order to actually do the scrolling as well as any possible
// associated actions.
//
// ScrollCustomization generally relies on using the nativeApplyScroll to
// scroll the element; however, the rootScroller may need to execute actions
// both before and after the native scroll which is currently unsupported.
// Because of this, the ViewportScrollCallback can scroll the Element directly.
// This is accomplished by descendant classes implementing the setScroller
// method which RootScrollerController will call to fill the callback with the
// appropriate ScrollableArea to use.
class ViewportScrollCallback : public ScrollStateCallback {
public:
    virtual ~ViewportScrollCallback() {}

    virtual void setScroller(ScrollableArea*) = 0;

    DEFINE_INLINE_VIRTUAL_TRACE() {
        ScrollStateCallback::trace(visitor);
    }
protected:
    ScrollResult performNativeScroll(ScrollState&, ScrollableArea&);
};

} // namespace blink

#endif // ViewportScrollCallback_h
