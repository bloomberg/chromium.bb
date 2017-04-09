/*
 * Copyright (C) Research In Motion Limited 2010-2012. All rights reserved.
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

#include "core/layout/svg/SVGTextMetrics.h"

#include "platform/fonts/FontOrientation.h"

namespace blink {

SVGTextMetrics::SVGTextMetrics(unsigned length, float width, float height)
    : width_(width), height_(height), length_(length) {}

SVGTextMetrics::SVGTextMetrics(SVGTextMetrics::MetricsType)
    : SVGTextMetrics(1, 0, 0) {}

float SVGTextMetrics::Advance(FontOrientation orientation) const {
  switch (orientation) {
    case FontOrientation::kHorizontal:
    case FontOrientation::kVerticalRotated:
      return Width();
    case FontOrientation::kVerticalUpright:
      return Height();
    default:
      NOTREACHED();
      return Width();
  }
}

}  // namespace blink
