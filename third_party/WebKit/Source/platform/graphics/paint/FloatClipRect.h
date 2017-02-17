// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FloatClipRect_h
#define FloatClipRect_h

#include "platform/geometry/FloatRect.h"
#include "platform/geometry/LayoutRect.h"
#include "wtf/Allocator.h"

namespace blink {

class PLATFORM_EXPORT FloatClipRect {
  USING_FAST_MALLOC(FloatClipRect);

 public:
  FloatClipRect()
      : m_rect(FloatRect(LayoutRect::infiniteIntRect())),
        m_hasRadius(false),
        m_isInfinite(true) {}

  FloatClipRect(const FloatRect& rect)
      : m_rect(rect), m_hasRadius(false), m_isInfinite(false) {}

  const FloatRect& rect() const { return m_rect; }

  void intersect(const FloatRect& other) {
    if (m_isInfinite) {
      m_rect = other;
      m_isInfinite = false;
    } else {
      m_rect.intersect(other);
    }
  }

  bool hasRadius() const { return m_hasRadius; }
  void setHasRadius() {
    m_hasRadius = true;
    m_isInfinite = false;
  }

  void setRect(const FloatRect& rect) {
    m_rect = rect;
    m_isInfinite = false;
  }

  void moveBy(const FloatPoint& offset) {
    if (m_isInfinite)
      return;
    m_rect.moveBy(offset);
  }

  bool isInfinite() const { return m_isInfinite; }

 private:
  FloatRect m_rect;
  bool m_hasRadius : 1;
  bool m_isInfinite : 1;
};

inline bool operator==(const FloatClipRect& a, const FloatClipRect& b) {
  if (a.isInfinite() && b.isInfinite())
    return true;
  if (!a.isInfinite() && !b.isInfinite())
    return a.rect() == b.rect() && a.hasRadius() == b.hasRadius();
  return false;
}

}  // namespace blink

#endif  // FloatClipRect_h
