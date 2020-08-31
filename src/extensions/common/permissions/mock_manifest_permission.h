// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_MOCK_MANIFEST_PERMISSION_H_
#define EXTENSIONS_COMMON_PERMISSIONS_MOCK_MANIFEST_PERMISSION_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "extensions/common/permissions/manifest_permission.h"

namespace base {
class Value;
}

namespace extensions {

// Useful for mocking ManifestPermission in tests.
class MockManifestPermission : public ManifestPermission {
 public:
  explicit MockManifestPermission(const std::string& name);

  std::string name() const override;
  std::string id() const override;

  PermissionIDSet GetPermissions() const override;

  bool FromValue(const base::Value* value) override;
  std::unique_ptr<base::Value> ToValue() const override;

  std::unique_ptr<ManifestPermission> Diff(
      const ManifestPermission* rhs) const override;
  std::unique_ptr<ManifestPermission> Union(
      const ManifestPermission* rhs) const override;
  std::unique_ptr<ManifestPermission> Intersect(
      const ManifestPermission* rhs) const override;
  bool RequiresManagementUIWarning() const override;

 private:
  std::string name_;

  DISALLOW_COPY_AND_ASSIGN(MockManifestPermission);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_MOCK_MANIFEST_PERMISSION_H_
