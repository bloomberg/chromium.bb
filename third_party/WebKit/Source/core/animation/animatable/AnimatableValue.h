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

#ifndef AnimatableValue_h
#define AnimatableValue_h

#include "core/CoreExport.h"
#include "core/css/CSSValue.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/RefCounted.h"

namespace blink {

class CORE_EXPORT AnimatableValue : public RefCounted<AnimatableValue> {
 public:
  virtual ~AnimatableValue() {}

  static RefPtr<AnimatableValue> Interpolate(const AnimatableValue*,
                                             const AnimatableValue*,
                                             double fraction);
  bool IsDouble() const { return GetType() == kTypeDouble; }
  bool IsFilterOperations() const { return GetType() == kTypeFilterOperations; }
  bool IsTransform() const { return GetType() == kTypeTransform; }
  bool IsUnknown() const { return GetType() == kTypeUnknown; }

  bool IsSameType(const AnimatableValue* value) const {
    DCHECK(value);
    return value->GetType() == GetType();
  }

 protected:
  enum AnimatableType {
    kTypeDouble,
    kTypeFilterOperations,
    kTypeTransform,
    kTypeUnknown,
  };

  virtual RefPtr<AnimatableValue> InterpolateTo(const AnimatableValue*,
                                                double fraction) const {
    NOTREACHED();
    return nullptr;
  }
  static RefPtr<AnimatableValue> DefaultInterpolateTo(
      const AnimatableValue* left,
      const AnimatableValue* right,
      double fraction) {
    return TakeConstRef((fraction < 0.5) ? left : right);
  }

  template <class T>
  static RefPtr<T> TakeConstRef(const T* value) {
    return RefPtr<T>(const_cast<T*>(value));
  }

 private:
  virtual AnimatableType GetType() const = 0;

  template <class Keyframe>
  friend class KeyframeEffectModel;
};

#define DEFINE_ANIMATABLE_VALUE_TYPE_CASTS(thisType, predicate)         \
  DEFINE_TYPE_CASTS(thisType, AnimatableValue, value, value->predicate, \
                    value.predicate)

}  // namespace blink

#endif  // AnimatableValue_h
