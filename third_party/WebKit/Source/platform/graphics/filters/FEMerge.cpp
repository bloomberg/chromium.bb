/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
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

#include "platform/graphics/filters/FEMerge.h"

#include <memory>
#include "SkMergeImageFilter.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "platform/text/TextStream.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

FEMerge::FEMerge(Filter* filter) : FilterEffect(filter) {}

FEMerge* FEMerge::Create(Filter* filter) {
  return new FEMerge(filter);
}

sk_sp<SkImageFilter> FEMerge::CreateImageFilter() {
  unsigned size = NumberOfEffectInputs();

  std::unique_ptr<sk_sp<SkImageFilter>[]> input_refs =
      WrapArrayUnique(new sk_sp<SkImageFilter>[size]);
  for (unsigned i = 0; i < size; ++i) {
    input_refs[i] = SkiaImageFilterBuilder::Build(
        InputEffect(i), OperatingInterpolationSpace());
  }
  SkImageFilter::CropRect rect = GetCropRect();
  return SkMergeImageFilter::Make(input_refs.get(), size, &rect);
}

TextStream& FEMerge::ExternalRepresentation(TextStream& ts, int indent) const {
  WriteIndent(ts, indent);
  ts << "[feMerge";
  FilterEffect::ExternalRepresentation(ts);
  unsigned size = NumberOfEffectInputs();
  ts << " mergeNodes=\"" << size << "\"]\n";
  for (unsigned i = 0; i < size; ++i)
    InputEffect(i)->ExternalRepresentation(ts, indent + 1);
  return ts;
}

}  // namespace blink
