// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/quarantine/quarantine_features_win.h"

namespace download {

// This feature controls whether the InvokeAttachmentServices function will be
// called. Has no effect on machines that are domain-joined, where the function
// is always called.
const base::Feature kInvokeAttachmentServices{"InvokeAttachmentServices",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace download
