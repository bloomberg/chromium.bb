// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_METRICS_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_METRICS_UTIL_H_

#include "base/metrics/histogram_functions.h"

namespace plugin_vm {

extern const char kPluginVmImageDownloadedSizeHistogram[];
extern const char kPluginVmLaunchResultHistogram[];
extern const char kPluginVmSetupResultHistogram[];

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PluginVmLaunchResult {
  kSuccess = 0,
  kError = 1,
  kMaxValue = kError,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PluginVmSetupResult {
  kSuccess = 0,

  kPluginVmIsNotAllowed = 1,

  kErrorDownloadingPluginVmImage = 2,
  kErrorImportingPluginVmImage = 3,

  kUserCancelledDownloadingPluginVmImage = 4,
  kUserCancelledImportingPluginVmImage = 5,

  kMaxValue = kUserCancelledImportingPluginVmImage,
};

void RecordPluginVmImageDownloadedSizeHistogram(uint64_t bytes_downloaded);
void RecordPluginVmLaunchResultHistogram(PluginVmLaunchResult launch_result);
void RecordPluginVmSetupResultHistogram(PluginVmSetupResult setup_result);

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_METRICS_UTIL_H_
