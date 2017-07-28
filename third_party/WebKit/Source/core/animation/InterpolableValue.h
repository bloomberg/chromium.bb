// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterpolableValue_h
#define InterpolableValue_h

#include <memory>
#include "core/CoreExport.h"
#include "core/animation/animatable/AnimatableValue.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Vector.h"

namespace blink {

// Represents the components of a PropertySpecificKeyframe's value that change
// smoothly as it interpolates to an adjacent value.
class CORE_EXPORT InterpolableValue {
  USING_FAST_MALLOC(InterpolableValue);

 public:
  virtual ~InterpolableValue() {}

  virtual bool IsNumber() const { return false; }
  virtual bool IsBool() const { return false; }
  virtual bool IsList() const { return false; }
  virtual bool IsAnimatableValue() const { return false; }

  virtual bool Equals(const InterpolableValue&) const = 0;
  virtual std::unique_ptr<InterpolableValue> Clone() const = 0;
  virtual std::unique_ptr<InterpolableValue> CloneAndZero() const = 0;
  virtual void Scale(double scale) = 0;
  virtual void ScaleAndAdd(double scale, const InterpolableValue& other) = 0;

 private:
  virtual void Interpolate(const InterpolableValue& to,
                           const double progress,
                           InterpolableValue& result) const = 0;

  friend class LegacyStyleInterpolation;
  friend class TransitionInterpolation;
  friend class PairwisePrimitiveInterpolation;

  // Keep interpolate private, but allow calls within the hierarchy without
  // knowledge of type.
  friend class InterpolableNumber;
  friend class InterpolableBool;
  friend class InterpolableList;

  friend class AnimationInterpolableValueTest;
};

class CORE_EXPORT InterpolableNumber final : public InterpolableValue {
 public:
  static std::unique_ptr<InterpolableNumber> Create(double value) {
    return WTF::WrapUnique(new InterpolableNumber(value));
  }

  bool IsNumber() const final { return true; }
  double Value() const { return value_; }
  bool Equals(const InterpolableValue& other) const final;
  std::unique_ptr<InterpolableValue> Clone() const final {
    return Create(value_);
  }
  std::unique_ptr<InterpolableValue> CloneAndZero() const final {
    return Create(0);
  }
  void Scale(double scale) final;
  void ScaleAndAdd(double scale, const InterpolableValue& other) final;
  void Set(double value) { value_ = value; }

 private:
  void Interpolate(const InterpolableValue& to,
                   const double progress,
                   InterpolableValue& result) const final;
  double value_;

  explicit InterpolableNumber(double value) : value_(value) {}
};

class CORE_EXPORT InterpolableList : public InterpolableValue {
 public:
  // Explicitly delete operator= because MSVC automatically generate
  // copy constructors and operator= for dll-exported classes.
  // Since InterpolableList is not copyable, automatically generated
  // operator= causes MSVC compiler error.
  // However, we cannot use WTF_MAKE_NONCOPYABLE because InterpolableList
  // has its own copy constructor. So just delete operator= here.
  InterpolableList& operator=(const InterpolableList&) = delete;

  static std::unique_ptr<InterpolableList> Create(
      const InterpolableList& other) {
    return WTF::WrapUnique(new InterpolableList(other));
  }

  static std::unique_ptr<InterpolableList> Create(size_t size) {
    return WTF::WrapUnique(new InterpolableList(size));
  }

  bool IsList() const final { return true; }
  void Set(size_t position, std::unique_ptr<InterpolableValue> value) {
    values_[position] = std::move(value);
  }
  const InterpolableValue* Get(size_t position) const {
    return values_[position].get();
  }
  std::unique_ptr<InterpolableValue>& GetMutable(size_t position) {
    return values_[position];
  }
  size_t length() const { return values_.size(); }
  bool Equals(const InterpolableValue& other) const final;
  std::unique_ptr<InterpolableValue> Clone() const final {
    return Create(*this);
  }
  std::unique_ptr<InterpolableValue> CloneAndZero() const final;
  void Scale(double scale) final;
  void ScaleAndAdd(double scale, const InterpolableValue& other) final;

 private:
  void Interpolate(const InterpolableValue& to,
                   const double progress,
                   InterpolableValue& result) const final;
  explicit InterpolableList(size_t size) : values_(size) {}

  InterpolableList(const InterpolableList& other) : values_(other.length()) {
    for (size_t i = 0; i < length(); i++)
      Set(i, other.values_[i]->Clone());
  }

  Vector<std::unique_ptr<InterpolableValue>> values_;
};

// FIXME: Remove this when we can.
class InterpolableAnimatableValue : public InterpolableValue {
 public:
  static std::unique_ptr<InterpolableAnimatableValue> Create(
      PassRefPtr<AnimatableValue> value) {
    return WTF::WrapUnique(new InterpolableAnimatableValue(std::move(value)));
  }

  bool IsAnimatableValue() const final { return true; }
  AnimatableValue* Value() const { return value_.Get(); }
  bool Equals(const InterpolableValue&) const final {
    NOTREACHED();
    return false;
  }
  std::unique_ptr<InterpolableValue> Clone() const final {
    return Create(value_);
  }
  std::unique_ptr<InterpolableValue> CloneAndZero() const final {
    NOTREACHED();
    return nullptr;
  }
  void Scale(double scale) final { NOTREACHED(); }
  void ScaleAndAdd(double scale, const InterpolableValue& other) final {
    NOTREACHED();
  }

 private:
  void Interpolate(const InterpolableValue& to,
                   const double progress,
                   InterpolableValue& result) const final;
  RefPtr<AnimatableValue> value_;

  InterpolableAnimatableValue(PassRefPtr<AnimatableValue> value)
      : value_(std::move(value)) {}
};

DEFINE_TYPE_CASTS(InterpolableNumber,
                  InterpolableValue,
                  value,
                  value->IsNumber(),
                  value.IsNumber());
DEFINE_TYPE_CASTS(InterpolableList,
                  InterpolableValue,
                  value,
                  value->IsList(),
                  value.IsList());
DEFINE_TYPE_CASTS(InterpolableAnimatableValue,
                  InterpolableValue,
                  value,
                  value->IsAnimatableValue(),
                  value.IsAnimatableValue());

}  // namespace blink

#endif
