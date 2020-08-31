// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SECURITY_TOKEN_PIN_ERROR_GENERATOR_H_
#define CHROMEOS_COMPONENTS_SECURITY_TOKEN_PIN_ERROR_GENERATOR_H_

#include "base/component_export.h"
#include "base/strings/string16.h"
#include "chromeos/components/security_token_pin/constants.h"

namespace chromeos {
namespace security_token_pin {

// Generate an error message for a security pin token dialog, based on dialog
// parameters |error_label|, |attempts_left|, and |accept_input|.
COMPONENT_EXPORT(SECURITY_TOKEN_PIN)
base::string16 GenerateErrorMessage(ErrorLabel error_label,
                                    int attempts_left,
                                    bool accept_input);

}  // namespace security_token_pin
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SECURITY_TOKEN_PIN_ERROR_GENERATOR_H_
