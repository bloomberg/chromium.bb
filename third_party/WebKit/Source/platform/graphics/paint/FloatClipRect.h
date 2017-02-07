// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FloatClipRect_h
#define FloatClipRect_h

#include "platform/geometry/FloatRect.h"
#include "wtf/Allocator.h"

namespace blink {

class PLATFORM_EXPORT FloatClipRect {
  USING_FAST_MALLOC(FloatClipRect);

 public:
  FloatClipRect() : m_hasRadius(false) {}

  FloatClipRect(const FloatRect& rect) : m_rect(rect), m_hasRadius(false) {}

  const FloatRect& rect() const { return m_rect; }

  void intersect(const FloatRect& other) { m_rect.intersect(other); }

  bool hasRadius() const { return m_hasRadius; }
  void setHasRadius(bool hasRadius) { m_hasRadius = hasRadius; }

  void setRect(const FloatRect& rect) { m_rect = rect; }

 private:
  FloatRect m_rect;
  bool m_hasRadius;
};

}  // namespace blink

#endif  // FloatClipRect_h
