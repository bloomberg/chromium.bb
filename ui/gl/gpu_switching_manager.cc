// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gpu_switching_manager.h"

#include "base/logging.h"

namespace gfx {

// static
GpuSwitchingManager* GpuSwitchingManager::GetInstance() {
  return Singleton<GpuSwitchingManager>::get();
}

GpuSwitchingManager::GpuSwitchingManager() {
}

GpuSwitchingManager::~GpuSwitchingManager() {
}

void GpuSwitchingManager::ForceUseOfIntegratedGpu() {
  gpu_switching_option_.clear();
  gpu_switching_option_.push_back(PreferIntegratedGpu);
}

void GpuSwitchingManager::ForceUseOfDiscreteGpu() {
  gpu_switching_option_.clear();
  gpu_switching_option_.push_back(PreferDiscreteGpu);
}

GpuPreference GpuSwitchingManager::AdjustGpuPreference(
    GpuPreference gpu_preference) {
  if (gpu_switching_option_.size() == 0)
    return gpu_preference;
  DCHECK_EQ(gpu_switching_option_.size(), 1u);
#if defined(OS_MACOSX)
  // Create a pixel format that lasts the lifespan of Chrome, so Chrome
  // stays on the discrete GPU.
  if (gpu_switching_option_[0] == PreferDiscreteGpu)
    GLContextCGL::ForceUseOfDiscreteGPU();
#endif  // OS_MACOSX
  return gpu_switching_option_[0];
}

}  // namespace gfx

