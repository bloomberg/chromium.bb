/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "platform/graphics/filters/SourceAlpha.h"

#include "platform/graphics/filters/Filter.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "platform/text/TextStream.h"
#include "platform/wtf/text/WTFString.h"
#include "third_party/skia/include/effects/SkColorFilterImageFilter.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"

namespace blink {

SourceAlpha* SourceAlpha::Create(FilterEffect* source_effect) {
  return new SourceAlpha(source_effect);
}

SourceAlpha::SourceAlpha(FilterEffect* source_effect)
    : FilterEffect(source_effect->GetFilter()) {
  SetOperatingInterpolationSpace(source_effect->OperatingInterpolationSpace());
  InputEffects().push_back(source_effect);
}

sk_sp<SkImageFilter> SourceAlpha::CreateImageFilter() {
  sk_sp<SkImageFilter> source_graphic(SkiaImageFilterBuilder::Build(
      InputEffect(0), OperatingInterpolationSpace()));
  SkScalar matrix[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0,          0,
                         0, 0, 0, 0, 0, 0, 0, 0, SK_Scalar1, 0};
  sk_sp<SkColorFilter> color_filter =
      SkColorFilter::MakeMatrixFilterRowMajor255(matrix);
  return SkColorFilterImageFilter::Make(std::move(color_filter),
                                        std::move(source_graphic));
}

TextStream& SourceAlpha::ExternalRepresentation(TextStream& ts,
                                                int indent) const {
  WriteIndent(ts, indent);
  ts << "[SourceAlpha]\n";
  return ts;
}

}  // namespace blink
