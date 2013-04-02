// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_CRYPTO_KEY_SYSTEMS_INFO_H_
#define WEBKIT_MEDIA_CRYPTO_KEY_SYSTEMS_INFO_H_

#include <string>

namespace webkit_media {

struct MediaFormatAndKeySystem {
  const char* mime_type;
  const char* codecs_list;
  const char* key_system;
};

struct KeySystemPluginTypePair {
  const char* key_system;
  const char* plugin_type;
};

// Specifies the container and codec combinations supported by individual
// key systems. Each line is a container-codecs combination and the key system
// that supports it. Multiple codecs can be listed. In all cases, the container
// without a codec is also supported.
// This list is converted at runtime into individual container-codec-key system
// entries in KeySystems::key_system_map_.
extern const MediaFormatAndKeySystem kSupportedFormatKeySystemCombinations[];
extern const int kNumSupportedFormatKeySystemCombinations;

// There should be one entry for each key system.
extern const KeySystemPluginTypePair kKeySystemToPluginTypeMapping[];
extern const int kNumKeySystemToPluginTypeMapping;

// Returns whether |key_system| is compatible with the user's system.
bool IsSystemCompatible(const std::string& key_system);

// Returns the name that UMA will use for the given |key_system|.
// This function can be called frequently. Hence this function should be
// implemented not to impact performance.
std::string KeySystemNameForUMAGeneric(const std::string& key_system);

// Returns whether built-in AesDecryptor can be used for the given |key_system|.
bool CanUseBuiltInAesDecryptor(const std::string& key_system);

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_CRYPTO_KEY_SYSTEMS_INFO_H_
