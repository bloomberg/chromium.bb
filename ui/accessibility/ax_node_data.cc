// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_node_data.h"

#include <set>

#include "base/containers/hash_tables.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

using base::DoubleToString;
using base::IntToString;

namespace ui {

AXNodeData::AXNodeData()
    : id(-1),
      role(AX_ROLE_UNKNOWN),
      state(-1) {
}

AXNodeData::~AXNodeData() {
}

void AXNodeData::AddStringAttribute(
    StringAttribute attribute, const std::string& value) {
  string_attributes.push_back(std::make_pair(attribute, value));
}

void AXNodeData::AddIntAttribute(
    IntAttribute attribute, int value) {
  int_attributes.push_back(std::make_pair(attribute, value));
}

void AXNodeData::AddFloatAttribute(
    FloatAttribute attribute, float value) {
  float_attributes.push_back(std::make_pair(attribute, value));
}

void AXNodeData::AddBoolAttribute(
    BoolAttribute attribute, bool value) {
  bool_attributes.push_back(std::make_pair(attribute, value));
}

void AXNodeData::AddIntListAttribute(
    IntListAttribute attribute, const std::vector<int32>& value) {
  intlist_attributes.push_back(std::make_pair(attribute, value));
}

void AXNodeData::SetName(std::string name) {
  string_attributes.push_back(std::make_pair(ATTR_NAME, name));
}

void AXNodeData::SetValue(std::string value) {
  string_attributes.push_back(std::make_pair(ATTR_VALUE, value));
}

}  // namespace ui
