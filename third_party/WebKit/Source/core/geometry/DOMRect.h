// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMRect_h
#define DOMRect_h

#include "bindings/core/v8/Dictionary.h"
#include "core/CoreExport.h"
#include "core/geometry/DOMRectReadOnly.h"
#include "platform/geometry/FloatRect.h"

namespace blink {

class DOMRect;
class DOMRectInit;

class CORE_EXPORT DOMRect final : public DOMRectReadOnly {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DOMRect* Create(double x = 0,
                         double y = 0,
                         double width = 0,
                         double height = 0);
  static DOMRect* FromFloatRect(const FloatRect&);
  static DOMRect* fromRect(const DOMRectInit&);

  void setX(double x) { x_ = x; }
  void setY(double y) { y_ = y; }
  void setWidth(double width) { width_ = width; }
  void setHeight(double height) { height_ = height; }

 protected:
  DOMRect(double x, double y, double z, double w);
};

}  // namespace blink

#endif
