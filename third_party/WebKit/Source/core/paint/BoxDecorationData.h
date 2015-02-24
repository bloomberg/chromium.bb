// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BoxDecorationData_h
#define BoxDecorationData_h

#include "core/layout/LayoutBoxModelObject.h" // For BackgroundBleedAvoidance.
#include "platform/graphics/Color.h"

namespace blink {

class GraphicsContext;
class LayoutBox;

// Information extracted from LayoutStyle for box painting.
class BoxDecorationData {
public:
    BoxDecorationData(const LayoutBox&, GraphicsContext*);

    Color backgroundColor;
    bool hasBackground;
    bool hasBorder;
    bool hasAppearance;
    BackgroundBleedAvoidance bleedAvoidance() { return static_cast<BackgroundBleedAvoidance>(m_bleedAvoidance); }

private:
    BackgroundBleedAvoidance determineBackgroundBleedAvoidance(const LayoutBox&, GraphicsContext*);
    bool borderObscuresBackgroundEdge(const LayoutStyle&, const FloatSize& contextScale) const;
    unsigned m_bleedAvoidance : 2; // BackgroundBleedAvoidance
};

} // namespace blink

#endif
