// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/crypto/key_systems_info.h"

#include "base/basictypes.h"

#include "widevine_cdm_version.h" // In SHARED_INTERMEDIATE_DIR.

#if defined(WIDEVINE_CDM_AVAILABLE) && \
    defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include <gnu/libc-version.h>
#include "base/version.h"
#endif

namespace webkit_media {

static const char kClearKeyKeySystem[] = "webkit-org.w3.clearkey";

static const char kExternalClearKeyKeySystem[] =
    "org.chromium.externalclearkey";

// TODO(ddorwin): Automatically support parent systems: http://crbug.com/164303.
static const char kWidevineBaseKeySystem[] = "com.widevine";

#if defined(WIDEVINE_CDM_CENC_SUPPORT_AVAILABLE)
// The supported codecs depend on what the CDM provides.
static const char kWidevineVideoMp4Codecs[] =
#if defined(WIDEVINE_CDM_AVC1_SUPPORT_AVAILABLE) && \
    defined(WIDEVINE_CDM_AAC_SUPPORT_AVAILABLE)
    "avc1,mp4a";
#elif defined(WIDEVINE_CDM_AVC1_SUPPORT_AVAILABLE)
    "avc1";
#else
    "";  // No codec strings are supported.
#endif

static const char kWidevineAudioMp4Codecs[] =
#if defined(WIDEVINE_CDM_AAC_SUPPORT_AVAILABLE)
    "mp4a";
#else
    "";  // No codec strings are supported.
#endif
#endif  // defined(WIDEVINE_CDM_CENC_SUPPORT_AVAILABLE)

const MediaFormatAndKeySystem kSupportedFormatKeySystemCombinations[] = {
  // Clear Key.
  { "video/webm", "vorbis,vp8,vp8.0", kClearKeyKeySystem },
  { "audio/webm", "vorbis", kClearKeyKeySystem },
#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
  { "video/mp4", "avc1,mp4a", kClearKeyKeySystem },
  { "audio/mp4", "mp4a", kClearKeyKeySystem },
#endif

  // External Clear Key (used for testing).
  { "video/webm", "vorbis,vp8,vp8.0", kExternalClearKeyKeySystem },
  { "audio/webm", "vorbis", kExternalClearKeyKeySystem },
#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
  { "video/mp4", "avc1,mp4a", kExternalClearKeyKeySystem },
  { "audio/mp4", "mp4a", kExternalClearKeyKeySystem },
#endif

#if defined(WIDEVINE_CDM_AVAILABLE)
  // Widevine.
  { "video/webm", "vorbis,vp8,vp8.0", kWidevineKeySystem },
  { "audio/webm", "vorbis", kWidevineKeySystem },
  { "video/webm", "vorbis,vp8,vp8.0", kWidevineBaseKeySystem },
  { "audio/webm", "vorbis", kWidevineBaseKeySystem },
#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
#if defined(WIDEVINE_CDM_CENC_SUPPORT_AVAILABLE)
  { "video/mp4", kWidevineVideoMp4Codecs, kWidevineKeySystem },
  { "video/mp4", kWidevineVideoMp4Codecs, kWidevineBaseKeySystem },
  { "audio/mp4", kWidevineAudioMp4Codecs, kWidevineKeySystem },
  { "audio/mp4", kWidevineAudioMp4Codecs, kWidevineBaseKeySystem },
#endif  // defined(WIDEVINE_CDM_CENC_SUPPORT_AVAILABLE)
#endif  // defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
#endif  // WIDEVINE_CDM_AVAILABLE
};

const int kNumSupportedFormatKeySystemCombinations =
    arraysize(kSupportedFormatKeySystemCombinations);

const KeySystemPluginTypePair kKeySystemToPluginTypeMapping[] = {
  // TODO(xhwang): Update this with the real plugin name.
  { kExternalClearKeyKeySystem, "application/x-ppapi-clearkey-cdm"},
#if defined(WIDEVINE_CDM_AVAILABLE)
  { kWidevineKeySystem, kWidevineCdmPluginMimeType}
#endif  // WIDEVINE_CDM_AVAILABLE
};

const int kNumKeySystemToPluginTypeMapping =
    arraysize(kKeySystemToPluginTypeMapping);

bool IsSystemCompatible(const std::string& key_system) {
#if defined(WIDEVINE_CDM_AVAILABLE) && \
    defined(OS_LINUX) && !defined(OS_CHROMEOS)
  if (key_system == kWidevineKeySystem) {
    Version glibc_version(gnu_get_libc_version());
    DCHECK(glibc_version.IsValid());
    return !glibc_version.IsOlderThan(WIDEVINE_CDM_MIN_GLIBC_VERSION);
  }
#endif
  return true;
}

std::string KeySystemNameForUMAGeneric(const std::string& key_system) {
  if (key_system == kClearKeyKeySystem)
    return "ClearKey";
#if defined(WIDEVINE_CDM_AVAILABLE)
  if (key_system == kWidevineKeySystem)
    return "Widevine";
#endif  // WIDEVINE_CDM_AVAILABLE
  return "Unknown";
}

bool CanUseBuiltInAesDecryptor(const std::string& key_system) {
  return  key_system == kClearKeyKeySystem;
}

} // namespace webkit_media
