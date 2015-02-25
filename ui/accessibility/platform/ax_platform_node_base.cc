// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_base.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"

namespace ui {

namespace {

// Predicate that returns true if the first value of a pair is |first|.
template<typename FirstType, typename SecondType>
struct FirstIs {
  FirstIs(FirstType first)
      : first_(first) {}
  bool operator()(std::pair<FirstType, SecondType> const& p) {
    return p.first == first_;
  }
  FirstType first_;
};

// Helper function that finds in a vector of pairs by matching on the
// first value, and returns an iterator.
template<typename FirstType, typename SecondType>
typename std::vector<std::pair<FirstType, SecondType>>::const_iterator
    FindInVectorOfPairs(
        FirstType first,
        const std::vector<std::pair<FirstType, SecondType>>& vector) {
  return std::find_if(vector.begin(),
                      vector.end(),
                      FirstIs<FirstType, SecondType>(first));
}

}  // namespace

void AXPlatformNodeBase::Init(AXPlatformNodeDelegate* delegate) {
  delegate_ = delegate;
}

const AXNodeData& AXPlatformNodeBase::GetData() const {
  CHECK(delegate_);
  return delegate_->GetData();
}

gfx::Rect AXPlatformNodeBase::GetBoundsInScreen() const {
  CHECK(delegate_);
  gfx::Rect bounds = GetData().location;
  bounds.Offset(delegate_->GetGlobalCoordinateOffset());
  return bounds;
}

gfx::NativeViewAccessible AXPlatformNodeBase::GetParent() {
  CHECK(delegate_);
  return delegate_->GetParent();
}

int AXPlatformNodeBase::GetChildCount() {
  CHECK(delegate_);
  return delegate_->GetChildCount();
}

gfx::NativeViewAccessible AXPlatformNodeBase::ChildAtIndex(int index) {
  CHECK(delegate_);
  return delegate_->ChildAtIndex(index);
}

// AXPlatformNode overrides.

void AXPlatformNodeBase::Destroy() {
  delegate_ = nullptr;
  delete this;
}

gfx::NativeViewAccessible AXPlatformNodeBase::GetNativeViewAccessible() {
  return nullptr;
}

AXPlatformNodeDelegate* AXPlatformNodeBase::GetDelegate() const {
  return delegate_;
}

// Helpers.

AXPlatformNodeBase* AXPlatformNodeBase::GetPreviousSibling() {
  CHECK(delegate_);
  gfx::NativeViewAccessible parent_accessible = GetParent();
  AXPlatformNodeBase* parent = FromNativeViewAccessible(parent_accessible);
  if (!parent)
    return nullptr;

  int previous_index = GetIndexInParent() - 1;
  if (previous_index >= 0 &&
      previous_index < parent->GetChildCount()) {
    return FromNativeViewAccessible(parent->ChildAtIndex(previous_index));
  }
  return nullptr;
}

AXPlatformNodeBase* AXPlatformNodeBase::GetNextSibling() {
  CHECK(delegate_);
  gfx::NativeViewAccessible parent_accessible = GetParent();
  AXPlatformNodeBase* parent = FromNativeViewAccessible(parent_accessible);
  if (!parent)
    return nullptr;

  int next_index = GetIndexInParent() + 1;
  if (next_index >= 0 && next_index < parent->GetChildCount())
    return FromNativeViewAccessible(parent->ChildAtIndex(next_index));
  return nullptr;
}

bool AXPlatformNodeBase::IsDescendant(AXPlatformNodeBase* node) {
  CHECK(delegate_);
  if (!node)
    return false;
  if (node == this)
    return true;
  AXPlatformNodeBase* parent = FromNativeViewAccessible(node->GetParent());
  return IsDescendant(parent);
}

bool AXPlatformNodeBase::HasBoolAttribute(
    ui::AXBoolAttribute attribute) const {
  CHECK(delegate_);
  const ui::AXNodeData& data = GetData();
  auto iter = FindInVectorOfPairs(attribute, data.bool_attributes);
  return iter != data.bool_attributes.end();
}

bool AXPlatformNodeBase::GetBoolAttribute(
    ui::AXBoolAttribute attribute) const {
  CHECK(delegate_);
  bool result;
  if (GetBoolAttribute(attribute, &result))
    return result;
  return false;
}

bool AXPlatformNodeBase::GetBoolAttribute(
    ui::AXBoolAttribute attribute, bool* value) const {
  CHECK(delegate_);
  const ui::AXNodeData& data = GetData();
  auto iter = FindInVectorOfPairs(attribute, data.bool_attributes);
  if (iter != data.bool_attributes.end()) {
    *value = iter->second;
    return true;
  }

  return false;
}

bool AXPlatformNodeBase::HasFloatAttribute(
    ui::AXFloatAttribute attribute) const {
  CHECK(delegate_);
  const ui::AXNodeData& data = GetData();
  auto iter = FindInVectorOfPairs(attribute, data.float_attributes);
  return iter != data.float_attributes.end();
}

float AXPlatformNodeBase::GetFloatAttribute(
    ui::AXFloatAttribute attribute) const {
  CHECK(delegate_);
  float result;
  if (GetFloatAttribute(attribute, &result))
    return result;
  return 0.0;
}

bool AXPlatformNodeBase::GetFloatAttribute(
    ui::AXFloatAttribute attribute, float* value) const {
  CHECK(delegate_);
  const ui::AXNodeData& data = GetData();
  auto iter = FindInVectorOfPairs(attribute, data.float_attributes);
  if (iter != data.float_attributes.end()) {
    *value = iter->second;
    return true;
  }

  return false;
}

bool AXPlatformNodeBase::HasIntAttribute(
    ui::AXIntAttribute attribute) const {
  CHECK(delegate_);
  const ui::AXNodeData& data = GetData();
  auto iter = FindInVectorOfPairs(attribute, data.int_attributes);
  return iter != data.int_attributes.end();
}

int AXPlatformNodeBase::GetIntAttribute(
    ui::AXIntAttribute attribute) const {
  CHECK(delegate_);
  int result;
  if (GetIntAttribute(attribute, &result))
    return result;
  return 0;
}

bool AXPlatformNodeBase::GetIntAttribute(
    ui::AXIntAttribute attribute, int* value) const {
  CHECK(delegate_);
  const ui::AXNodeData& data = GetData();
  auto iter = FindInVectorOfPairs(attribute, data.int_attributes);
  if (iter != data.int_attributes.end()) {
    *value = iter->second;
    return true;
  }

  return false;
}

bool AXPlatformNodeBase::HasStringAttribute(
    ui::AXStringAttribute attribute) const {
  CHECK(delegate_);
  const ui::AXNodeData& data = GetData();
  auto iter = FindInVectorOfPairs(attribute, data.string_attributes);
  return iter != data.string_attributes.end();
}

const std::string& AXPlatformNodeBase::GetStringAttribute(
    ui::AXStringAttribute attribute) const {
  CHECK(delegate_);
  const ui::AXNodeData& data = GetData();
  CR_DEFINE_STATIC_LOCAL(std::string, empty_string, ());
  auto iter = FindInVectorOfPairs(attribute, data.string_attributes);
  return iter != data.string_attributes.end() ? iter->second : empty_string;
}

bool AXPlatformNodeBase::GetStringAttribute(
    ui::AXStringAttribute attribute, std::string* value) const {
  CHECK(delegate_);
  const ui::AXNodeData& data = GetData();
  auto iter = FindInVectorOfPairs(attribute, data.string_attributes);
  if (iter != data.string_attributes.end()) {
    *value = iter->second;
    return true;
  }

  return false;
}

base::string16 AXPlatformNodeBase::GetString16Attribute(
    ui::AXStringAttribute attribute) const {
  CHECK(delegate_);
  std::string value_utf8;
  if (!GetStringAttribute(attribute, &value_utf8))
    return base::string16();
  return base::UTF8ToUTF16(value_utf8);
}

bool AXPlatformNodeBase::GetString16Attribute(
    ui::AXStringAttribute attribute,
    base::string16* value) const {
  CHECK(delegate_);
  std::string value_utf8;
  if (!GetStringAttribute(attribute, &value_utf8))
    return false;
  *value = base::UTF8ToUTF16(value_utf8);
  return true;
}

AXPlatformNodeBase::AXPlatformNodeBase() {
}

AXPlatformNodeBase::~AXPlatformNodeBase() {
}

// static
AXPlatformNodeBase* AXPlatformNodeBase::FromNativeViewAccessible(
    gfx::NativeViewAccessible accessible) {
  return static_cast<AXPlatformNodeBase*>(
      AXPlatformNode::FromNativeViewAccessible(accessible));
}

}  // namespace ui
