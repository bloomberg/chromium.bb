// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_attributes.h"

#include <type_traits>

#include "base/atomicops.h"
#include "base/logging.h"
#include "base/macros.h"
#include "build/build_config.h"

#if defined(OS_WIN)
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
#if defined(OS_WIN)
  { "DirectX-Capturer", &webrtc::ScreenCapturerWinDirectx::IsSupported },
  // TODO(zijiehe): Add DirectX version attributes. Blocked by change
  // https://codereview.chromium.org/2468083002/.
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
