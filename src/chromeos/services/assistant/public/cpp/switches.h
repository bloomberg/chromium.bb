// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_SWITCHES_H_
#define CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_SWITCHES_H_

#include "base/component_export.h"

namespace chromeos {
namespace assistant {
namespace switches {

// NOTE: Switches are reserved for developer-facing options. End-user facing
// features should use base::Feature. See features.h.

// Forces the assistant first-run onboarding view, even if the user has seen it
// before. Useful when working on UI layout.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const char kForceAssistantOnboarding[];

}  // namespace switches
}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_SWITCHES_H_
