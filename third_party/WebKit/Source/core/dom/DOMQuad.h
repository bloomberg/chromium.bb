// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMQuad_h
#define DOMQuad_h

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"

namespace blink {

class DOMPoint;
class DOMPointInit;
class DOMQuadInit;
class DOMRect;
class DOMRectInit;

class CORE_EXPORT DOMQuad : public GarbageCollected<DOMQuad>,
                            public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DOMQuad* create(const DOMPointInit& p1,
                         const DOMPointInit& p2,
                         const DOMPointInit& p3,
                         const DOMPointInit& p4);
  static DOMQuad* fromRect(const DOMRectInit&);
  static DOMQuad* fromQuad(const DOMQuadInit&);

  DOMPoint* p1() const { return m_p1; }
  DOMPoint* p2() const { return m_p2; }
  DOMPoint* p3() const { return m_p3; }
  DOMPoint* p4() const { return m_p4; }

  DOMRect* getBounds();

  ScriptValue toJSONForBinding(ScriptState*) const;

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_p1);
    visitor->trace(m_p2);
    visitor->trace(m_p3);
    visitor->trace(m_p4);
  }

 private:
  DOMQuad(const DOMPointInit& p1,
          const DOMPointInit& p2,
          const DOMPointInit& p3,
          const DOMPointInit& p4);
  DOMQuad(double x, double y, double width, double height);

  void calculateBounds();

  Member<DOMPoint> m_p1;
  Member<DOMPoint> m_p2;
  Member<DOMPoint> m_p3;
  Member<DOMPoint> m_p4;

  double m_left;
  double m_right;
  double m_top;
  double m_bottom;
};

}  // namespace blink

#endif
