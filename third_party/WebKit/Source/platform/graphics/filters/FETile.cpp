/*
 * Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
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

#include "platform/graphics/filters/FETile.h"

#include "SkTileImageFilter.h"
#include "platform/graphics/filters/Filter.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "platform/text/TextStream.h"

namespace blink {

FETile::FETile(Filter* filter) : FilterEffect(filter) {}

FETile* FETile::Create(Filter* filter) {
  return new FETile(filter);
}

FloatRect FETile::MapInputs(const FloatRect& rect) const {
  return AbsoluteBounds();
}

sk_sp<SkImageFilter> FETile::CreateImageFilter() {
  sk_sp<SkImageFilter> input(SkiaImageFilterBuilder::Build(
      InputEffect(0), OperatingInterpolationSpace()));
  FloatRect src_rect;
  if (InputEffect(0)->GetFilterEffectType() == kFilterEffectTypeSourceInput)
    src_rect = GetFilter()->FilterRegion();
  else
    src_rect = InputEffect(0)->FilterPrimitiveSubregion();
  FloatRect dst_rect = FilterPrimitiveSubregion();
  return SkTileImageFilter::Make(src_rect, dst_rect, std::move(input));
}

TextStream& FETile::ExternalRepresentation(TextStream& ts, int indent) const {
  WriteIndent(ts, indent);
  ts << "[feTile";
  FilterEffect::ExternalRepresentation(ts);
  ts << "]\n";
  InputEffect(0)->ExternalRepresentation(ts, indent + 1);

  return ts;
}

}  // namespace blink
