// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_DESK_TEMPLATE_UTIL_H_
#define COMPONENTS_DESKS_STORAGE_CORE_DESK_TEMPLATE_UTIL_H_

#include <string>

#include "ash/public/cpp/desk_template.h"

namespace desks_storage {

namespace desk_template_util {

ash::DeskTemplate* FindOtherEntryWithName(
    const std::u16string& name,
    const base::GUID& uuid,
    const std::map<base::GUID, std::unique_ptr<ash::DeskTemplate>>& entries);

}  // namespace desk_template_util

}  // namespace desks_storage

#endif  // COMPONENTS_DESKS_STORAGE_CORE_DESK_TEMPLATE_UTIL_H_
