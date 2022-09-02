// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_KEY_SYSTEM_SUPPORT_WIN_H_
#define CONTENT_BROWSER_MEDIA_KEY_SYSTEM_SUPPORT_WIN_H_

#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "media/cdm/cdm_capability.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

using CdmCapabilityCB =
    base::OnceCallback<void(absl::optional<media::CdmCapability>)>;

// Returns the hardware secure CdmCapability supported in MediaFoundationService
// for `key_system` by the CDM located in `cdm_path`.
// TODO(xhwang): Also support software secure CdmCapability supported in
// MediaFoundationService.
void GetMediaFoundationServiceHardwareSecureCdmCapability(
    const std::string& key_system,
    const base::FilePath& cdm_path,
    CdmCapabilityCB cdm_capability_cb);

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_KEY_SYSTEM_SUPPORT_WIN_H_
