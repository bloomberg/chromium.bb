// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/quarantine/public/cpp/quarantine_features_win.h"

namespace quarantine {

// This feature controls whether the quarantine service should run in
// the browser process or a new utility process.
// Unused until quarantine service is fully implemented.
const base::Feature kOutOfProcessQuarantine{"OutOfProcessQuarantine",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// This feature controls whether the InvokeAttachmentServices function will be
// called. Has no effect on machines that are domain-joined, where the function
// is always called.
const base::Feature kInvokeAttachmentServices{"InvokeAttachmentServices",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace quarantine
