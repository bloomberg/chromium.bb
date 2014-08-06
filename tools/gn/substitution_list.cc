// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/substitution_list.h"

#include <string.h>

#include "tools/gn/value.h"

SubstitutionList::SubstitutionList() {
}

SubstitutionList::~SubstitutionList() {
}

bool SubstitutionList::Parse(const Value& value, Err* err) {
  if (!value.VerifyTypeIs(Value::LIST, err))
    return false;

  const std::vector<Value>& input_list = value.list_value();
  list_.resize(input_list.size());
  for (size_t i = 0; i < input_list.size(); i++) {
    if (!list_[i].Parse(input_list[i], err))
      return false;
  }

  FillRequiredTypes();
  return true;
}

bool SubstitutionList::Parse(const std::vector<std::string>& values,
                             const ParseNode* origin,
                             Err* err) {
  list_.resize(values.size());
  for (size_t i = 0; i < values.size(); i++) {
    if (!list_[i].Parse(values[i], origin, err))
      return false;
  }

  FillRequiredTypes();
  return true;
}

SubstitutionList SubstitutionList::MakeForTest(
    const char* a,
    const char* b,
    const char* c) {
  std::vector<std::string> input_strings;
  input_strings.push_back(a);
  if (b)
    input_strings.push_back(b);
  if (c)
    input_strings.push_back(c);

  Err err;
  SubstitutionList result;
  result.Parse(input_strings, NULL, &err);
  return result;
}

void SubstitutionList::FillRequiredTypes() {
  bool required_type_bits[SUBSTITUTION_NUM_TYPES];
  memset(&required_type_bits, 0, SUBSTITUTION_NUM_TYPES);
  for (size_t i = 0; i < list_.size(); i++)
    list_[i].FillRequiredTypes(required_type_bits);

  for (size_t i = SUBSTITUTION_FIRST_PATTERN; i < SUBSTITUTION_NUM_TYPES; i++) {
    if (required_type_bits[i])
      required_types_.push_back(static_cast<SubstitutionType>(i));
  }
}
