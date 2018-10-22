// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/public/cpp/features.h"

namespace chromeos {
namespace ime {

// TODO(https://crbug.com/837156): Add this feature to chrome://flags.
// If enabled, allows the qualified first-party IME extensions to connect to
// the ChromeOS IME service.
const base::Feature kImeServiceConnectable{"ImeServiceConnectable",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace ime
}  // namespace chromeos
