// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintProperties_h
#define PaintProperties_h

#include <iosfwd>

namespace blink {

// The set of paint properties applying to a |PaintChunk|.
// In particular, this does not mean properties like background-color, but
// rather the hierarchy of transforms, clips, effects, etc. that apply to a
// contiguous chunk of drawings.
struct PaintProperties {
    // TODO(jbroman): Add actual properties.
};

inline bool operator==(const PaintProperties&, const PaintProperties&)
{
    return true;
}

inline bool operator!=(const PaintProperties& a, const PaintProperties& b)
{
    return !(a == b);
}

// Redeclared here to avoid ODR issues.
// See platform/testing/PaintPrinters.h.
void PrintTo(const PaintProperties&, std::ostream*);

} // namespace blink

#endif // PaintProperties_h
