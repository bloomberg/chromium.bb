/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AnimatableFilterOperations_h
#define AnimatableFilterOperations_h

#include "core/animation/animatable/AnimatableValue.h"
#include "core/style/FilterOperations.h"

namespace blink {

class AnimatableFilterOperations final : public AnimatableValue {
 public:
  static RefPtr<AnimatableFilterOperations> Create(
      const FilterOperations& operations) {
    return WTF::AdoptRef(new AnimatableFilterOperations(operations));
  }

  ~AnimatableFilterOperations() override {}

  const FilterOperations& Operations() const {
    return operation_wrapper_->Operations();
  }

 protected:
  RefPtr<AnimatableValue> InterpolateTo(const AnimatableValue*,
                                        double fraction) const override;

 private:
  AnimatableFilterOperations(const FilterOperations& operations)
      : operation_wrapper_(FilterOperationsWrapper::Create(operations)) {}

  AnimatableType GetType() const override { return kTypeFilterOperations; }

  Persistent<FilterOperationsWrapper> operation_wrapper_;
};

DEFINE_ANIMATABLE_VALUE_TYPE_CASTS(AnimatableFilterOperations,
                                   IsFilterOperations());

}  // namespace blink

#endif  // AnimatableFilterOperations_h
