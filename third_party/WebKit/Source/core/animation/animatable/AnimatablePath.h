// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimatablePath_h
#define AnimatablePath_h

#include "core/CoreExport.h"
#include "core/animation/animatable/AnimatableValue.h"
#include "core/style/StylePath.h"

namespace blink {

class AnimatablePath final : public AnimatableValue {
 public:
  ~AnimatablePath() override {}
  static PassRefPtr<AnimatablePath> Create(PassRefPtr<StylePath> path) {
    return AdoptRef(new AnimatablePath(std::move(path)));
  }

 private:
  explicit AnimatablePath(PassRefPtr<StylePath> path) : path_(path) {}
  AnimatableType GetType() const override { return kTypePath; }
  bool EqualTo(const AnimatableValue*) const override;
  const RefPtr<StylePath> path_;
};

DEFINE_ANIMATABLE_VALUE_TYPE_CASTS(AnimatablePath, IsPath());

}  // namespace blink

#endif  // AnimatablePath_h
