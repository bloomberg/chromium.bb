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
#include "core/svg/SVGSVGElement.h"
#include "core/svg/SVGTransformList.h"
#include "platform/wtf/text/ParsingUtilities.h"

namespace blink {

SVGViewSpec::SVGViewSpec()
    : view_box_(SVGRect::CreateInvalid()),
      preserve_aspect_ratio_(SVGPreserveAspectRatio::Create()),
      transform_(SVGTransformList::Create()) {}

DEFINE_TRACE(SVGViewSpec) {
  visitor->Trace(view_box_);
  visitor->Trace(preserve_aspect_ratio_);
  visitor->Trace(transform_);
}

SVGViewSpec* SVGViewSpec::CreateForElement(SVGSVGElement& root_element) {
  SVGViewSpec* view_spec = root_element.ViewSpec();
  if (!view_spec)
    view_spec = new SVGViewSpec();
  else
    view_spec->Reset();
  view_spec->InheritViewAttributesFromElement(root_element);
  return view_spec;
}

bool SVGViewSpec::ParseViewSpec(const String& spec) {
  if (spec.IsEmpty())
    return false;
  if (spec.Is8Bit()) {
    const LChar* ptr = spec.Characters8();
    const LChar* end = ptr + spec.length();
    return ParseViewSpecInternal(ptr, end);
  }
  const UChar* ptr = spec.Characters16();
  const UChar* end = ptr + spec.length();
  return ParseViewSpecInternal(ptr, end);
}

void SVGViewSpec::SetViewBox(const FloatRect& rect) {
  ViewBox()->SetValue(rect);
}

void SVGViewSpec::SetPreserveAspectRatio(const SVGPreserveAspectRatio& other) {
  PreserveAspectRatio()->SetAlign(other.Align());
  PreserveAspectRatio()->SetMeetOrSlice(other.MeetOrSlice());
}

void SVGViewSpec::Reset() {
  ResetZoomAndPan();
  transform_->Clear();
  SetViewBox(FloatRect());
  PreserveAspectRatio()->SetDefault();
}

namespace {

enum ViewSpecFunctionType {
  kUnknown,
  kPreserveAspectRatio,
  kTransform,
  kViewBox,
  kViewTarget,
  kZoomAndPan,
};

template <typename CharType>
static ViewSpecFunctionType ScanViewSpecFunction(const CharType*& ptr,
                                                 const CharType* end) {
  DCHECK_LT(ptr, end);
  switch (*ptr) {
    case 'v':
      if (SkipToken(ptr, end, "viewBox"))
        return kViewBox;
      if (SkipToken(ptr, end, "viewTarget"))
        return kViewTarget;
      break;
    case 'z':
      if (SkipToken(ptr, end, "zoomAndPan"))
        return kZoomAndPan;
      break;
    case 'p':
      if (SkipToken(ptr, end, "preserveAspectRatio"))
        return kPreserveAspectRatio;
      break;
    case 't':
      if (SkipToken(ptr, end, "transform"))
        return kTransform;
      break;
  }
  return kUnknown;
}

}  // namespace

template <typename CharType>
bool SVGViewSpec::ParseViewSpecInternal(const CharType* ptr,
                                        const CharType* end) {
  if (!SkipToken(ptr, end, "svgView"))
    return false;

  if (!SkipExactly<CharType>(ptr, end, '('))
    return false;

  while (ptr < end && *ptr != ')') {
    ViewSpecFunctionType function_type = ScanViewSpecFunction(ptr, end);
    if (function_type == kUnknown)
      return false;

    if (!SkipExactly<CharType>(ptr, end, '('))
      return false;

    switch (function_type) {
      case kViewBox: {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        if (!(ParseNumber(ptr, end, x) && ParseNumber(ptr, end, y) &&
              ParseNumber(ptr, end, width) &&
              ParseNumber(ptr, end, height, kDisallowWhitespace)))
          return false;
        SetViewBox(FloatRect(x, y, width, height));
        break;
      }
      case kViewTarget: {
        // Ignore arguments.
        SkipUntil<CharType>(ptr, end, ')');
        break;
      }
      case kZoomAndPan:
        if (!ParseZoomAndPan(ptr, end))
          return false;
        break;
      case kPreserveAspectRatio:
        if (!PreserveAspectRatio()->Parse(ptr, end, false))
          return false;
        break;
      case kTransform:
        transform_->Parse(ptr, end);
        break;
      default:
        NOTREACHED();
        break;
    }

    if (!SkipExactly<CharType>(ptr, end, ')'))
      return false;

    SkipExactly<CharType>(ptr, end, ';');
  }
  return SkipExactly<CharType>(ptr, end, ')');
}

}  // namespace blink
