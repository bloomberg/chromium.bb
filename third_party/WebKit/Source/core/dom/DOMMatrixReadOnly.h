// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMMatrixReadOnly_h
#define DOMMatrixReadOnly_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class DOMMatrixReadOnly : public GarbageCollected<DOMMatrixReadOnly>, public ScriptWrappableBase {
public:
    double a() const { return m11(); }
    double b() const { return m12(); }
    double c() const { return m21(); }
    double d() const { return m22(); }
    double e() const { return m41(); }
    double f() const { return m42(); }

    double m11() const { return m_matrix[0][0]; }
    double m12() const { return m_matrix[0][1]; }
    double m13() const { return m_matrix[0][2]; }
    double m14() const { return m_matrix[0][3]; }
    double m21() const { return m_matrix[1][0]; }
    double m22() const { return m_matrix[1][1]; }
    double m23() const { return m_matrix[1][2]; }
    double m24() const { return m_matrix[1][3]; }
    double m31() const { return m_matrix[2][0]; }
    double m32() const { return m_matrix[2][1]; }
    double m33() const { return m_matrix[2][2]; }
    double m34() const { return m_matrix[2][3]; }
    double m41() const { return m_matrix[3][0]; }
    double m42() const { return m_matrix[3][1]; }
    double m43() const { return m_matrix[3][2]; }
    double m44() const { return m_matrix[3][3]; }

    bool is2D() const;
    bool isIdentity() const;

    void trace(Visitor*) { }

protected:
    double m_matrix[4][4];
    bool m_is2D;
};

} // namespace blink

#endif
