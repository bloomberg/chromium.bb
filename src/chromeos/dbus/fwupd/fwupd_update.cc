// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fwupd/fwupd_update.h"

#include "base/files/file_path.h"

namespace chromeos {

FwupdUpdate::FwupdUpdate() = default;

FwupdUpdate::FwupdUpdate(const std::string& version,
                         const std::string& description,
                         int priority,
                         const base::FilePath& filepath)
    : version(version),
      description(description),
      priority(priority),
      filepath(filepath) {}

FwupdUpdate::FwupdUpdate(FwupdUpdate&& other) = default;
FwupdUpdate& FwupdUpdate::operator=(FwupdUpdate&& other) = default;
FwupdUpdate::~FwupdUpdate() = default;

}  // namespace chromeos
