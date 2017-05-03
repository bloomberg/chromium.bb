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

namespace blink {

// The FrameViewBase class is the parent of Scrollbar.
// TODO(joelhockey): Move core/paint/ScrollbarManager to platform/scroll
// and use it to replace this class.
class PLATFORM_EXPORT FrameViewBase : public GarbageCollectedMixin {
 public:
  FrameViewBase(){};
  virtual ~FrameViewBase(){};

  virtual IntPoint Location() const = 0;

  virtual bool IsFrameView() const { return false; }
  virtual bool IsRemoteFrameView() const { return false; }
  virtual bool IsErrorplaceholder() { return false; }

  virtual void SetParent(FrameViewBase*) = 0;
  virtual FrameViewBase* Parent() const = 0;

  // TODO(joelhockey): Remove this from FrameViewBase once FrameView children
  // use FrameOrPlugin rather than FrameViewBase.  This method does not apply to
  // scrollbars.
  virtual void SetParentVisible(bool visible) {}

  // ConvertFromRootFrame must be in FrameViewBase rather than FrameView
  // to be visible to Scrollbar::ConvertFromRootFrame and
  // RemoteFrameView::UpdateRemoteViewportIntersection. The related
  // ConvertFromContainingFrameViewBase must be declared locally to be visible.
  IntRect ConvertFromRootFrame(const IntRect& rect_in_root_frame) const {
    if (const FrameViewBase* parent_frame_view_base = Parent()) {
      IntRect parent_rect =
          parent_frame_view_base->ConvertFromRootFrame(rect_in_root_frame);
      return ConvertFromContainingFrameViewBase(parent_rect);
    }
    return rect_in_root_frame;
  }

  IntPoint ConvertFromRootFrame(const IntPoint& point_in_root_frame) const {
    if (const FrameViewBase* parent_frame_view_base = Parent()) {
      IntPoint parent_point =
          parent_frame_view_base->ConvertFromRootFrame(point_in_root_frame);
      return ConvertFromContainingFrameViewBase(parent_point);
    }
    return point_in_root_frame;
  }

  FloatPoint ConvertFromRootFrame(const FloatPoint& point_in_root_frame) const {
    // FrameViewBase / windows are required to be IntPoint aligned, but we may
    // need to convert FloatPoint values within them (eg. for event
    // co-ordinates).
    IntPoint floored_point = FlooredIntPoint(point_in_root_frame);
    FloatPoint parent_point = ConvertFromRootFrame(floored_point);
    FloatSize window_fraction = point_in_root_frame - floored_point;
    // Use linear interpolation handle any fractional value (eg. for iframes
    // subject to a transform beyond just a simple translation).
    // FIXME: Add FloatPoint variants of all co-ordinate space conversion APIs.
    if (!window_fraction.IsEmpty()) {
      const int kFactor = 1000;
      IntPoint parent_line_end = ConvertFromRootFrame(
          floored_point + RoundedIntSize(window_fraction.ScaledBy(kFactor)));
      FloatSize parent_fraction =
          (parent_line_end - parent_point).ScaledBy(1.0f / kFactor);
      parent_point.Move(parent_fraction);
    }
    return parent_point;
  }

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

  virtual void FrameRectsChanged() { NOTREACHED(); }

  virtual void Dispose() {}
};

}  // namespace blink

#endif  // FrameViewBase_h
