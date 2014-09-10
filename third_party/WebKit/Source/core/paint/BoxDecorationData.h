// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BoxDecorationData_h
#define BoxDecorationData_h

#include "core/rendering/RenderBoxModelObject.h"
#include "platform/graphics/Color.h"

namespace blink {

class RenderStyle;
class GraphicsContext;

// Information extracted from RenderStyle for box painting.
class BoxDecorationData {
public:
    BoxDecorationData(const RenderStyle&, bool canRenderBorderImage, bool backgroundHasOpaqueTopLayer, GraphicsContext*);

    Color backgroundColor;
    bool hasBackground;
    bool hasBorder;
    bool hasAppearance;
    BackgroundBleedAvoidance bleedAvoidance() { return static_cast<BackgroundBleedAvoidance>(m_bleedAvoidance); }

private:
    BackgroundBleedAvoidance determineBackgroundBleedAvoidance(const RenderStyle&, bool canRenderBorderImage, bool backgroundHasOpaqueTopLayer, GraphicsContext*);
    bool borderObscuresBackgroundEdge(const RenderStyle&, const FloatSize& contextScale) const;
    unsigned m_bleedAvoidance : 2; // BackgroundBleedAvoidance
};

} // namespace blink

#endif
