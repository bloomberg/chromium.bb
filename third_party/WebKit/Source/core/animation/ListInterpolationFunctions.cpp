// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/ListInterpolationFunctions.h"

#include "core/animation/UnderlyingValueOwner.h"
#include "core/css/CSSValueList.h"
#include "wtf/MathExtras.h"
#include <memory>

namespace blink {

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(NonInterpolableList);

bool ListInterpolationFunctions::EqualValues(
    const InterpolationValue& a,
    const InterpolationValue& b,
    EqualNonInterpolableValuesCallback equal_non_interpolable_values) {
  if (!a && !b)
    return true;

  if (!a || !b)
    return false;

  const InterpolableList& interpolable_list_a =
      ToInterpolableList(*a.interpolable_value);
  const InterpolableList& interpolable_list_b =
      ToInterpolableList(*b.interpolable_value);

  if (interpolable_list_a.length() != interpolable_list_b.length())
    return false;

  size_t length = interpolable_list_a.length();
  if (length == 0)
    return true;

  const NonInterpolableList& non_interpolable_list_a =
      ToNonInterpolableList(*a.non_interpolable_value);
  const NonInterpolableList& non_interpolable_list_b =
      ToNonInterpolableList(*b.non_interpolable_value);

  for (size_t i = 0; i < length; i++) {
    if (!equal_non_interpolable_values(non_interpolable_list_a.Get(i),
                                       non_interpolable_list_b.Get(i)))
      return false;
  }
  return true;
}

PairwiseInterpolationValue ListInterpolationFunctions::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end,
    MergeSingleItemConversionsCallback merge_single_item_conversions) {
  size_t start_length = ToInterpolableList(*start.interpolable_value).length();
  size_t end_length = ToInterpolableList(*end.interpolable_value).length();

  if (start_length == 0 && end_length == 0) {
    return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                      std::move(end.interpolable_value),
                                      nullptr);
  }

  if (start_length == 0) {
    std::unique_ptr<InterpolableValue> start_interpolable_value =
        end.interpolable_value->CloneAndZero();
    return PairwiseInterpolationValue(std::move(start_interpolable_value),
                                      std::move(end.interpolable_value),
                                      std::move(end.non_interpolable_value));
  }

  if (end_length == 0) {
    std::unique_ptr<InterpolableValue> end_interpolable_value =
        start.interpolable_value->CloneAndZero();
    return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                      std::move(end_interpolable_value),
                                      std::move(start.non_interpolable_value));
  }

  size_t final_length = lowestCommonMultiple(start_length, end_length);
  std::unique_ptr<InterpolableList> result_start_interpolable_list =
      InterpolableList::Create(final_length);
  std::unique_ptr<InterpolableList> result_end_interpolable_list =
      InterpolableList::Create(final_length);
  Vector<RefPtr<NonInterpolableValue>> result_non_interpolable_values(
      final_length);

  InterpolableList& start_interpolable_list =
      ToInterpolableList(*start.interpolable_value);
  InterpolableList& end_interpolable_list =
      ToInterpolableList(*end.interpolable_value);
  NonInterpolableList& start_non_interpolable_list =
      ToNonInterpolableList(*start.non_interpolable_value);
  NonInterpolableList& end_non_interpolable_list =
      ToNonInterpolableList(*end.non_interpolable_value);

  for (size_t i = 0; i < final_length; i++) {
    InterpolationValue start(
        start_interpolable_list.Get(i % start_length)->Clone(),
        start_non_interpolable_list.Get(i % start_length));
    InterpolationValue end(end_interpolable_list.Get(i % end_length)->Clone(),
                           end_non_interpolable_list.Get(i % end_length));
    PairwiseInterpolationValue result =
        merge_single_item_conversions(std::move(start), std::move(end));
    if (!result)
      return nullptr;
    result_start_interpolable_list->Set(
        i, std::move(result.start_interpolable_value));
    result_end_interpolable_list->Set(i,
                                      std::move(result.end_interpolable_value));
    result_non_interpolable_values[i] =
        std::move(result.non_interpolable_value);
  }

  return PairwiseInterpolationValue(
      std::move(result_start_interpolable_list),
      std::move(result_end_interpolable_list),
      NonInterpolableList::Create(std::move(result_non_interpolable_values)));
}

