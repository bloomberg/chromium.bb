// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_attributes.h"

#if defined(OS_WIN)
#include <D3DCommon.h>
#endif

#include <type_traits>

#include "base/atomicops.h"
#include "base/logging.h"
#include "base/macros.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "third_party/webrtc/modules/desktop_capture/win/dxgi_duplicator_controller.h"
#include "third_party/webrtc/modules/desktop_capture/win/screen_capturer_win_directx.h"
#endif

namespace remoting {

namespace {
static constexpr char kSeparator[] = ",";

struct Attribute {
  const char* name;
  bool(* get_value_func)();
};

inline constexpr bool IsDebug() {
#if defined(NDEBUG)
  return false;
#else
  return true;
#endif
}

inline constexpr bool IsChromeBranded() {
#if defined(GOOGLE_CHROME_BUILD)
  return true;
#elif defined(CHROMIUM_BUILD)
  return false;
#else
  #error Only Chrome and Chromium brands are supported.
#endif
}

inline constexpr bool IsChromiumBranded() {
  return !IsChromeBranded();
}

inline constexpr bool IsOfficialBuild() {
#if defined(OFFICIAL_BUILD)
  return true;
#else
  return false;
#endif
}

inline constexpr bool IsNonOfficialBuild() {
  return !IsOfficialBuild();
}

#if defined(OS_WIN)
inline bool MinD3DFeatureLevelGreatThan10() {
  webrtc::DxgiDuplicatorController::D3dInfo info;
  if (webrtc::DxgiDuplicatorController::Instance()->RetrieveD3dInfo(&info)) {
    return info.min_feature_level >= D3D_FEATURE_LEVEL_10_0;
  }
  return false;
}

inline bool MinD3DFeatureLevelGreatThan11() {
  webrtc::DxgiDuplicatorController::D3dInfo info;
  if (webrtc::DxgiDuplicatorController::Instance()->RetrieveD3dInfo(&info)) {
    return info.min_feature_level >= D3D_FEATURE_LEVEL_11_0;
  }
  return false;
}

inline bool MinD3DFeatureLevelGreatThan12() {
  webrtc::DxgiDuplicatorController::D3dInfo info;
  if (webrtc::DxgiDuplicatorController::Instance()->RetrieveD3dInfo(&info)) {
    return info.min_feature_level >= D3D_FEATURE_LEVEL_12_0;
  }
  return false;
}
#endif

// By using arraysize() macro in base/macros.h, it's illegal to have empty
// arrays.
//
// error: no matching function for call to 'ArraySizeHelper'
// note: candidate template ignored: substitution failure
// [with T = const remoting::StaticAttribute, N = 0]:
// zero-length arrays are not permitted in C++.
//
// So we need IsDebug() function, and "Debug-Build" Attribute.

static constexpr Attribute kAttributes[] = {
  { "Debug-Build", &IsDebug },
  { "ChromeBrand", &IsChromeBranded },
  { "ChromiumBrand", &IsChromiumBranded },
  { "OfficialBuild", &IsOfficialBuild },
  { "NonOfficialBuild", &IsNonOfficialBuild },
#if defined(OS_WIN)
  { "DirectX-Capturer", &webrtc::ScreenCapturerWinDirectx::IsSupported },
  { "MinD3DGT10", &MinD3DFeatureLevelGreatThan10 },
  { "MinD3DGT11", &MinD3DFeatureLevelGreatThan11 },
  { "MinD3DGT12", &MinD3DFeatureLevelGreatThan12 },
#endif
};

}  // namespace

static_assert(std::is_pod<Attribute>::value, "Attribute should be POD.");

std::string GetHostAttributes() {
  std::string result;
  // By using ranged for-loop, MSVC throws error C3316:
  // 'const remoting::StaticAttribute [0]':
  // an array of unknown size cannot be used in a range-based for statement.
  for (size_t i = 0; i < arraysize(kAttributes); i++) {
    const auto& attribute = kAttributes[i];
    DCHECK_EQ(std::string(attribute.name).find(kSeparator), std::string::npos);
    if (attribute.get_value_func()) {
      if (!result.empty()) {
        result.append(kSeparator);
      }
      result.append(attribute.name);
    }
  }

  return result;
}

}  // namespace remoting
