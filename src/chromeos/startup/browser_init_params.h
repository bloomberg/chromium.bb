// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_STARTUP_BROWSER_INIT_PARAMS_H_
#define CHROMEOS_STARTUP_BROWSER_INIT_PARAMS_H_

#include "base/no_destructor.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"

namespace chromeos {

// Stores and handles BrowserInitParams.
class COMPONENT_EXPORT(CHROMEOS_STARTUP) BrowserInitParams {
 public:
  // Returns BrowserInitParams which is passed from ash-chrome. On launching
  // lacros-chrome from ash-chrome, ash-chrome creates a memory backed file
  // serializes the BrowserInitParams to it, and the forked/executed
  // lacros-chrome process inherits the file descriptor. The data is read
  // in the constructor so is available from the beginning.
  static const crosapi::mojom::BrowserInitParams* Get();

  BrowserInitParams(const BrowserInitParams&) = delete;
  BrowserInitParams& operator=(const BrowserInitParams&) = delete;

  // Sets `init_params_` to the provided value.
  // Useful for tests that cannot setup a full Lacros test environment with a
  // working Mojo connection to Ash.
  static void SetInitParamsForTests(
      crosapi::mojom::BrowserInitParamsPtr init_params);

  // Create Mem FD from `init_params_`. This must be called after `init_params_`
  // has initialized by calling GetInstance().
  static base::ScopedFD CreateStartupData();

  static bool disable_crosapi_for_testing() {
    return disable_crosapi_for_testing_;
  }

 private:
  friend base::NoDestructor<BrowserInitParams>;

  static BrowserInitParams* GetInstance();

  BrowserInitParams();
  ~BrowserInitParams();

  // Needs to access |disable_crosapi_for_testing_|.
  friend class ScopedDisableCrosapiForTesting;

  // Tests will set this to |true| which will make all crosapi functionality
  // unavailable. Should be set from ScopedDisableCrosapiForTesting always.
  // TODO(https://crbug.com/1131722): Ideally we could stub this out or make
  // this functional for tests without modifying production code
  static bool disable_crosapi_for_testing_;

  // Parameters passed from ash-chrome.
  crosapi::mojom::BrowserInitParamsPtr init_params_;
};

}  // namespace chromeos

#endif  // CHROMEOS_STARTUP_BROWSER_INIT_PARAMS_H_
