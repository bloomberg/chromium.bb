/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2010 Zoltan Herczeg <zherczeg@webkit.org>
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

#ifndef FEConvolveMatrix_h
#define FEConvolveMatrix_h

#include "platform/geometry/IntPoint.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/filters/Filter.h"
#include "platform/graphics/filters/FilterEffect.h"
#include "platform/wtf/Vector.h"

namespace blink {

enum EdgeModeType {
  EDGEMODE_UNKNOWN = 0,
  EDGEMODE_DUPLICATE = 1,
  EDGEMODE_WRAP = 2,
  EDGEMODE_NONE = 3
};

class PLATFORM_EXPORT FEConvolveMatrix final : public FilterEffect {
 public:
  static FEConvolveMatrix* Create(Filter*,
                                  const IntSize&,
                                  float,
                                  float,
                                  const IntPoint&,
                                  EdgeModeType,
                                  bool,
                                  const Vector<float>&);

  bool SetDivisor(float);
  bool SetBias(float);
  bool SetTargetOffset(const IntPoint&);
  bool SetEdgeMode(EdgeModeType);
  bool SetPreserveAlpha(bool);

  TextStream& ExternalRepresentation(TextStream&, int indention) const override;

 private:
  FEConvolveMatrix(Filter*,
                   const IntSize&,
                   float,
                   float,
                   const IntPoint&,
                   EdgeModeType,
                   bool,
                   const Vector<float>&);

  FloatRect MapEffect(const FloatRect&) const final;

  sk_sp<PaintFilter> CreateImageFilter() override;

  bool ParametersValid() const;

  IntSize kernel_size_;
  float divisor_;
  float bias_;
  IntPoint target_offset_;
  EdgeModeType edge_mode_;
  bool preserve_alpha_;
  Vector<float> kernel_matrix_;
};

}  // namespace blink

#endif  // FEConvolveMatrix_h
