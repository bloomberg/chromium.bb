// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gpu_switching_manager.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "ui/gl/gl_context_cgl.h"
#endif  // OS_MACOSX

namespace {

#if defined(OS_MACOSX)
bool SupportsOnlineAndOfflineRenderers() {
  // Enumerate all hardware-accelerated renderers. If we find one
  // online and one offline, assume we're on a dual-GPU system.
  GLuint display_mask = static_cast<GLuint>(-1);
  CGLRendererInfoObj renderer_info = NULL;
  GLint num_renderers = 0;

  bool found_online = false;
  bool found_offline = false;

  if (CGLQueryRendererInfo(display_mask,
                           &renderer_info,
                           &num_renderers) != kCGLNoError) {
    return false;
  }

  gfx::ScopedCGLRendererInfoObj scoper(renderer_info);

  for (GLint i = 0; i < num_renderers; ++i) {
    GLint accelerated = 0;
    if (CGLDescribeRenderer(renderer_info,
                            i,
                            kCGLRPAccelerated,
                            &accelerated) != kCGLNoError) {
      return false;
    }

    if (!accelerated)
      continue;

    GLint online = 0;
    if (CGLDescribeRenderer(renderer_info,
                            i,
                            kCGLRPOnline,
                            &online) != kCGLNoError) {
      return false;
    }

    if (online) {
      found_online = true;
    } else {
      found_offline = true;
    }
  }

  return (found_online && found_offline);
}
#endif  // OS_MACOSX

}  // namespace anonymous

namespace ui {

// static
GpuSwitchingManager* GpuSwitchingManager::GetInstance() {
  return Singleton<GpuSwitchingManager>::get();
}

GpuSwitchingManager::GpuSwitchingManager()
    : gpu_switching_option_(gfx::PreferIntegratedGpu),
      gpu_switching_option_set_(false),
      supports_dual_gpus_(false),
      supports_dual_gpus_set_(false) {
#if defined(OS_MACOSX)
  discrete_pixel_format_ = NULL;
#endif  // OS_MACOSX
}

GpuSwitchingManager::~GpuSwitchingManager() {
#if defined(OS_MACOSX)
  if (discrete_pixel_format_)
    CGLReleasePixelFormat(discrete_pixel_format_);
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
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
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
      flag = SupportsOnlineAndOfflineRenderers();
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

gfx::GpuPreference GpuSwitchingManager::AdjustGpuPreference(
    gfx::GpuPreference gpu_preference) {
  if (!gpu_switching_option_set_)
    return gpu_preference;
  return gpu_switching_option_;
}

#if defined(OS_MACOSX)
void GpuSwitchingManager::SwitchToDiscreteGpuMac() {
  if (discrete_pixel_format_)
    return;
  CGLPixelFormatAttribute attribs[1];
  attribs[0] = static_cast<CGLPixelFormatAttribute>(0);
  GLint num_pixel_formats = 0;
  CGLChoosePixelFormat(attribs, &discrete_pixel_format_, &num_pixel_formats);
}
#endif  // OS_MACOSX

}  // namespace ui
