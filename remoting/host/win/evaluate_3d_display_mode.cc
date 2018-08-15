// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/evaluate_3d_display_mode.h"

#include <D3DCommon.h>
#include <comdef.h>
#include <dxgi1_3.h>
#include <wrl/client.h>

#include <iostream>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "remoting/host/evaluate_capability.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/switches.h"

namespace remoting {

namespace {

constexpr char k3dDisplayModeEnabled[] = "3D-Display-Mode";

}  // namespace

int Evaluate3dDisplayMode() {
  Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
  HRESULT hr = CreateDXGIFactory2(/*flags=*/0, IID_PPV_ARGS(&factory));
  if (hr != S_OK) {
    LOG(WARNING) << "CreateDXGIFactory2 failed: 0x" << std::hex << hr;
    return kInitializationFailed;
  }

  if (factory->IsWindowedStereoEnabled())
    std::cout << k3dDisplayModeEnabled << std::endl;

  return kSuccessExitCode;
}

bool Get3dDisplayModeEnabled() {
  std::string output;
  if (EvaluateCapability(kEvaluate3dDisplayMode, &output) != kSuccessExitCode)
    return false;

  base::TrimString(output, base::kWhitespaceASCII, &output);

  bool is_3d_display_mode_enabled = (output == k3dDisplayModeEnabled);
  LOG_IF(INFO, is_3d_display_mode_enabled) << "3D Display Mode enabled.";

  return is_3d_display_mode_enabled;
}

}  // namespace remoting
