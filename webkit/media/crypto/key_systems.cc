// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/crypto/key_systems.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

namespace webkit_media {

static const char kClearKeyKeySystem[] = "webkit-org.w3.clearkey";
static const char kExternalClearKeyKeySystem[] =
    "org.chromium.externalclearkey";

struct MediaFormatAndKeySystem {
  const char* mime_type;
  const char* codec;
  const char* key_system;
};

struct KeySystemPluginTypePair {
  const char* key_system;
  const char* plugin_type;
};

static const MediaFormatAndKeySystem
supported_format_key_system_combinations[] = {
  // TODO(ddorwin): Reconsider based on how usage of this class evolves.
  // For now, this class is stateless, so we do not have the opportunity to
  // build a list using ParseCodecString() like
  // net::MimeUtil::InitializeMimeTypeMaps(). Therefore, the following line must
  // be separate entries.
  // { "video/webm", "vorbis,vp8,vp8.0", kClearKeyKeySystem },
  { "video/webm", "vorbis", kClearKeyKeySystem },
  { "video/webm", "vp8", kClearKeyKeySystem },
  { "video/webm", "vp8.0", kClearKeyKeySystem },
  { "audio/webm", "vorbis", kClearKeyKeySystem },
  { "video/webm", "", kClearKeyKeySystem },
  { "audio/webm", "", kClearKeyKeySystem },
  { "video/webm", "vorbis", kExternalClearKeyKeySystem },
  { "video/webm", "vp8", kExternalClearKeyKeySystem },
  { "video/webm", "vp8.0", kExternalClearKeyKeySystem },
  { "audio/webm", "vorbis", kExternalClearKeyKeySystem },
  { "video/webm", "", kExternalClearKeyKeySystem },
  { "audio/webm", "", kExternalClearKeyKeySystem }
};

static const KeySystemPluginTypePair key_system_to_plugin_type_mapping[] = {
  // TODO(xhwang): Update this with the real plugin name.
  { kExternalClearKeyKeySystem, "application/x-ppapi-clearkey-cdm" }
};

static bool IsSupportedKeySystemWithContainerAndCodec(
    const std::string& mime_type,
    const std::string& codec,
    const std::string& key_system) {
  for (size_t i = 0;
       i < arraysize(supported_format_key_system_combinations);
       ++i) {
    const MediaFormatAndKeySystem& combination =
        supported_format_key_system_combinations[i];
    if (combination.mime_type == mime_type &&
        combination.codec == codec &&
        combination.key_system == key_system)
      return true;
  }

  return false;
}

bool IsSupportedKeySystem(const WebKit::WebString& key_system) {
  return CanUseAesDecryptor(key_system.utf8().data()) ||
         !GetPluginType(key_system.utf8().data()).empty();
}

bool IsSupportedKeySystemWithMediaMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs,
    const std::string& key_system) {
  if (codecs.empty())
    return IsSupportedKeySystemWithContainerAndCodec(mime_type, "", key_system);

  for (size_t i = 0; i < codecs.size(); ++i) {
    if (!IsSupportedKeySystemWithContainerAndCodec(
            mime_type, codecs[i], key_system))
      return false;
  }

  return true;
}

bool CanUseAesDecryptor(const std::string& key_system) {
  return key_system == kClearKeyKeySystem;
}

std::string GetPluginType(const std::string& key_system) {
  for (size_t i = 0; i < arraysize(key_system_to_plugin_type_mapping); ++i) {
    if (key_system_to_plugin_type_mapping[i].key_system == key_system)
      return key_system_to_plugin_type_mapping[i].plugin_type;
  }

  return std::string();
}

}  // namespace webkit_media
