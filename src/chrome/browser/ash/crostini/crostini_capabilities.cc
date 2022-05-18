// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_capabilities.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "third_party/cros_system_api/constants/vm_tools.h"

namespace crostini {

void CrostiniCapabilities::Build(
    Profile* profile,
    base::OnceCallback<void(std::unique_ptr<guest_os::GuestOsCapabilities>)>
        callback) {
  std::string reason;
  if (!CrostiniFeatures::Get()->IsAllowedNow(profile, &reason)) {
    LOG(WARNING) << "Crostini is not allowed: " << reason;
    std::move(callback).Run(nullptr);
    return;
  }
  // WrapUnique is used because the constructor is private.
  std::move(callback).Run(base::WrapUnique(new CrostiniCapabilities()));
}

CrostiniCapabilities::~CrostiniCapabilities() = default;

std::string CrostiniCapabilities::GetSecurityContext() const {
  return vm_tools::kConciergeSecurityContext;
}

}  // namespace crostini
