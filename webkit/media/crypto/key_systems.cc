// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/crypto/key_systems.h"

#include "media/base/decryptor.h"
#include "media/crypto/aes_decryptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

namespace webkit_media {

namespace {

const char kClearKeyKeySystem[] = "webkit-org.w3.clearkey";

struct MediaFormatAndKeySystem {
  const char* mime_type;
  const char* codec;
  const char* key_system;
};

static const MediaFormatAndKeySystem
supported_format_key_system_combinations[] = {
  // TODO(ddorwin): Reconsider based on how usage of this class evolves.
  // For now, this class is stateless, so we do not have the opportunity to
  // build a list using ParseCodecString() like
  // net::MimeUtil::InitializeMimeTypeMaps(). Therfore, the following line must
  // be separate entries.
  // { "video/webm", "vorbis,vp8,vp8.0", kClearKeyKeySystem },
  { "video/webm", "vorbis", kClearKeyKeySystem },
  { "video/webm", "vp8", kClearKeyKeySystem },
  { "video/webm", "vp8.0", kClearKeyKeySystem },
  { "audio/webm", "vorbis", kClearKeyKeySystem },
  { "video/webm", "", kClearKeyKeySystem },
  { "audio/webm", "", kClearKeyKeySystem }
};

bool IsSupportedKeySystemWithContainerAndCodec(const std::string& mime_type,
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

}  // namespace

bool IsSupportedKeySystem(const WebKit::WebString& key_system) {
  if (key_system == kClearKeyKeySystem)
    return true;
  return false;
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

scoped_ptr<media::Decryptor> CreateDecryptor(const std::string& key_system,
                                             media::DecryptorClient* client) {
  if (key_system == kClearKeyKeySystem)
    return scoped_ptr<media::Decryptor>(new media::AesDecryptor(client));
  return scoped_ptr<media::Decryptor>();
}

}  // namespace webkit_media
