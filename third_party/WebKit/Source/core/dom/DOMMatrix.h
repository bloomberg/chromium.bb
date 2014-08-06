// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMMatrix_h
#define DOMMatrix_h

#include "core/dom/DOMMatrixReadOnly.h"

namespace blink {

class DOMMatrix : public DOMMatrixReadOnly {
public:
    static DOMMatrix* create();
    static DOMMatrix* create(DOMMatrixReadOnly*);

    void setA(double value) { setM11(value); }
    void setB(double value) { setM12(value); }
    void setC(double value) { setM21(value); }
    void setD(double value) { setM22(value); }
    void setE(double value) { setM41(value); }
    void setF(double value) { setM42(value); }

    void setM11(double value) { m_matrix[0][0] = value; }
    void setM12(double value) { m_matrix[0][1] = value; }
    void setM13(double value) { m_matrix[0][2] = value; setIs2D(!value); }
    void setM14(double value) { m_matrix[0][3] = value; setIs2D(!value); }
    void setM21(double value) { m_matrix[1][0] = value; }
    void setM22(double value) { m_matrix[1][1] = value; }
    void setM23(double value) { m_matrix[1][2] = value; setIs2D(!value); }
    void setM24(double value) { m_matrix[1][3] = value; setIs2D(!value); }
    void setM31(double value) { m_matrix[2][0] = value; setIs2D(!value); }
    void setM32(double value) { m_matrix[2][1] = value; setIs2D(!value); }
    void setM33(double value) { m_matrix[2][2] = value; setIs2D(value != 1); }
    void setM34(double value) { m_matrix[2][3] = value; setIs2D(!value); }
    void setM41(double value) { m_matrix[3][0] = value; }
    void setM42(double value) { m_matrix[3][1] = value; }
    void setM43(double value) { m_matrix[3][2] = value; setIs2D(!value); }
    void setM44(double value) { m_matrix[3][3] = value; setIs2D(value != 1); }

private:
    DOMMatrix(double m11, double m12, double m13, double m14,
        double m21, double m22, double m23, double m24,
        double m31, double m32, double m33, double m34,
        double m41, double m42, double m43, double m44,
        bool is2D = true);

    void setIs2D(bool value);
};

} // namespace blink

#endif
