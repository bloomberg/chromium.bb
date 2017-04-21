/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008 Collabora Ltd.  All rights reserved.
 * Copyright (C) 2013 Google Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FrameViewBase_h
#define FrameViewBase_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/IntRect.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"

namespace blink {

class Event;

// The FrameViewBase class serves as a base class for FrameView, Scrollbar, and
// PluginView.
//
// FrameViewBases are connected in a hierarchy, with the restriction that
// plugins and scrollbars are always leaves of the tree. Only FrameView can have
// children (and therefore the FrameViewBase class has no concept of children).
class PLATFORM_EXPORT FrameViewBase
    : public GarbageCollectedFinalized<FrameViewBase> {
 public:
  FrameViewBase();
  virtual ~FrameViewBase();

  int X() const { return FrameRect().X(); }
  int Y() const { return FrameRect().Y(); }
  int Width() const { return FrameRect().Width(); }
  int Height() const { return FrameRect().Height(); }
  IntSize Size() const { return FrameRect().Size(); }
  IntPoint Location() const { return FrameRect().Location(); }

  virtual void SetFrameRect(const IntRect& frame_rect) {
    frame_rect_ = frame_rect;
  }
  const IntRect& FrameRect() const { return frame_rect_; }

  void Resize(int w, int h) { SetFrameRect(IntRect(X(), Y(), w, h)); }
  void Resize(const IntSize& s) { SetFrameRect(IntRect(Location(), s)); }

  bool IsSelfVisible() const {
    return self_visible_;
  }  // Whether or not we have been explicitly marked as visible or not.
  bool IsParentVisible() const {
    return parent_visible_;
  }  // Whether or not our parent is visible.
  bool IsVisible() const {
    return self_visible_ && parent_visible_;
  }  // Whether or not we are actually visible.
  virtual void SetParentVisible(bool visible) { parent_visible_ = visible; }
  void SetSelfVisible(bool v) { self_visible_ = v; }

  virtual bool IsFrameView() const { return false; }
  virtual bool IsRemoteFrameView() const { return false; }
  virtual bool IsPluginView() const { return false; }
  virtual bool IsPluginContainer() const { return false; }
  virtual bool IsScrollbar() const { return false; }

  virtual void SetParent(FrameViewBase*);
  FrameViewBase* Parent() const { return parent_; }
  FrameViewBase* Root() const;

  virtual void HandleEvent(Event*) {}

  // ConvertFromRootFrame must be in FrameViewBase rather than FrameView
  // to be visible to Scrollbar::ConvertFromRootFrame and
  // RemoteFrameView::UpdateRemoteViewportIntersection. The related
  // ConvertFromContainingFrameViewBase must be declared locally to be visible.
  IntRect ConvertFromRootFrame(const IntRect&) const;
  IntPoint ConvertFromRootFrame(const IntPoint&) const;
  FloatPoint ConvertFromRootFrame(const FloatPoint&) const;
  // TODO(joelhockey): Change all these to pure virtual functions
  // Once RemoteFrameView no longer inherits from FrameViewBase.
  virtual IntRect ConvertFromContainingFrameViewBase(
      const IntRect& parent_rect) const {
    NOTREACHED();
    return parent_rect;
  }
  virtual IntPoint ConvertFromContainingFrameViewBase(
      const IntPoint& parent_point) const {
    NOTREACHED();
    return parent_point;
  }

  virtual void FrameRectsChanged() {}

  virtual void GeometryMayHaveChanged() {}

  // Notifies this frameviewbase that it will no longer be receiving events.
  virtual void EventListenersRemoved() {}

  DECLARE_VIRTUAL_TRACE();
  virtual void Dispose() {}

 private:
  Member<FrameViewBase> parent_;
  IntRect frame_rect_;
  bool self_visible_;
  bool parent_visible_;
};

}  // namespace blink

#endif  // FrameViewBase_h
