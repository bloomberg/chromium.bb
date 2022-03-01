// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_HANDLER_TEST_HELPER_H_
#define CHROMEOS_NETWORK_NETWORK_HANDLER_TEST_HELPER_H_

#include "chromeos/network/network_test_helper_base.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {

// Helper class for tests that uses network handler classes. This class
// handles initialization and shutdown of Shill and Hermes DBus clients and
// NetworkHandler instance.
//
// NOTE: This class is intended for use in tests that use NetworkHandler::Get()
// and thus require NetworkHandler and related DBus clients to be initialized.
// For tests that only need NetworkStateHandler and or NetworkDeviceHandler to
// be initialized use NetworkStateTestHelper.
class NetworkHandlerTestHelper : public NetworkTestHelperBase {
 public:
  explicit NetworkHandlerTestHelper();
  ~NetworkHandlerTestHelper();

  // Registers any prefs required by NetworkHandler.
  void RegisterPrefs(PrefRegistrySimple* user_registry,
                     PrefRegistrySimple* device_registry);

  // Calls NetworkHandler::InitializePrefServices.
  void InitializePrefs(PrefService* user_prefs, PrefService* device_prefs);

 private:
  bool network_handler_initialized_ = false;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::NetworkHandlerTestHelper;
}

#endif  // CHROMEOS_NETWORK_NETWORK_HANDLER_TEST_HELPER_H_
