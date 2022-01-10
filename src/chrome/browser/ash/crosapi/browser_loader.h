// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_LOADER_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_LOADER_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/component_updater/cros_component_manager.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "components/component_updater/component_updater_service.h"

namespace crosapi {

using browser_util::LacrosSelection;

// Manages download of the lacros-chrome binary.
// This class is a part of ash-chrome.
class BrowserLoader {
 public:
  // Contructor for production.
  explicit BrowserLoader(
      scoped_refptr<component_updater::CrOSComponentManager> manager);
  // Constructor for testing.
  BrowserLoader(scoped_refptr<component_updater::CrOSComponentManager> manager,
                component_updater::ComponentUpdateService* updater,
                chromeos::UpstartClient* upstart_client);

  BrowserLoader(const BrowserLoader&) = delete;
  BrowserLoader& operator=(const BrowserLoader&) = delete;

  virtual ~BrowserLoader();

  // Starts to load lacros-chrome binary or the rootfs lacros-chrome binary.
  // |callback| is called on completion with the path to the lacros-chrome on
  // success, or an empty filepath on failure, and the loaded lacros selection
  // which is either 'rootfs' or 'stateful'.
  using LoadCompletionCallback =
      base::OnceCallback<void(const base::FilePath&, LacrosSelection)>;
  virtual void Load(LoadCompletionCallback callback);

  // Starts to unload lacros-chrome binary.
  // Note that this triggers to remove the user directory for lacros-chrome.
  virtual void Unload();

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserLoaderTest,
                           OnLoadSelectionQuicklyChooseRootfs);
  FRIEND_TEST_ALL_PREFIXES(BrowserLoaderTest, OnLoadVersionSelectionStateful);
  FRIEND_TEST_ALL_PREFIXES(BrowserLoaderTest, OnLoadVersionSelectionRootfs);
  FRIEND_TEST_ALL_PREFIXES(BrowserLoaderTest,
                           OnLoadVersionSelectionRootfsIsOlder);

  // Loads/Installs the stateful lacros component.
  void LoadStatefulLacros(LoadCompletionCallback callback);

  // Loads the rootfs lacros component.
  void LoadRootfsLacros(LoadCompletionCallback callback);
  void OnLoadRootfsLacros(LoadCompletionCallback callback,
                          bool already_mounted);

  // Called to quickly load rootfs lacros if stateful lacros was missing and
  // start the stateful lacros installation.
  void OnLoadSelection(LoadCompletionCallback callback, bool was_installed);

  // Called to make sure stateful lacros is mounted before version comparison.
  void OnLoadSelectionMountStateful(
      LoadCompletionCallback callback,
      component_updater::CrOSComponentManager::Error error,
      const base::FilePath& path);

  // Called to determine which lacros to load based on version (rootfs vs
  // stateful).
  void LoadVersionSelection(LoadCompletionCallback callback);
  void OnLoadVersionSelection(LoadCompletionCallback callback,
                              base::Version rootfs_lacros_version);

  // Called on the completion of loading.
  void OnLoadComplete(LoadCompletionCallback callback,
                      component_updater::CrOSComponentManager::Error error,
                      const base::FilePath& path);

  // Unloading hops threads. This is called after we check whether Lacros was
  // installed and maybe clean up the user directory.
  void OnCheckInstalled(bool was_installed);

  // Unloads the component. Called after system salt is available.
  void UnloadAfterCleanUp(const std::string& ignored_salt);

  // Callback from upstart mounting lacros-chrome.
  void OnUpstartLacrosMounter(LoadCompletionCallback callback, bool success);

  scoped_refptr<component_updater::CrOSComponentManager> component_manager_;

  // May be null in tests.
  component_updater::ComponentUpdateService* const component_update_service_;

  // Pointer held to `UpstartClient` for testing purposes.
  // Otherwise, the lifetime is the same as `chromeos::UpstartClient::Get()`.
  chromeos::UpstartClient* const upstart_client_;

  base::WeakPtrFactory<BrowserLoader> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_LOADER_H_
