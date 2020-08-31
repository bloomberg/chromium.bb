// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_MANIFEST_PERMISSION_H_
#define EXTENSIONS_COMMON_PERMISSIONS_MANIFEST_PERMISSION_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/pickle.h"
#include "extensions/common/permissions/api_permission_set.h"

namespace base {
class PickleIterator;
class Value;
}

namespace extensions {

// Represent the custom behavior of a top-level manifest entry contributing to
// permission messages and storage.
class ManifestPermission {
 public:
  ManifestPermission();
  virtual ~ManifestPermission();

  // The manifest key this permission applies to.
  virtual std::string name() const = 0;

  // Same as name(), needed for compatibility with APIPermission.
  virtual std::string id() const = 0;

  // The set of permissions this manifest entry has. These permissions are used
  // by PermissionMessageProvider to generate meaningful permission messages
  // for the app.
  virtual PermissionIDSet GetPermissions() const = 0;

  // Parses the ManifestPermission from |value|. Returns false if error happens.
  virtual bool FromValue(const base::Value* value) = 0;

  // Stores this into a new created Value.
  virtual std::unique_ptr<base::Value> ToValue() const = 0;

  // Clones this.
  std::unique_ptr<ManifestPermission> Clone() const;

  // Returns a new manifest permission which equals this - |rhs|.
  virtual std::unique_ptr<ManifestPermission> Diff(
      const ManifestPermission* rhs) const = 0;

  // Returns a new manifest permission which equals the union of this and |rhs|.
  virtual std::unique_ptr<ManifestPermission> Union(
      const ManifestPermission* rhs) const = 0;

  // Returns a new manifest permission which equals the intersect of this and
  // |rhs|.
  virtual std::unique_ptr<ManifestPermission> Intersect(
      const ManifestPermission* rhs) const = 0;

  // Returns true if one of the permissions should trigger a permission message
  // in the management page.  If the permission should trigger a warning message
  // in chrome://management, set this function to return true.
  virtual bool RequiresManagementUIWarning() const = 0;

  // Returns true if any of the included permissions should trigger the full
  // warning on the login screen of the managed-guest session. Reach out to the
  // privacy team before setting this function to return false.
  virtual bool RequiresManagedSessionFullLoginWarning() const;

  // Returns true if |rhs| is a subset of this.
  bool Contains(const ManifestPermission* rhs) const;

  // Returns true if |rhs| is equal to this.
  bool Equal(const ManifestPermission* rhs) const;

  // IPC functions
  // Writes this into the given IPC message |m|.
  void Write(base::Pickle* m) const;

  // Reads from the given IPC message |m|.
  bool Read(const base::Pickle* m, base::PickleIterator* iter);

  // Logs this permission.
  void Log(std::string* log) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(ManifestPermission);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_MANIFEST_PERMISSION_H_
