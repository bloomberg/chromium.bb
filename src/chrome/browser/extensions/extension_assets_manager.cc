// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_assets_manager.h"

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/extensions/extension_assets_manager_chromeos.h"
#endif

namespace extensions {
namespace {

class ExtensionAssetsManagerImpl :  public ExtensionAssetsManager {
 public:
  static ExtensionAssetsManagerImpl* GetInstance() {
    return base::Singleton<ExtensionAssetsManagerImpl>::get();
  }

  // Override from ExtensionAssetsManager.
  void InstallExtension(const Extension* extension,
                        const base::FilePath& unpacked_extension_root,
                        const base::FilePath& local_install_dir,
                        Profile* profile,
                        InstallExtensionCallback callback) override {
    callback.Run(file_util::InstallExtension(
        unpacked_extension_root,
        extension->id(),
        extension->VersionString(),
        local_install_dir));
  }

  void UninstallExtension(const std::string& id,
                          Profile* profile,
                          const base::FilePath& local_install_dir,
                          const base::FilePath& extension_root) override {
    file_util::UninstallExtension(local_install_dir, id);
  }

 private:
  friend struct base::DefaultSingletonTraits<ExtensionAssetsManagerImpl>;

  ExtensionAssetsManagerImpl() {}
  ~ExtensionAssetsManagerImpl() override {}

  DISALLOW_COPY_AND_ASSIGN(ExtensionAssetsManagerImpl);
};

}  // namespace

// static
ExtensionAssetsManager* ExtensionAssetsManager::GetInstance() {
#if defined(OS_CHROMEOS)
  return ExtensionAssetsManagerChromeOS::GetInstance();
#else
  // If not Chrome OS, use trivial implementation that doesn't share anything.
  return ExtensionAssetsManagerImpl::GetInstance();
#endif  // OS_CHROMEOS
}

}  // namespace extensions