static void RepeatToLength(InterpolationValue& value, size_t length) {
  InterpolableList& interpolable_list =
      ToInterpolableList(*value.interpolable_value);
  NonInterpolableList& non_interpolable_list =
      ToNonInterpolableList(*value.non_interpolable_value);
  size_t current_length = interpolable_list.length();
  DCHECK_GT(current_length, 0U);
  if (current_length == length)
    return;
  DCHECK_LT(current_length, length);
  std::unique_ptr<InterpolableList> new_interpolable_list =
      InterpolableList::Create(length);
  Vector<RefPtr<NonInterpolableValue>> new_non_interpolable_values(length);
  for (size_t i = length; i-- > 0;) {
    new_interpolable_list->Set(
        i, i < current_length
               ? std::move(interpolable_list.GetMutable(i))
               : interpolable_list.Get(i % current_length)->Clone());
    new_non_interpolable_values[i] =
        non_interpolable_list.Get(i % current_length);
  }
  value.interpolable_value = std::move(new_interpolable_list);
  value.non_interpolable_value =
      NonInterpolableList::Create(std::move(new_non_interpolable_values));
}

static bool NonInterpolableListsAreCompatible(
    const NonInterpolableList& a,
    const NonInterpolableList& b,
    size_t length,
    ListInterpolationFunctions::NonInterpolableValuesAreCompatibleCallback
        non_interpolable_values_are_compatible) {
  for (size_t i = 0; i < length; i++) {
    if (!non_interpolable_values_are_compatible(a.Get(i % a.length()),
                                                b.Get(i % b.length())))
      return false;
  }
  return true;
}

void ListInterpolationFunctions::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationType& type,
    const InterpolationValue& value,
    NonInterpolableValuesAreCompatibleCallback
        non_interpolable_values_are_compatible,
    CompositeItemCallback composite_item) {
  size_t underlying_length =
      ToInterpolableList(*underlying_value_owner.Value().interpolable_value)
          .length();
  if (underlying_length == 0) {
    DCHECK(!underlying_value_owner.Value().non_interpolable_value);
    underlying_value_owner.Set(type, value);
    return;
  }

  const InterpolableList& interpolable_list =
      ToInterpolableList(*value.interpolable_value);
  size_t value_length = interpolable_list.length();
  if (value_length == 0) {
    DCHECK(!value.non_interpolable_value);
    underlying_value_owner.MutableValue().interpolable_value->Scale(
        underlying_fraction);
    return;
  }

  const NonInterpolableList& non_interpolable_list =
      ToNonInterpolableList(*value.non_interpolable_value);
  size_t new_length = lowestCommonMultiple(underlying_length, value_length);
  if (!NonInterpolableListsAreCompatible(
          ToNonInterpolableList(
              *underlying_value_owner.Value().non_interpolable_value),
          non_interpolable_list, new_length,
          non_interpolable_values_are_compatible)) {
    underlying_value_owner.Set(type, value);
    return;
  }

  InterpolationValue& underlying_value = underlying_value_owner.MutableValue();
  if (underlying_length < new_length)
    RepeatToLength(underlying_value, new_length);

  InterpolableList& underlying_interpolable_list =
      ToInterpolableList(*underlying_value.interpolable_value);
  NonInterpolableList& underlying_non_interpolable_list =
      ToNonInterpolableList(*underlying_value.non_interpolable_value);
  for (size_t i = 0; i < new_length; i++) {
    composite_item(underlying_interpolable_list.GetMutable(i),
                   underlying_non_interpolable_list.GetMutable(i),
                   underlying_fraction,
                   *interpolable_list.Get(i % value_length),
                   non_interpolable_list.Get(i % value_length));
  }
}

}  // namespace blink
