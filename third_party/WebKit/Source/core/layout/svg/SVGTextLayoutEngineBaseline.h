/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
 */

#ifndef SVGTextLayoutEngineBaseline_h
#define SVGTextLayoutEngineBaseline_h

#include "base/macros.h"
#include "core/layout/api/LineLayoutItem.h"
#include "core/style/SVGComputedStyleDefs.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/Unicode.h"

namespace blink {

class Font;
class ComputedStyle;

// Helper class used by SVGTextLayoutEngine to handle 'alignment-baseline' /
// 'dominant-baseline' and 'baseline-shift'.
class SVGTextLayoutEngineBaseline {
  STACK_ALLOCATED();

 public:
  SVGTextLayoutEngineBaseline(const Font&, float effective_zoom);

  float CalculateBaselineShift(const ComputedStyle&) const;
  float CalculateAlignmentBaselineShift(bool is_vertical_text,
                                        LineLayoutItem) const;

 private:
  EAlignmentBaseline DominantBaselineToAlignmentBaseline(bool is_vertical_text,
                                                         LineLayoutItem) const;

  const Font& font_;

  // Everything we read from the m_font's font descriptor during layout is
  // scaled by the effective zoom, as fonts always are in computed style. Since
  // layout inside SVG takes place in unzoomed coordinates we have to compensate
  // for zoom when reading values from the font descriptor.
  float effective_zoom_;
  DISALLOW_COPY_AND_ASSIGN(SVGTextLayoutEngineBaseline);
};

}  // namespace blink

#endif
