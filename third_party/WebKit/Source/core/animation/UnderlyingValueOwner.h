// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UnderlyingValueOwner_h
#define UnderlyingValueOwner_h

#include <memory>
#include "core/CoreExport.h"
#include "core/animation/TypedInterpolationValue.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

// Handles memory management of underlying InterpolationValues in applyStack()
// Ensures we perform copy on write if we are not the owner of an underlying
// InterpolationValue. This functions similar to a DataRef except on
// std::unique_ptr'd objects.
class CORE_EXPORT UnderlyingValueOwner {
  WTF_MAKE_NONCOPYABLE(UnderlyingValueOwner);
  STACK_ALLOCATED();

 public:
  UnderlyingValueOwner()
      : type_(nullptr), value_owner_(nullptr), value_(nullptr) {}

  operator bool() const {
    DCHECK_EQ(static_cast<bool>(type_), static_cast<bool>(value_));
    return type_;
  }

  const InterpolationType& GetType() const {
    DCHECK(type_);
    return *type_;
  }

  const InterpolationValue& Value() const;

  void Set(std::nullptr_t);
  void Set(const InterpolationType&, const InterpolationValue&);
  void Set(const InterpolationType&, InterpolationValue&&);
  void Set(std::unique_ptr<TypedInterpolationValue>);
  void Set(const TypedInterpolationValue*);

  InterpolationValue& MutableValue();

 private:
  const InterpolationType* type_;
  InterpolationValue value_owner_;
  const InterpolationValue* value_;
};

}  // namespace blink

#endif  // UnderlyingValueOwner_h
