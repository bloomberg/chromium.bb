// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONFLICTS_THIRD_PARTY_METRICS_RECORDER_WIN_H_
#define CHROME_BROWSER_CONFLICTS_THIRD_PARTY_METRICS_RECORDER_WIN_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/conflicts/module_database_observer_win.h"

struct ModuleInfoData;
struct ModuleInfoKey;

// Records metrics about third party modules loaded into Chrome.
class ThirdPartyMetricsRecorder : public ModuleDatabaseObserver {
 public:
  ThirdPartyMetricsRecorder();
  ~ThirdPartyMetricsRecorder() override;

  // ModuleDatabaseObserver:
  void OnNewModuleFound(const ModuleInfoKey& module_key,
                        const ModuleInfoData& module_data) override;
  void OnModuleDatabaseIdle() override;

 private:
  // The size of the unsigned modules crash keys.
  static constexpr size_t kCrashKeySize = 256;

  // A helper function that writes the unsigned modules name to a series of
  // crash keys. The crash keys are leaked so that they can be picked up by the
  // crash reporter. Creating another instance of ThirdPartyMetricsRecorder
  // will start overwriting the current values in the crash keys. This is not
  // a problem in practice because this class is leaked.
  void AddUnsignedModuleToCrashkeys(const base::string16& module_basename);

  // The index of the crash key that is currently being updated.
  size_t current_key_index_ = 0;

  // The value of the crash key that is currently being updated.
  std::string current_value_;

  // Flag used to avoid sending module counts multiple times.
  bool metrics_emitted_ = false;

  // Counters for different types of modules.
  size_t module_count_ = 0;
  size_t unsigned_module_count_ = 0;
  size_t signed_module_count_ = 0;
  size_t catalog_module_count_ = 0;
  size_t microsoft_module_count_ = 0;
  size_t loaded_third_party_module_count_ = 0;
  size_t not_loaded_third_party_module_count_ = 0;

  // Counts the number of shell extensions.
  size_t shell_extensions_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ThirdPartyMetricsRecorder);
};

#endif  // CHROME_BROWSER_CONFLICTS_THIRD_PARTY_METRICS_RECORDER_WIN_H_
