// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/feature_policy/feature_policy.h"

namespace blink {

PolicyValue::PolicyValue() : type_(mojom::PolicyValueType::kNull) {}

PolicyValue::~PolicyValue() = default;

PolicyValue::PolicyValue(mojom::PolicyValueType type) : type_(type) {
  DCHECK_EQ(type, mojom::PolicyValueType::kNull);
}

PolicyValue::PolicyValue(bool bool_value)
    : type_(mojom::PolicyValueType::kBool), bool_value_(bool_value) {}

PolicyValue PolicyValue::CreateMaxPolicyValue(mojom::PolicyValueType type) {
  PolicyValue value;
  value.SetType(type);
  value.SetToMax();
  return value;
}

PolicyValue PolicyValue::CreateMinPolicyValue(mojom::PolicyValueType type) {
  PolicyValue value;
  value.SetType(type);
  value.SetToMin();
  return value;
}

bool PolicyValue::BoolValue() const {
  DCHECK_EQ(type_, mojom::PolicyValueType::kBool);
  return bool_value_;
}

void PolicyValue::SetBoolValue(bool bool_value) {
  type_ = mojom::PolicyValueType::kBool;
  bool_value_ = bool_value;
}

PolicyValue& PolicyValue::operator=(const PolicyValue& rhs) {
  if (this != &rhs) {
    type_ = rhs.type_;
    bool_value_ = rhs.bool_value_;
  }
  return *this;
}

PolicyValue PolicyValue::Combine(const PolicyValue& lhs,
                                 const PolicyValue& rhs) {
  DCHECK_EQ(lhs.Type(), rhs.Type());
  PolicyValue final_value;
  final_value.SetType(lhs.Type());
  switch (lhs.Type()) {
    case mojom::PolicyValueType::kBool:
      final_value.SetBoolValue(lhs.BoolValue() && rhs.BoolValue());
      break;
    default:
      NOTREACHED();
  }
  return final_value;
}

void PolicyValue::Combine(const PolicyValue& rhs) {
  DCHECK_EQ(type_, rhs.Type());
  switch (type_) {
    case mojom::PolicyValueType::kBool:
      SetBoolValue(bool_value_ && rhs.BoolValue());
      break;
    default:
      NOTREACHED();
  }
}

bool operator==(const PolicyValue& lhs, const PolicyValue& rhs) {
  if (lhs.Type() != rhs.Type())
    return false;
  switch (lhs.Type()) {
    case mojom::PolicyValueType::kBool:
      return lhs.BoolValue() == rhs.BoolValue();
    case mojom::PolicyValueType::kNull:
      return true;
  }
  NOTREACHED();
  return false;
}

bool operator!=(const PolicyValue& lhs, const PolicyValue& rhs) {
  return !(lhs == rhs);
}

bool operator<(const PolicyValue& lhs, const PolicyValue& rhs) {
  DCHECK_EQ(lhs.Type(), rhs.Type());
  switch (lhs.Type()) {
    case mojom::PolicyValueType::kBool:
      return rhs.BoolValue();
    case mojom::PolicyValueType::kNull:
      NOTREACHED();
      break;
  }
  return false;
}

bool operator<=(const PolicyValue& lhs, const PolicyValue& rhs) {
  return lhs < rhs || lhs == rhs;
}

bool operator>(const PolicyValue& lhs, const PolicyValue& rhs) {
  return rhs < lhs;
}

bool operator>=(const PolicyValue& lhs, const PolicyValue& rhs) {
  return rhs < lhs || rhs == lhs;
}

void PolicyValue::SetToMax() {
  switch (type_) {
    case mojom::PolicyValueType::kBool:
      bool_value_ = true;
      break;
    default:
      break;
  }
  return;
}

void PolicyValue::SetToMin() {
  switch (type_) {
    case mojom::PolicyValueType::kBool:
      bool_value_ = false;
      break;
    default:
      NOTREACHED();
  }
  return;
}

}  // namespace blink
