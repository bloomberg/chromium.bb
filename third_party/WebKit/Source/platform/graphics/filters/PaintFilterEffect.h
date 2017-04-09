// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintFilterEffect_h
#define PaintFilterEffect_h

#include "platform/graphics/filters/FilterEffect.h"
#include "platform/graphics/paint/PaintFlags.h"

namespace blink {

class PLATFORM_EXPORT PaintFilterEffect : public FilterEffect {
 public:
  static PaintFilterEffect* Create(Filter*, const PaintFlags&);
  ~PaintFilterEffect() override;

  FilterEffectType GetFilterEffectType() const override {
    return kFilterEffectTypeSourceInput;
  }

  TextStream& ExternalRepresentation(TextStream&, int indention) const override;
  sk_sp<SkImageFilter> CreateImageFilter() override;

 private:
  PaintFilterEffect(Filter*, const PaintFlags&);

  PaintFlags flags_;
};

}  // namespace blink

#endif  // PaintFilterEffect_h
