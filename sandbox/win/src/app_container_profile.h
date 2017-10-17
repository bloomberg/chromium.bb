// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_APP_CONTAINER_PROFILE_H_
#define SANDBOX_SRC_APP_CONTAINER_PROFILE_H_

#include <windows.h>

#include <accctrl.h>

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/win/scoped_handle.h"
#include "sandbox/win/src/security_capabilities.h"
#include "sandbox/win/src/sid.h"

namespace sandbox {

class AppContainerProfile {
 public:
  // Increments the reference count of this object. The reference count must
  // be incremented if this interface is given to another component.
  void AddRef();

  // Decrements the reference count of this object. When the reference count
  // is zero the object is automatically destroyed.
  // Indicates that the caller is done with this interface. After calling
  // release no other method should be called.
  void Release();

  // Get a handle to a registry key for this package.
  bool GetRegistryLocation(REGSAM desired_access, base::win::ScopedHandle* key);

  // Get a folder path to a location for this package.
  bool GetFolderPath(base::FilePath* file_path);

  // Get a pipe name usable by this AC.
  bool GetPipePath(const wchar_t* pipe_name, base::FilePath* pipe_path);

  // Get the package SID for this AC.
  Sid GetPackageSid() const;

  // Do an access check based on this profile for a named object. If method
  // returns true then access_status reflects whether access was granted and
  // granted_access gives the final access rights. The object_type can be one of
  // SE_FILE_OBJECT, SE_REGISTRY_KEY, SE_REGISTRY_WOW64_32KEY. See
  // ::GetNamedSecurityInfo for more information about how the enumeration is
  // used and what format object_name needs to be.
  bool AccessCheck(const wchar_t* object_name,
                   SE_OBJECT_TYPE object_type,
                   DWORD desired_access,
                   DWORD* granted_access,
                   BOOL* access_status);

  // Adds a capability by name to this profile.
  bool AddCapability(const wchar_t* capability_name);
  // Adds a capability from a known list.
  bool AddCapability(WellKnownCapabilities capability);
  // Adds a capability from a SID
  bool AddCapability(const Sid& capability_sid);

  // Enable Low Privilege AC.
  void SetEnableLowPrivilegeAppContainer(bool enable);
  bool GetEnableLowPrivilegeAppContainer();

  // Get a allocated SecurityCapabilities object for this App Container.
  std::unique_ptr<SecurityCapabilities> GetSecurityCapabilities();

  // Creates a new AppContainerProfile object. This will create a new profile
  // if it doesn't already exist. The profile must be deleted manually using
  // the Delete method if it's no longer required.
  static scoped_refptr<AppContainerProfile> Create(const wchar_t* package_name,
                                                   const wchar_t* display_name,
                                                   const wchar_t* description);

  // Opens an AppContainerProfile object. No checks will be made on
  // whether the package exists or not.
  static scoped_refptr<AppContainerProfile> Open(const wchar_t* package_name);

  // Delete a profile based on name. Returns true if successful, or if the
  // package doesn't already exist.
  static bool Delete(const wchar_t* package_name);

 private:
  AppContainerProfile(const Sid& package_sid);
  ~AppContainerProfile();

  bool BuildLowBoxToken(base::win::ScopedHandle* token);

  // Standard object-lifetime reference counter.
  volatile LONG ref_count_;
  Sid package_sid_;
  bool enable_low_privilege_app_container_;
  std::vector<Sid> capabilities_;

  DISALLOW_COPY_AND_ASSIGN(AppContainerProfile);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_APP_CONTAINER_PROFILE_H_
