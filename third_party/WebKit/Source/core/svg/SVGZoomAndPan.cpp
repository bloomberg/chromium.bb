/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
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

#include "core/svg/SVGZoomAndPan.h"

#include "platform/wtf/text/ParsingUtilities.h"

namespace blink {

SVGZoomAndPan::SVGZoomAndPan() : zoom_and_pan_(kSVGZoomAndPanMagnify) {}

void SVGZoomAndPan::ResetZoomAndPan() {
  zoom_and_pan_ = kSVGZoomAndPanMagnify;
}

bool SVGZoomAndPan::IsKnownAttribute(const QualifiedName& attr_name) {
  return attr_name == SVGNames::zoomAndPanAttr;
}

template <typename CharType>
static bool ParseZoomAndPanInternal(const CharType*& start,
                                    const CharType* end,
                                    SVGZoomAndPanType& zoom_and_pan) {
  if (SkipToken(start, end, "disable")) {
    zoom_and_pan = kSVGZoomAndPanDisable;
    return true;
  }
  if (SkipToken(start, end, "magnify")) {
    zoom_and_pan = kSVGZoomAndPanMagnify;
    return true;
  }
  return false;
}

bool SVGZoomAndPan::ParseZoomAndPan(const LChar*& start, const LChar* end) {
  return ParseZoomAndPanInternal(start, end, zoom_and_pan_);
}

bool SVGZoomAndPan::ParseZoomAndPan(const UChar*& start, const UChar* end) {
  return ParseZoomAndPanInternal(start, end, zoom_and_pan_);
}

}  // namespace blink
