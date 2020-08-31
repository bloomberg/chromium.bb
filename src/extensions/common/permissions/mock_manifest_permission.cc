// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/mock_manifest_permission.h"

#include "extensions/common/permissions/api_permission_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

MockManifestPermission::MockManifestPermission(const std::string& name)
    : name_(name) {}

std::string MockManifestPermission::name() const {
  return name_;
}

std::string MockManifestPermission::id() const {
  return name();
}

PermissionIDSet MockManifestPermission::GetPermissions() const {
  return PermissionIDSet();
}

bool MockManifestPermission::FromValue(const base::Value* value) {
  return true;
}

std::unique_ptr<base::Value> MockManifestPermission::ToValue() const {
  return std::make_unique<base::Value>();
}

std::unique_ptr<ManifestPermission> MockManifestPermission::Diff(
    const ManifestPermission* rhs) const {
  const MockManifestPermission* other =
      static_cast<const MockManifestPermission*>(rhs);
  EXPECT_EQ(name_, other->name_);
  return nullptr;
}

std::unique_ptr<ManifestPermission> MockManifestPermission::Union(
    const ManifestPermission* rhs) const {
  const MockManifestPermission* other =
      static_cast<const MockManifestPermission*>(rhs);
  EXPECT_EQ(name_, other->name_);
  return std::make_unique<MockManifestPermission>(name_);
}

std::unique_ptr<ManifestPermission> MockManifestPermission::Intersect(
    const ManifestPermission* rhs) const {
  const MockManifestPermission* other =
      static_cast<const MockManifestPermission*>(rhs);
  EXPECT_EQ(name_, other->name_);
  return std::make_unique<MockManifestPermission>(name_);
}

bool MockManifestPermission::RequiresManagementUIWarning() const {
  return false;
}

}  // namespace extensions
