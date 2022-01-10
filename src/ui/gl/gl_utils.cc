// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains some useful utilities for the ui/gl classes.

#include "ui/gl/gl_utils.h"

#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/logging.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_switches.h"

#if defined(USE_EGL)
#include "ui/gl/gl_surface_egl.h"
#endif  // defined(USE_EGL)

#if defined(OS_ANDROID)
#include "base/posix/eintr_wrapper.h"
#include "third_party/libsync/src/include/sync/sync.h"
#endif

#if defined(OS_WIN)
#include <d3d11_1.h>
#include "base/strings/stringprintf.h"
#include "media/base/win/mf_helpers.h"
#include "ui/gl/direct_composition_surface_win.h"
#endif

namespace gl {
namespace {

int GetIntegerv(unsigned int name) {
  int value = 0;
  glGetIntegerv(name, &value);
  return value;
}

}  // namespace

// Used by chrome://gpucrash and gpu_benchmarking_extension's
// CrashForTesting.
void Crash() {
  DVLOG(1) << "GPU: Simulating GPU crash";
  // Good bye, cruel world.
  volatile int* it_s_the_end_of_the_world_as_we_know_it = nullptr;
  *it_s_the_end_of_the_world_as_we_know_it = 0xdead;
}

// Used by chrome://gpuhang.
void Hang() {
  DVLOG(1) << "GPU: Simulating GPU hang";
  int do_not_delete_me = 0;
  for (;;) {
    // Do not sleep here. The GPU watchdog timer tracks
    // the amount of user time this thread is using and
    // it doesn't use much while calling Sleep.

    // The following are multiple mechanisms to prevent compilers from
    // optimizing out the endless loop. Hope at least one of them works.
    base::debug::Alias(&do_not_delete_me);
    ++do_not_delete_me;

    __asm__ volatile("");
  }
}

#if defined(OS_ANDROID)
base::ScopedFD MergeFDs(base::ScopedFD a, base::ScopedFD b) {
  if (!a.is_valid())
    return b;
  if (!b.is_valid())
    return a;

  base::ScopedFD merged(HANDLE_EINTR(sync_merge("", a.get(), b.get())));
  if (!merged.is_valid())
    LOG(ERROR) << "Failed to merge fences.";
  return merged;
}
#endif

bool UsePassthroughCommandDecoder(const base::CommandLine* command_line) {
  std::string switch_value;
  if (command_line->HasSwitch(switches::kUseCmdDecoder)) {
    switch_value = command_line->GetSwitchValueASCII(switches::kUseCmdDecoder);
  }

  if (switch_value == kCmdDecoderPassthroughName) {
    return true;
  } else if (switch_value == kCmdDecoderValidatingName) {
    return false;
  } else {
    // Unrecognized or missing switch, use the default.
    return features::UsePassthroughCommandDecoder();
  }
}

bool PassthroughCommandDecoderSupported() {
#if defined(USE_EGL)
  // Using the passthrough command buffer requires that specific ANGLE
  // extensions are exposed
  return gl::GLSurfaceEGL::IsCreateContextBindGeneratesResourceSupported() &&
         gl::GLSurfaceEGL::IsCreateContextWebGLCompatabilitySupported() &&
         gl::GLSurfaceEGL::IsRobustResourceInitSupported() &&
         gl::GLSurfaceEGL::IsDisplayTextureShareGroupSupported() &&
         gl::GLSurfaceEGL::IsCreateContextClientArraysSupported();
#else
  // The passthrough command buffer is only supported on top of ANGLE/EGL
  return false;
#endif  // defined(USE_EGL)
}

#if defined(OS_WIN)
// This function is thread safe.
bool AreOverlaysSupportedWin() {
  return gl::DirectCompositionSurfaceWin::AreOverlaysSupported();
}

unsigned int FrameRateToPresentDuration(float frame_rate) {
  if (frame_rate == 0)
    return 0u;
  // Present duration unit is 100 ns.
  return static_cast<unsigned int>(1.0E7 / frame_rate);
}

UINT GetOverlaySupportFlags(DXGI_FORMAT format) {
  return gl::DirectCompositionSurfaceWin::GetOverlaySupportFlags(format);
}

unsigned int DirectCompositionRootSurfaceBufferCount() {
  return base::FeatureList::IsEnabled(features::kDCompTripleBufferRootSwapChain)
             ? 3u
             : 2u;
}

bool ShouldForceDirectCompositionRootSurfaceFullDamage() {
  static bool should_force = []() {
    const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
    if (cmd_line->HasSwitch(
            switches::kDirectCompositionForceFullDamageForTesting)) {
      return true;
    }
    UINT brga_flags = DirectCompositionSurfaceWin::GetOverlaySupportFlags(
        DXGI_FORMAT_B8G8R8A8_UNORM);
    constexpr UINT kSupportBits =
        DXGI_OVERLAY_SUPPORT_FLAG_DIRECT | DXGI_OVERLAY_SUPPORT_FLAG_SCALING;
    if ((brga_flags & kSupportBits) == 0)
      return false;
    if (!base::FeatureList::IsEnabled(
            features::kDirectCompositionForceFullDamage)) {
      return false;
    }
    return true;
  }();
  return should_force;
}

// Labels swapchain buffers with the string name_prefix + _Buffer_ +
// <buffer_number>
void LabelSwapChainBuffers(IDXGISwapChain* swap_chain,
                           const char* name_prefix) {
  DXGI_SWAP_CHAIN_DESC desc;
  HRESULT hr = swap_chain->GetDesc(&desc);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to GetDesc from swap chain: "
                << logging::SystemErrorCodeToString(hr);
    return;
  }
  for (unsigned int i = 0; i < desc.BufferCount; i++) {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> swap_chain_buffer;
    hr = swap_chain->GetBuffer(i, IID_PPV_ARGS(&swap_chain_buffer));
    if (FAILED(hr)) {
      DLOG(ERROR) << "GetBuffer on swap chain buffer " << i
                  << "failed: " << logging::SystemErrorCodeToString(hr);
      return;
    }
    const std::string buffer_name =
        base::StringPrintf("%s_Buffer_%d", name_prefix, i);
    hr = media::SetDebugName(swap_chain_buffer.Get(), buffer_name.c_str());
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to label swap chain buffer " << i << ": "
                  << logging::SystemErrorCodeToString(hr);
    }
  }
}

