// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/DOMPoint.h"

namespace WebCore {

DOMPoint* DOMPoint::create(const Dictionary& point)
{
    double x = 0;
    double y = 0;
    double z = 0;
    double w = 0;

    if (!DictionaryHelper::get(point, "x", x))
        x = 0;
    if (!DictionaryHelper::get(point, "y", y))
        y = 0;
    if (!DictionaryHelper::get(point, "z", z))
        z = 0;
    if (!DictionaryHelper::get(point, "w", w))
        w = 1;

    return new DOMPoint(x, y, z, w);
}

DOMPoint* DOMPoint::create(double x, double y, double z, double w)
{
    return new DOMPoint(x, y, z, w);
}

DOMPoint::DOMPoint(double x, double y, double z, double w)
    : DOMPointReadOnly(x, y, z, w)
{
}

} // namespace WebCore
