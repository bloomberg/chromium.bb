// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMPointReadOnly_h
#define DOMPointReadOnly_h

#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class DOMMatrixInit;
class DOMPoint;
class DOMPointInit;
class ExceptionState;
class ScriptState;
class ScriptValue;

class CORE_EXPORT DOMPointReadOnly : public GarbageCollected<DOMPointReadOnly>,
                                     public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DOMPointReadOnly* Create(double x, double y, double z, double w);
  static DOMPointReadOnly* fromPoint(const DOMPointInit&);

  double x() const { return x_; }
  double y() const { return y_; }
  double z() const { return z_; }
  double w() const { return w_; }

  void Trace(blink::Visitor* visitor) {}

  ScriptValue toJSONForBinding(ScriptState*) const;
  DOMPoint* matrixTransform(DOMMatrixInit&, ExceptionState&);

 protected:
  DOMPointReadOnly(double x, double y, double z, double w);

  double x_;
  double y_;
  double z_;
  double w_;
};

}  // namespace blink

#endif
