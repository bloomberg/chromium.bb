// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_STARTUP_STARTUP_H_
#define CHROMEOS_STARTUP_STARTUP_H_

#include <string>

#include "base/component_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

// Reads the startup data. The FD to be read for the startup data should be
// specified via the kCrosStartupDataFD command line flag. This function
// consumes the FD, so this must not be called twice in a process.
COMPONENT_EXPORT(CHROMEOS_STARTUP)
absl::optional<std::string> ReadStartupData();

}  // namespace chromeos

#endif  // CHROMEOS_STARTUP_STARTUP_H_
