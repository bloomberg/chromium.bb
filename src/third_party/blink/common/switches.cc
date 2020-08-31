// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/switches.h"

namespace blink {
namespace switches {

// Used to communicate managed policy for the UserAgentClientHint feature.
// This feature is typically controlled by base::Feature (see
// renderer/platform/scheduler/common/features.*) but requires an enterprise
// policy override.

extern const char kUserAgentClientHintDisable[] =
    "user-agent-client-hint-disable";
}  // namespace switches
}  // namespace blink
