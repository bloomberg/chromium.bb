/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef PaintPhase_h
#define PaintPhase_h

namespace blink {

/*
 *  The painting of a layer occurs in three distinct phases.  Each phase involves
 *  a recursive descent into the layer's layout objects. The first phase is the background phase.
 *  The backgrounds and borders of all blocks are painted.  Inlines are not painted at all.
 *  Floats must paint above block backgrounds but entirely below inline content that can overlap them.
 *  In the foreground phase, all inlines are fully painted.  Inline replaced elements will get all
 *  three phases invoked on them during this phase.
 */

enum PaintPhase {
    PaintPhaseBlockBackground = 0,
    PaintPhaseChildBlockBackground = 1,
    PaintPhaseChildBlockBackgrounds = 2,
    PaintPhaseFloat = 3,
    PaintPhaseForeground = 4,
    PaintPhaseOutline = 5,
    PaintPhaseChildOutlines = 6,
    PaintPhaseSelfOutline = 7,
    PaintPhaseSelection = 8,
    PaintPhaseTextClip = 9,
    PaintPhaseMask = 10,
    PaintPhaseClippingMask = 11,
    PaintPhaseMax = PaintPhaseClippingMask,
    // These values must be kept in sync with DisplayItem::Type and DisplayItem::typeAsDebugString().
};

// Those flags are meant as global tree operations. This means
// that they should be constant for a paint phase.
enum GlobalPaintFlag {
    GlobalPaintNormalPhase = 0,
    // Used when painting selection as part of a drag-image. This
    // flag disables a lot of the painting code and specifically
    // triggers a PaintPhaseSelection.
    GlobalPaintSelectionOnly = 1 << 0,
    // Used when painting a drag-image or printing in order to
    // ignore the hardware layers and paint the whole tree
    // into the topmost layer.
    GlobalPaintFlattenCompositingLayers = 1 << 1,
    // Used when printing in order to adapt the output to the medium, for
    // instance by not painting shadows and selections on text, and add
    // URL metadata for links.
    GlobalPaintPrinting = 1 << 2
};

typedef unsigned GlobalPaintFlags;

} // namespace blink

#endif // PaintPhase_h
