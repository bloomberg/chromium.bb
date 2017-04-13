/*
 * Copyright (C) 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "platform/FrameViewBase.h"

#include "platform/wtf/Assertions.h"

namespace blink {

FrameViewBase::FrameViewBase()
    : parent_(nullptr), self_visible_(false), parent_visible_(false) {}

FrameViewBase::~FrameViewBase() {}

DEFINE_TRACE(FrameViewBase) {
  visitor->Trace(parent_);
}

void FrameViewBase::SetParent(FrameViewBase* frame_view_base) {
  DCHECK(!frame_view_base || !parent_);
  if (!frame_view_base || !frame_view_base->IsVisible())
    SetParentVisible(false);
  parent_ = frame_view_base;
  if (frame_view_base && frame_view_base->IsVisible())
    SetParentVisible(true);
}

FrameViewBase* FrameViewBase::Root() const {
  const FrameViewBase* top = this;
  while (top->Parent())
    top = top->Parent();
  if (top->IsFrameView())
    return const_cast<FrameViewBase*>(static_cast<const FrameViewBase*>(top));
  return 0;
}

IntRect FrameViewBase::ConvertFromRootFrame(
    const IntRect& rect_in_root_frame) const {
  if (const FrameViewBase* parent_frame_view_base = Parent()) {
    IntRect parent_rect =
        parent_frame_view_base->ConvertFromRootFrame(rect_in_root_frame);
    return ConvertFromContainingFrameViewBase(parent_rect);
  }
  return rect_in_root_frame;
}

IntRect FrameViewBase::ConvertToRootFrame(const IntRect& local_rect) const {
  if (const FrameViewBase* parent_frame_view_base = Parent()) {
    IntRect parent_rect = ConvertToContainingFrameViewBase(local_rect);
    return parent_frame_view_base->ConvertToRootFrame(parent_rect);
  }
  return local_rect;
}

IntPoint FrameViewBase::ConvertFromRootFrame(
    const IntPoint& point_in_root_frame) const {
  if (const FrameViewBase* parent_frame_view_base = Parent()) {
    IntPoint parent_point =
        parent_frame_view_base->ConvertFromRootFrame(point_in_root_frame);
    return ConvertFromContainingFrameViewBase(parent_point);
  }
  return point_in_root_frame;
}

FloatPoint FrameViewBase::ConvertFromRootFrame(
    const FloatPoint& point_in_root_frame) const {
  // FrameViewBase / windows are required to be IntPoint aligned, but we may
  // need to convert FloatPoint values within them (eg. for event co-ordinates).
  IntPoint floored_point = FlooredIntPoint(point_in_root_frame);
  FloatPoint parent_point = this->ConvertFromRootFrame(floored_point);
  FloatSize window_fraction = point_in_root_frame - floored_point;
  // Use linear interpolation handle any fractional value (eg. for iframes
  // subject to a transform beyond just a simple translation).
  // FIXME: Add FloatPoint variants of all co-ordinate space conversion APIs.
  if (!window_fraction.IsEmpty()) {
    const int kFactor = 1000;
    IntPoint parent_line_end = this->ConvertFromRootFrame(
        floored_point + RoundedIntSize(window_fraction.ScaledBy(kFactor)));
    FloatSize parent_fraction =
        (parent_line_end - parent_point).ScaledBy(1.0f / kFactor);
    parent_point.Move(parent_fraction);
  }
  return parent_point;
}

IntPoint FrameViewBase::ConvertToRootFrame(const IntPoint& local_point) const {
  if (const FrameViewBase* parent_frame_view_base = Parent()) {
    IntPoint parent_point = ConvertToContainingFrameViewBase(local_point);
    return parent_frame_view_base->ConvertToRootFrame(parent_point);
  }
  return local_point;
}

IntRect FrameViewBase::ConvertToContainingFrameViewBase(
    const IntRect& local_rect) const {
  if (const FrameViewBase* parent_frame_view_base = Parent()) {
    IntRect parent_rect(local_rect);
    parent_rect.SetLocation(parent_frame_view_base->ConvertChildToSelf(
        this, local_rect.Location()));
    return parent_rect;
  }
  return local_rect;
}

IntRect FrameViewBase::ConvertFromContainingFrameViewBase(
    const IntRect& parent_rect) const {
  if (const FrameViewBase* parent_frame_view_base = Parent()) {
    IntRect local_rect = parent_rect;
    local_rect.SetLocation(parent_frame_view_base->ConvertSelfToChild(
        this, local_rect.Location()));
    return local_rect;
  }

  return parent_rect;
}

IntPoint FrameViewBase::ConvertToContainingFrameViewBase(
    const IntPoint& local_point) const {
  if (const FrameViewBase* parent_frame_view_base = Parent())
    return parent_frame_view_base->ConvertChildToSelf(this, local_point);

  return local_point;
}

IntPoint FrameViewBase::ConvertFromContainingFrameViewBase(
    const IntPoint& parent_point) const {
  if (const FrameViewBase* parent_frame_view_base = Parent())
    return parent_frame_view_base->ConvertSelfToChild(this, parent_point);

  return parent_point;
}

IntPoint FrameViewBase::ConvertChildToSelf(const FrameViewBase*,
                                           const IntPoint& point) const {
  return point;
}

IntPoint FrameViewBase::ConvertSelfToChild(const FrameViewBase*,
                                           const IntPoint& point) const {
  return point;
}

}  // namespace blink
