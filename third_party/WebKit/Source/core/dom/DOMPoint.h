// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMPoint_h
#define DOMPoint_h

#include "bindings/core/v8/Dictionary.h"
#include "core/dom/DOMPointReadOnly.h"

namespace WebCore {

class DOMPoint FINAL : public DOMPointReadOnly {
public:
    static DOMPoint* create(const Dictionary&);
    static DOMPoint* create(double x, double y, double z = 0, double w = 1);

    void setX(double x) { m_x = x; }
    void setY(double y) { m_y = y; }
    void setZ(double z) { m_z = z; }
    void setW(double w) { m_w = w; }

protected:
    DOMPoint(double x, double y, double z, double w);
};

} // namespace WebCore

#endif
