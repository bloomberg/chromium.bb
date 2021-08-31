// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CELLULAR_SETUP_ESIM_TEST_UTILS_H_
#define CHROMEOS_SERVICES_CELLULAR_SETUP_ESIM_TEST_UTILS_H_

#include "chromeos/services/cellular_setup/public/mojom/esim_manager.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace cellular_setup {

// Calls GetProperties on a remote euicc object and waits for
// result. Returns the resulting EuiccProperties structure.
mojom::EuiccPropertiesPtr GetEuiccProperties(
    const mojo::Remote<mojom::Euicc>& euicc);

// Calls GetProperties on a remote esim_profile object and waits
// for result. Returns the resulting EuiccProperties structure.
mojom::ESimProfilePropertiesPtr GetESimProfileProperties(
    const mojo::Remote<mojom::ESimProfile>& esim_profile);

// Calls GetProfileList on a remote euicc object and waits
// for result. Returns the resulting list of ESimProfile
// pending remotes.
std::vector<mojo::PendingRemote<mojom::ESimProfile>> GetProfileList(
    const mojo::Remote<mojom::Euicc>& euicc);

}  // namespace cellular_setup
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_CELLULAR_SETUP_ESIM_TEST_UTILS_H_