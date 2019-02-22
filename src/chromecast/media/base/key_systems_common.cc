// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/key_systems_common.h"

#include <cstddef>

#include "media/base/key_system_names.h"
#include "third_party/widevine/cdm/buildflags.h"

#if BUILDFLAG(ENABLE_WIDEVINE)
#include "third_party/widevine/cdm/widevine_cdm_common.h"  // nogncheck
#endif

namespace chromecast {
namespace media {

#if defined(PLAYREADY_CDM_AVAILABLE)
const char kChromecastPlayreadyKeySystem[] = "com.chromecast.playready";
#endif  // defined(PLAYREADY_CDM_AVAILABLE)

CastKeySystem GetKeySystemByName(const std::string& key_system_name) {
#if BUILDFLAG(ENABLE_WIDEVINE)
  if (key_system_name.compare(kWidevineKeySystem) == 0) {
    return KEY_SYSTEM_WIDEVINE;
  }
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

#if defined(PLAYREADY_CDM_AVAILABLE)
  if (key_system_name.compare(kChromecastPlayreadyKeySystem) == 0) {
    return KEY_SYSTEM_PLAYREADY;
  }
#endif  // defined(PLAYREADY_CDM_AVAILABLE)

  if (::media::IsClearKey(key_system_name)) {
    return KEY_SYSTEM_CLEAR_KEY;
  }

  return KEY_SYSTEM_NONE;
}

}  // namespace media
}  // namespace chromecast
