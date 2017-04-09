// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMPoint_h
#define DOMPoint_h

#include "core/CoreExport.h"
#include "core/geometry/DOMPointReadOnly.h"

namespace blink {

class DOMPointInit;

class CORE_EXPORT DOMPoint final : public DOMPointReadOnly {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DOMPoint* Create(double x, double y, double z = 0, double w = 1);
  static DOMPoint* fromPoint(const DOMPointInit&);

  void setX(double x) { x_ = x; }
  void setY(double y) { y_ = y; }
  void setZ(double z) { z_ = z; }
  void setW(double w) { w_ = w; }

 protected:
  DOMPoint(double x, double y, double z, double w);
};

}  // namespace blink

#endif
