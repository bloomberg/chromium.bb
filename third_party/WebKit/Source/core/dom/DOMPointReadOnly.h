// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMPointReadOnly_h
#define DOMPointReadOnly_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
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
  static DOMPointReadOnly* create(double x, double y, double z, double w);
  static DOMPointReadOnly* fromPoint(const DOMPointInit&);

  double x() const { return m_x; }
  double y() const { return m_y; }
  double z() const { return m_z; }
  double w() const { return m_w; }

  DEFINE_INLINE_TRACE() {}
  
  ScriptValue toJSONForBinding(ScriptState*) const;
  DOMPoint* matrixTransform(DOMMatrixInit&, ExceptionState&);

 protected:
  DOMPointReadOnly(double x, double y, double z, double w);

  double m_x;
  double m_y;
  double m_z;
  double m_w;
};

}  // namespace blink

#endif
