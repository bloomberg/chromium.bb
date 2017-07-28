// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ListInterpolationFunctions_h
#define ListInterpolationFunctions_h

#include <memory>
#include "core/animation/InterpolationValue.h"
#include "core/animation/PairwiseInterpolationValue.h"
#include "platform/wtf/Vector.h"

namespace blink {

class UnderlyingValueOwner;
class InterpolationType;

class ListInterpolationFunctions {
 public:
  template <typename CreateItemCallback>
  static InterpolationValue CreateList(size_t length, CreateItemCallback);
  static InterpolationValue CreateEmptyList() {
    return InterpolationValue(InterpolableList::Create(0));
  }

  enum class LengthMatchingStrategy { kLowestCommonMultiple, kPadToLargest };

  using MergeSingleItemConversionsCallback =
      PairwiseInterpolationValue (*)(InterpolationValue&& start,
                                     InterpolationValue&& end);
  static PairwiseInterpolationValue MaybeMergeSingles(
      InterpolationValue&& start,
      InterpolationValue&& end,
      LengthMatchingStrategy,
      MergeSingleItemConversionsCallback);

  using EqualNonInterpolableValuesCallback =
      bool (*)(const NonInterpolableValue*, const NonInterpolableValue*);
  static bool EqualValues(const InterpolationValue&,
                          const InterpolationValue&,
                          EqualNonInterpolableValuesCallback);

  using NonInterpolableValuesAreCompatibleCallback =
      bool (*)(const NonInterpolableValue*, const NonInterpolableValue*);
  using CompositeItemCallback = void (*)(std::unique_ptr<InterpolableValue>&,
                                         RefPtr<NonInterpolableValue>&,
                                         double underlying_fraction,
                                         const InterpolableValue&,
                                         const NonInterpolableValue*);
  static void Composite(UnderlyingValueOwner&,
                        double underlying_fraction,
                        const InterpolationType&,
                        const InterpolationValue&,
                        LengthMatchingStrategy,
                        NonInterpolableValuesAreCompatibleCallback,
                        CompositeItemCallback);
};

class NonInterpolableList : public NonInterpolableValue {
 public:
  ~NonInterpolableList() final {}

  static RefPtr<NonInterpolableList> Create() {
    return AdoptRef(new NonInterpolableList());
  }
  static RefPtr<NonInterpolableList> Create(
      Vector<RefPtr<NonInterpolableValue>>&& list) {
    return AdoptRef(new NonInterpolableList(std::move(list)));
  }

  size_t length() const { return list_.size(); }
  const NonInterpolableValue* Get(size_t index) const {
    return list_[index].Get();
  }
  NonInterpolableValue* Get(size_t index) { return list_[index].Get(); }
  RefPtr<NonInterpolableValue>& GetMutable(size_t index) {
    return list_[index];
  }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  NonInterpolableList() {}
  NonInterpolableList(Vector<RefPtr<NonInterpolableValue>>&& list)
      : list_(list) {}

  Vector<RefPtr<NonInterpolableValue>> list_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(NonInterpolableList);

template <typename CreateItemCallback>
InterpolationValue ListInterpolationFunctions::CreateList(
    size_t length,
    CreateItemCallback create_item) {
  if (length == 0)
    return CreateEmptyList();
  std::unique_ptr<InterpolableList> interpolable_list =
      InterpolableList::Create(length);
  Vector<RefPtr<NonInterpolableValue>> non_interpolable_values(length);
  for (size_t i = 0; i < length; i++) {
    InterpolationValue item = create_item(i);
    if (!item)
      return nullptr;
    interpolable_list->Set(i, std::move(item.interpolable_value));
    non_interpolable_values[i] = std::move(item.non_interpolable_value);
  }
  return InterpolationValue(
      std::move(interpolable_list),
      NonInterpolableList::Create(std::move(non_interpolable_values)));
}

}  // namespace blink

#endif  // ListInterpolationFunctions_h
