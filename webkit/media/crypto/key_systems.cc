// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "webkit/media/crypto/key_systems.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "net/base/mime_util.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "webkit/media/crypto/key_systems_info.h"

namespace webkit_media {

// Convert a WebString to ASCII, falling back on an empty string in the case
// of a non-ASCII string.
static std::string ToASCIIOrEmpty(const WebKit::WebString& string) {
  return IsStringASCII(string) ? UTF16ToASCII(string) : std::string();
}

class KeySystems {
 public:
  bool IsSupportedKeySystem(const std::string& key_system);

  bool IsSupportedKeySystemWithMediaMimeType(
      const std::string& mime_type,
      const std::vector<std::string>& codecs,
      const std::string& key_system);

 private:
  friend struct base::DefaultLazyInstanceTraits<KeySystems>;

  typedef base::hash_set<std::string> CodecMappings;
  typedef std::map<std::string, CodecMappings> MimeTypeMappings;
  typedef std::map<std::string, MimeTypeMappings> KeySystemMappings;

  KeySystems();

  bool IsSupportedKeySystemWithContainerAndCodec(
      const std::string& mime_type,
      const std::string& codec,
      const std::string& key_system);

  KeySystemMappings key_system_map_;

  DISALLOW_COPY_AND_ASSIGN(KeySystems);
};

static base::LazyInstance<KeySystems> g_key_systems = LAZY_INSTANCE_INITIALIZER;

KeySystems::KeySystems() {
  // Initialize the supported media type/key system combinations.
  for (int i = 0; i < kNumSupportedFormatKeySystemCombinations; ++i) {
    const MediaFormatAndKeySystem& combination =
        kSupportedFormatKeySystemCombinations[i];
    std::vector<std::string> mime_type_codecs;
    net::ParseCodecString(combination.codecs_list,
                          &mime_type_codecs,
                          false);

    CodecMappings codecs;
    for (size_t j = 0; j < mime_type_codecs.size(); ++j)
      codecs.insert(mime_type_codecs[j]);
    // Support the MIME type string alone, without codec(s) specified.
    codecs.insert("");

    // Key systems can be repeated, so there may already be an entry.
    KeySystemMappings::iterator key_system_iter =
        key_system_map_.find(combination.key_system);
    if (key_system_iter == key_system_map_.end()) {
      MimeTypeMappings mime_types_map;
      mime_types_map[combination.mime_type] = codecs;
      key_system_map_[combination.key_system] = mime_types_map;
    } else {
      MimeTypeMappings& mime_types_map = key_system_iter->second;
      // mime_types_map may not be repeated for a given key system.
      DCHECK(mime_types_map.find(combination.mime_type) ==
             mime_types_map.end());
      mime_types_map[combination.mime_type] = codecs;
    }
  }
}

bool KeySystems::IsSupportedKeySystem(const std::string& key_system) {
  bool is_supported = key_system_map_.find(key_system) != key_system_map_.end();
  return is_supported && IsSystemCompatible(key_system);
}

bool KeySystems::IsSupportedKeySystemWithContainerAndCodec(
    const std::string& mime_type,
    const std::string& codec,
    const std::string& key_system) {
  KeySystemMappings::const_iterator key_system_iter =
      key_system_map_.find(key_system);
  if (key_system_iter == key_system_map_.end())
    return false;

  const MimeTypeMappings& mime_types_map = key_system_iter->second;
  MimeTypeMappings::const_iterator mime_iter = mime_types_map.find(mime_type);
  if (mime_iter == mime_types_map.end())
    return false;

  const CodecMappings& codecs = mime_iter->second;
  return (codecs.find(codec) != codecs.end()) && IsSystemCompatible(key_system);
}

bool KeySystems::IsSupportedKeySystemWithMediaMimeType(
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

bool IsSupportedKeySystem(const WebKit::WebString& key_system) {
  return g_key_systems.Get().IsSupportedKeySystem(ToASCIIOrEmpty(key_system));
}

bool IsSupportedKeySystemWithMediaMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs,
    const std::string& key_system) {
  return g_key_systems.Get().IsSupportedKeySystemWithMediaMimeType(
      mime_type, codecs, key_system);
}

std::string KeySystemNameForUMA(const std::string& key_system) {
  return KeySystemNameForUMAGeneric(key_system);
}

std::string KeySystemNameForUMA(const WebKit::WebString& key_system) {
  return KeySystemNameForUMAGeneric(std::string(key_system.utf8().data()));
}

bool CanUseAesDecryptor(const std::string& key_system) {
  return CanUseBuiltInAesDecryptor(key_system);
}

std::string GetPluginType(const std::string& key_system) {
  for (int i = 0; i < kNumKeySystemToPluginTypeMapping; ++i) {
    if (kKeySystemToPluginTypeMapping[i].key_system == key_system)
      return kKeySystemToPluginTypeMapping[i].plugin_type;
  }

  return std::string();
}

}  // namespace webkit_media
