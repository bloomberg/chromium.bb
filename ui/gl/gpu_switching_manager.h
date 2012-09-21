// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GPU_SWITCHING_MANAGER_H_
#define UI_GL_GPU_SWITCHING_MANAGER_H_

#include <vector>

#include "base/memory/singleton.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gpu_preference.h"

namespace gfx {

class GL_EXPORT GpuSwitchingManager {
 public:
  // Getter for the singleton. This will return NULL on failure.
  static GpuSwitchingManager* GetInstance();

  // Set the switching option, but don't take any actions until later
  // we access GPU.
  void ForceUseOfIntegratedGpu();
  void ForceUseOfDiscreteGpu();

  // Adjust GpuPreference based on the current switching option.
  // If none is set, return the original GpuPreference.
  GpuPreference AdjustGpuPreference(GpuPreference gpu_preference);

 private:
  friend struct DefaultSingletonTraits<GpuSwitchingManager>;

  GpuSwitchingManager();
  virtual ~GpuSwitchingManager();

  std::vector<GpuPreference> gpu_switching_option_;

  DISALLOW_COPY_AND_ASSIGN(GpuSwitchingManager);
};

}  // namespace gfx

#endif  // UI_GL_GPU_SWITCHING_MANAGER_H_

