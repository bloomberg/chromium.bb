// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gpu_switching_manager.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_MACOSX)
#include <OpenGL/OpenGL.h>
#include "base/mac/mac_util.h"
#include "ui/gl/gl_context_cgl.h"
#endif  // OS_MACOSX

namespace ui {

struct GpuSwitchingManager::PlatformSpecific {
#if defined(OS_MACOSX)
  CGLPixelFormatObj discrete_pixel_format;
#endif  // OS_MACOSX
};

// static
GpuSwitchingManager* GpuSwitchingManager::GetInstance() {
  return Singleton<GpuSwitchingManager>::get();
}

GpuSwitchingManager::GpuSwitchingManager()
    : gpu_switching_option_(gfx::PreferIntegratedGpu),
      gpu_switching_option_set_(false),
      supports_dual_gpus_(false),
      supports_dual_gpus_set_(false),
      gpu_count_(0),
      platform_specific_(new PlatformSpecific) {
#if defined(OS_MACOSX)
  platform_specific_->discrete_pixel_format = nullptr;
#endif  // OS_MACOSX
}

GpuSwitchingManager::~GpuSwitchingManager() {
#if defined(OS_MACOSX)
  if (platform_specific_->discrete_pixel_format)
    CGLReleasePixelFormat(platform_specific_->discrete_pixel_format);
#endif  // OS_MACOSX
}

void GpuSwitchingManager::ForceUseOfIntegratedGpu() {
  DCHECK(SupportsDualGpus());
  if (gpu_switching_option_set_) {
    DCHECK_EQ(gpu_switching_option_, gfx::PreferIntegratedGpu);
  } else {
    gpu_switching_option_ = gfx::PreferIntegratedGpu;
    gpu_switching_option_set_ = true;
  }
}

void GpuSwitchingManager::ForceUseOfDiscreteGpu() {
  DCHECK(SupportsDualGpus());
  if (gpu_switching_option_set_) {
    DCHECK_EQ(gpu_switching_option_, gfx::PreferDiscreteGpu);
  } else {
    gpu_switching_option_ = gfx::PreferDiscreteGpu;
    gpu_switching_option_set_ = true;
#if defined(OS_MACOSX)
    // Create a pixel format that lasts the lifespan of Chrome, so Chrome
    // stays on the discrete GPU.
    SwitchToDiscreteGpuMac();
#endif  // OS_MACOSX
  }
}

bool GpuSwitchingManager::SupportsDualGpus() {
  if (!supports_dual_gpus_set_) {
    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();
    bool flag = false;
    if (command_line.HasSwitch(switches::kSupportsDualGpus)) {
      // GPU process, flag is passed down from browser process.
      std::string flag_string = command_line.GetSwitchValueASCII(
          switches::kSupportsDualGpus);
      if (flag_string == "true") {
        flag = true;
      } else if (flag_string == "false") {
        flag = false;
      } else {
        NOTIMPLEMENTED();
      }
    } else {
      // Browser process.
      // We only compute this flag in the browser process.
#if defined(OS_MACOSX)
      flag = (gpu_count_ == 2);
      if (flag && command_line.HasSwitch(switches::kUseGL) &&
          command_line.GetSwitchValueASCII(switches::kUseGL) !=
            gfx::kGLImplementationDesktopName)
        flag = false;

      if (flag && !base::mac::IsOSLionOrLater())
        flag = false;
#endif  // OS_MACOSX
    }
    supports_dual_gpus_ = flag;
    supports_dual_gpus_set_ = true;
  }
  return supports_dual_gpus_;
}

void GpuSwitchingManager::SetGpuCount(size_t gpu_count) {
  gpu_count_ = gpu_count;
}

void GpuSwitchingManager::AddObserver(GpuSwitchingObserver* observer) {
  observer_list_.AddObserver(observer);
}

void GpuSwitchingManager::RemoveObserver(GpuSwitchingObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void GpuSwitchingManager::NotifyGpuSwitched() {
  FOR_EACH_OBSERVER(GpuSwitchingObserver, observer_list_, OnGpuSwitched());
}

gfx::GpuPreference GpuSwitchingManager::AdjustGpuPreference(
    gfx::GpuPreference gpu_preference) {
  if (!gpu_switching_option_set_)
    return gpu_preference;
  return gpu_switching_option_;
}

#if defined(OS_MACOSX)
void GpuSwitchingManager::SwitchToDiscreteGpuMac() {
  if (platform_specific_->discrete_pixel_format)
    return;
  CGLPixelFormatAttribute attribs[1];
  attribs[0] = static_cast<CGLPixelFormatAttribute>(0);
  GLint num_pixel_formats = 0;
  CGLChoosePixelFormat(attribs, &platform_specific_->discrete_pixel_format,
                       &num_pixel_formats);
}
#endif  // OS_MACOSX

}  // namespace ui
