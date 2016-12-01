/*
 * Copyright (C) 2007, 2010 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGViewSpec.h"

#include "core/svg/SVGParserUtilities.h"
#include "core/svg/SVGPreserveAspectRatio.h"
#include "core/svg/SVGRect.h"
#include "core/svg/SVGTransformList.h"
#include "wtf/text/ParsingUtilities.h"

namespace blink {

SVGViewSpec::SVGViewSpec()
    : m_viewBox(SVGRect::createInvalid()),
      m_preserveAspectRatio(SVGPreserveAspectRatio::create()),
      m_transform(SVGTransformList::create()) {}

DEFINE_TRACE(SVGViewSpec) {
  visitor->trace(m_viewBox);
  visitor->trace(m_preserveAspectRatio);
  visitor->trace(m_transform);
}

bool SVGViewSpec::parseViewSpec(const String& spec) {
  if (spec.isEmpty())
    return false;
  if (spec.is8Bit()) {
    const LChar* ptr = spec.characters8();
    const LChar* end = ptr + spec.length();
    return parseViewSpecInternal(ptr, end);
  }
  const UChar* ptr = spec.characters16();
  const UChar* end = ptr + spec.length();
  return parseViewSpecInternal(ptr, end);
}

void SVGViewSpec::setViewBox(const FloatRect& rect) {
  viewBox()->setValue(rect);
}

void SVGViewSpec::setPreserveAspectRatio(const SVGPreserveAspectRatio& other) {
  preserveAspectRatio()->setAlign(other.align());
  preserveAspectRatio()->setMeetOrSlice(other.meetOrSlice());
}

void SVGViewSpec::reset() {
  resetZoomAndPan();
  m_transform->clear();
  setViewBox(FloatRect());
  preserveAspectRatio()->setDefault();
}

namespace {

enum ViewSpecFunctionType {
  Unknown,
  PreserveAspectRatio,
  Transform,
  ViewBox,
  ViewTarget,
  ZoomAndPan,
};

template <typename CharType>
static ViewSpecFunctionType scanViewSpecFunction(const CharType*& ptr,
                                                 const CharType* end) {
  DCHECK_LT(ptr, end);
  switch (*ptr) {
    case 'v':
      if (skipToken(ptr, end, "viewBox"))
        return ViewBox;
      if (skipToken(ptr, end, "viewTarget"))
        return ViewTarget;
      break;
    case 'z':
      if (skipToken(ptr, end, "zoomAndPan"))
        return ZoomAndPan;
      break;
    case 'p':
      if (skipToken(ptr, end, "preserveAspectRatio"))
        return PreserveAspectRatio;
      break;
    case 't':
      if (skipToken(ptr, end, "transform"))
        return Transform;
      break;
  }
  return Unknown;
}

}  // namespace

template <typename CharType>
bool SVGViewSpec::parseViewSpecInternal(const CharType* ptr,
                                        const CharType* end) {
  if (!skipToken(ptr, end, "svgView"))
    return false;

  if (!skipExactly<CharType>(ptr, end, '('))
    return false;

  while (ptr < end && *ptr != ')') {
    ViewSpecFunctionType functionType = scanViewSpecFunction(ptr, end);
    if (functionType == Unknown)
      return false;

    if (!skipExactly<CharType>(ptr, end, '('))
      return false;

    switch (functionType) {
      case ViewBox: {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        if (!(parseNumber(ptr, end, x) && parseNumber(ptr, end, y) &&
              parseNumber(ptr, end, width) &&
              parseNumber(ptr, end, height, DisallowWhitespace)))
          return false;
        setViewBox(FloatRect(x, y, width, height));
        break;
      }
      case ViewTarget: {
        // Ignore arguments.
        skipUntil<CharType>(ptr, end, ')');
        break;
      }
      case ZoomAndPan:
        if (!parseZoomAndPan(ptr, end))
          return false;
        break;
      case PreserveAspectRatio:
        if (!preserveAspectRatio()->parse(ptr, end, false))
          return false;
        break;
      case Transform:
        m_transform->parse(ptr, end);
        break;
      default:
        NOTREACHED();
        break;
    }

    if (!skipExactly<CharType>(ptr, end, ')'))
      return false;

    skipExactly<CharType>(ptr, end, ';');
  }
  return skipExactly<CharType>(ptr, end, ')');
}

}  // namespace blink
