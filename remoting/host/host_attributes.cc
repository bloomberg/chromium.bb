// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_attributes.h"

#include <type_traits>

#include "base/atomicops.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include <D3DCommon.h>

#include "base/win/windows_version.h"
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
};

}  // namespace

static_assert(std::is_pod<Attribute>::value, "Attribute should be POD.");

std::string GetHostAttributes() {
  std::vector<base::StringPiece> result;
  // By using ranged for-loop, MSVC throws error C3316:
  // 'const remoting::StaticAttribute [0]':
  // an array of unknown size cannot be used in a range-based for statement.
  for (size_t i = 0; i < arraysize(kAttributes); i++) {
    const auto& attribute = kAttributes[i];
    DCHECK_EQ(std::string(attribute.name).find(kSeparator), std::string::npos);
    if (attribute.get_value_func()) {
      result.push_back(attribute.name);
    }
  }
#if defined(OS_WIN)
  {
    webrtc::DxgiDuplicatorController::D3dInfo info;
    webrtc::ScreenCapturerWinDirectx::RetrieveD3dInfo(&info);
    if (info.min_feature_level >= D3D_FEATURE_LEVEL_10_0) {
      result.push_back("MinD3DGT10");
    }
    if (info.min_feature_level >= D3D_FEATURE_LEVEL_11_0) {
      result.push_back("MinD3DGT11");
    }
    if (info.min_feature_level >= D3D_FEATURE_LEVEL_12_0) {
      result.push_back("MinD3DGT12");
    }

    auto version = base::win::GetVersion();
    if (version >= base::win::VERSION_WIN8) {
      result.push_back("Win8+");
    }
    if (version >= base::win::VERSION_WIN8_1) {
      result.push_back("Win81+");
    }
    if (version >= base::win::VERSION_WIN10) {
      result.push_back("Win10+");
    }
  }
#endif

  return base::JoinString(result, kSeparator);
}

}  // namespace remoting
