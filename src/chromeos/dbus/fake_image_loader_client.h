// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_IMAGE_LOADER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_IMAGE_LOADER_CLIENT_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "chromeos/dbus/image_loader_client.h"

namespace base {
class FilePath;
}

namespace chromeos {

// A fake implementation of ImageLoaderClient. This class does nothing.
class CHROMEOS_EXPORT FakeImageLoaderClient : public ImageLoaderClient {
 public:
  FakeImageLoaderClient();
  ~FakeImageLoaderClient() override;

  // Sets intended mount path for a component.
  void SetMountPathForComponent(const std::string& component_name,
                                const base::FilePath& mount_path);

  // DBusClient override.
  void Init(dbus::Bus* dbus) override {}

  // ImageLoaderClient override:
  void RegisterComponent(const std::string& name,
                         const std::string& version,
                         const std::string& component_folder_abs_path,
                         DBusMethodCallback<bool> callback) override;
  void LoadComponent(const std::string& name,
                     DBusMethodCallback<std::string> callback) override;
  void LoadComponentAtPath(
      const std::string& name,
      const base::FilePath& path,
      DBusMethodCallback<base::FilePath> callback) override;
  void RemoveComponent(const std::string& name,
                       DBusMethodCallback<bool> callback) override;
  void RequestComponentVersion(
      const std::string& name,
      DBusMethodCallback<std::string> callback) override;
  void UnmountComponent(const std::string& name,
                        DBusMethodCallback<bool> callback) override;

 private:
  // Maps registered component name to its registered varsion.
  std::map<std::string, std::string> registered_components_;

  // Maps component names to paths to which they should be mounted.
  // Registered using SetMountPathForComponent() before LoadComponent*() is
  // called, and removed by a later call to UnmountComponent().
  std::map<std::string, base::FilePath> mount_paths_;

  DISALLOW_COPY_AND_ASSIGN(FakeImageLoaderClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_IMAGE_LOADER_CLIENT_H_
