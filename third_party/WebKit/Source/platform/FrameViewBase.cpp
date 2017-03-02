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

#include "wtf/Assertions.h"

namespace blink {

FrameViewBase::FrameViewBase()
    : m_parent(nullptr), m_selfVisible(false), m_parentVisible(false) {}

FrameViewBase::~FrameViewBase() {}

DEFINE_TRACE(FrameViewBase) {
  visitor->trace(m_parent);
}

void FrameViewBase::setParent(FrameViewBase* frameViewBase) {
  DCHECK(!frameViewBase || !m_parent);
  if (!frameViewBase || !frameViewBase->isVisible())
    setParentVisible(false);
  m_parent = frameViewBase;
  if (frameViewBase && frameViewBase->isVisible())
    setParentVisible(true);
}

FrameViewBase* FrameViewBase::root() const {
  const FrameViewBase* top = this;
  while (top->parent())
    top = top->parent();
  if (top->isFrameView())
    return const_cast<FrameViewBase*>(static_cast<const FrameViewBase*>(top));
  return 0;
}

IntRect FrameViewBase::convertFromRootFrame(
    const IntRect& rectInRootFrame) const {
  if (const FrameViewBase* parentFrameViewBase = parent()) {
    IntRect parentRect =
        parentFrameViewBase->convertFromRootFrame(rectInRootFrame);
    return convertFromContainingWidget(parentRect);
  }
  return rectInRootFrame;
}

IntRect FrameViewBase::convertToRootFrame(const IntRect& localRect) const {
  if (const FrameViewBase* parentFrameViewBase = parent()) {
    IntRect parentRect = convertToContainingWidget(localRect);
    return parentFrameViewBase->convertToRootFrame(parentRect);
  }
  return localRect;
}

IntPoint FrameViewBase::convertFromRootFrame(
    const IntPoint& pointInRootFrame) const {
  if (const FrameViewBase* parentFrameViewBase = parent()) {
    IntPoint parentPoint =
        parentFrameViewBase->convertFromRootFrame(pointInRootFrame);
    return convertFromContainingWidget(parentPoint);
  }
  return pointInRootFrame;
}

FloatPoint FrameViewBase::convertFromRootFrame(
    const FloatPoint& pointInRootFrame) const {
  // FrameViewBase / windows are required to be IntPoint aligned, but we may
  // need to convert FloatPoint values within them (eg. for event co-ordinates).
  IntPoint flooredPoint = flooredIntPoint(pointInRootFrame);
  FloatPoint parentPoint = this->convertFromRootFrame(flooredPoint);
  FloatSize windowFraction = pointInRootFrame - flooredPoint;
  // Use linear interpolation handle any fractional value (eg. for iframes
  // subject to a transform beyond just a simple translation).
  // FIXME: Add FloatPoint variants of all co-ordinate space conversion APIs.
  if (!windowFraction.isEmpty()) {
    const int kFactor = 1000;
    IntPoint parentLineEnd = this->convertFromRootFrame(
        flooredPoint + roundedIntSize(windowFraction.scaledBy(kFactor)));
    FloatSize parentFraction =
        (parentLineEnd - parentPoint).scaledBy(1.0f / kFactor);
    parentPoint.move(parentFraction);
  }
  return parentPoint;
}

IntPoint FrameViewBase::convertToRootFrame(const IntPoint& localPoint) const {
  if (const FrameViewBase* parentFrameViewBase = parent()) {
    IntPoint parentPoint = convertToContainingWidget(localPoint);
    return parentFrameViewBase->convertToRootFrame(parentPoint);
  }
  return localPoint;
}

IntRect FrameViewBase::convertToContainingWidget(
    const IntRect& localRect) const {
  if (const FrameViewBase* parentFrameViewBase = parent()) {
    IntRect parentRect(localRect);
    parentRect.setLocation(
        parentFrameViewBase->convertChildToSelf(this, localRect.location()));
    return parentRect;
  }
  return localRect;
}

IntRect FrameViewBase::convertFromContainingWidget(
    const IntRect& parentRect) const {
  if (const FrameViewBase* parentFrameViewBase = parent()) {
    IntRect localRect = parentRect;
    localRect.setLocation(
        parentFrameViewBase->convertSelfToChild(this, localRect.location()));
    return localRect;
  }

  return parentRect;
}

IntPoint FrameViewBase::convertToContainingWidget(
    const IntPoint& localPoint) const {
  if (const FrameViewBase* parentFrameViewBase = parent())
    return parentFrameViewBase->convertChildToSelf(this, localPoint);

  return localPoint;
}

IntPoint FrameViewBase::convertFromContainingWidget(
    const IntPoint& parentPoint) const {
  if (const FrameViewBase* parentFrameViewBase = parent())
    return parentFrameViewBase->convertSelfToChild(this, parentPoint);

  return parentPoint;
}

IntPoint FrameViewBase::convertChildToSelf(const FrameViewBase*,
                                           const IntPoint& point) const {
  return point;
}

IntPoint FrameViewBase::convertSelfToChild(const FrameViewBase*,
                                           const IntPoint& point) const {
  return point;
}

}  // namespace blink