// Same as LabelSwapChainAndBuffers, but only does the buffers. Used for resize
// operations
void LabelSwapChainAndBuffers(IDXGISwapChain* swap_chain,
                              const char* name_prefix) {
  media::SetDebugName(swap_chain, name_prefix);
  LabelSwapChainBuffers(swap_chain, name_prefix);
}
#endif  // OS_WIN

#if defined(OS_MAC)

ScopedEnableTextureRectangleInShaderCompiler::
    ScopedEnableTextureRectangleInShaderCompiler(gl::GLApi* gl_api) {
  if (gl_api) {
    DCHECK(!gl_api->glIsEnabledFn(GL_TEXTURE_RECTANGLE_ANGLE));
    gl_api->glEnableFn(GL_TEXTURE_RECTANGLE_ANGLE);
    gl_api_ = gl_api;
  } else {
    gl_api_ = nullptr;  // Signal to the destructor that this is a no-op.
  }
}

ScopedEnableTextureRectangleInShaderCompiler::
    ~ScopedEnableTextureRectangleInShaderCompiler() {
  if (gl_api_)
    gl_api_->glDisableFn(GL_TEXTURE_RECTANGLE_ANGLE);
}

#endif  // defined(OS_MAC)

ScopedPixelStore::ScopedPixelStore(unsigned int name, int value)
    : name_(name), old_value_(GetIntegerv(name)), value_(value) {
  if (value_ != old_value_)
    glPixelStorei(name_, value_);
}

ScopedPixelStore::~ScopedPixelStore() {
  if (value_ != old_value_)
    glPixelStorei(name_, old_value_);
}

}  // namespace gl
