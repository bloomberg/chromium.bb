// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BorderEdge_h
#define BorderEdge_h

#include "core/rendering/style/RenderStyleConstants.h"
#include "platform/graphics/Color.h"

namespace blink {

struct BorderEdge {
    BorderEdge(int edgeWidth, const Color& edgeColor, EBorderStyle edgeStyle, bool edgeIsTransparent, bool edgeIsPresent = true);
    BorderEdge();

    bool hasVisibleColorAndStyle() const;
    bool shouldRender() const;
    bool presentButInvisible() const;
    bool obscuresBackgroundEdge(float scale) const;
    bool obscuresBackground() const;
    int usedWidth() const;

    void getDoubleBorderStripeWidths(int& outerWidth, int& innerWidth) const;

    bool sharesColorWith(const BorderEdge& other) const;

    EBorderStyle borderStyle() const  { return static_cast<EBorderStyle>(style); }

    int width;
    Color color;
    bool isTransparent;
    bool isPresent;

private:
    unsigned style: 4; // EBorderStyle
};

} // namespace blink

#endif // BorderEdge_h
