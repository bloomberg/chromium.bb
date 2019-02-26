// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MAC_APP_SHIM_PARAM_TRAITS_H_
#define CHROME_COMMON_MAC_APP_SHIM_PARAM_TRAITS_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/common/mac/app_shim_launch.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/param_traits_macros.h"

IPC_ENUM_TRAITS_MAX_VALUE(apps::AppShimLaunchType,
                          apps::APP_SHIM_LAUNCH_NUM_TYPES - 1)
IPC_ENUM_TRAITS_MAX_VALUE(apps::AppShimLaunchResult,
                          apps::APP_SHIM_LAUNCH_NUM_RESULTS - 1)
IPC_ENUM_TRAITS_MAX_VALUE(apps::AppShimFocusType,
                          apps::APP_SHIM_FOCUS_NUM_TYPES - 1)
IPC_ENUM_TRAITS_MAX_VALUE(apps::AppShimAttentionType,
                          apps::APP_SHIM_ATTENTION_NUM_TYPES - 1)

#endif  // CHROME_COMMON_MAC_APP_SHIM_PARAM_TRAITS_H_
