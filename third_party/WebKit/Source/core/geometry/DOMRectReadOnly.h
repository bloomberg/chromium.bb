// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMRectReadOnly_h
#define DOMRectReadOnly_h

#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/IntRect.h"
#include "platform/heap/Handle.h"

namespace blink {

class DOMRectInit;
class ScriptValue;
class ScriptState;

class CORE_EXPORT DOMRectReadOnly : public GarbageCollected<DOMRectReadOnly>,
                                    public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DOMRectReadOnly* Create(double x,
                                 double y,
                                 double width,
                                 double height);
  static DOMRectReadOnly* FromIntRect(const IntRect&);
  static DOMRectReadOnly* FromFloatRect(const FloatRect&);
  static DOMRectReadOnly* fromRect(const DOMRectInit&);

  double x() const { return x_; }
  double y() const { return y_; }
  double width() const { return width_; }
  double height() const { return height_; }

  double top() const { return std::min(y_, y_ + height_); }
  double right() const { return std::max(x_, x_ + width_); }
  double bottom() const { return std::max(y_, y_ + height_); }
  double left() const { return std::min(x_, x_ + width_); }

  void Trace(blink::Visitor* visitor) {}

  ScriptValue toJSONForBinding(ScriptState*) const;

 protected:
  DOMRectReadOnly(double x, double y, double width, double height);

  double x_;
  double y_;
  double width_;
  double height_;
};

}  // namespace blink

#endif
