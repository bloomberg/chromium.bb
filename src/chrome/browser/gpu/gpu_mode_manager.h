// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GPU_GPU_MODE_MANAGER_H_
#define CHROME_BROWSER_GPU_GPU_MODE_MANAGER_H_

#include "base/macros.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;

class GpuModeManager {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

  GpuModeManager();
  ~GpuModeManager();

  bool initial_gpu_mode_pref() const;

 private:
  static bool IsGpuModePrefEnabled();

  PrefChangeRegistrar pref_registrar_;

  bool initial_gpu_mode_pref_;

  DISALLOW_COPY_AND_ASSIGN(GpuModeManager);
};

#endif  // CHROME_BROWSER_GPU_GPU_MODE_MANAGER_H_

