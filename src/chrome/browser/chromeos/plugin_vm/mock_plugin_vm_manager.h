// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLUGIN_VM_MOCK_PLUGIN_VM_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_PLUGIN_VM_MOCK_PLUGIN_VM_MANAGER_H_

#include "base/callback.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace plugin_vm {
namespace test {

class MockPluginVmManager : public PluginVmManager {
 public:
  MockPluginVmManager();
  ~MockPluginVmManager();
  MockPluginVmManager(const MockPluginVmManager&) = delete;
  MockPluginVmManager& operator=(const MockPluginVmManager&) = delete;

  MOCK_METHOD(void, LaunchPluginVm, (LaunchPluginVmCallback callback), ());
  MOCK_METHOD(void, StopPluginVm, (const std::string& name, bool force), ());
  MOCK_METHOD(void, UninstallPluginVm, (), ());
  MOCK_METHOD(uint64_t, seneschal_server_handle, (), (const));
  MOCK_METHOD(
      void,
      UpdateVmState,
      (base::OnceCallback<void(bool default_vm_exists)> success_callback,
       base::OnceClosure error_callback),
      ());
  MOCK_METHOD(void,
              AddVmStartingObserver,
              (chromeos::VmStartingObserver * observer),
              ());
  MOCK_METHOD(void,
              RemoveVmStartingObserver,
              (chromeos::VmStartingObserver * observer),
              ());
  MOCK_METHOD(vm_tools::plugin_dispatcher::VmState, vm_state, (), (const));
};

}  // namespace test
}  // namespace plugin_vm

#endif  // CHROME_BROWSER_CHROMEOS_PLUGIN_VM_MOCK_PLUGIN_VM_MANAGER_H_
