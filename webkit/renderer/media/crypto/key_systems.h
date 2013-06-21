// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_MEDIA_CRYPTO_KEY_SYSTEMS_H_
#define WEBKIT_RENDERER_MEDIA_CRYPTO_KEY_SYSTEMS_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"

namespace WebKit {
class WebString;
}

namespace webkit_media {

// Returns whether |key_sytem| is supported at all.
// Call IsSupportedKeySystemWithMediaMimeType() to determine whether a
// |key_system| supports a specific type of media.
bool IsSupportedKeySystem(const WebKit::WebString& key_system);

// Returns whether |key_sytem| supports the specified media type and codec(s).
bool IsSupportedKeySystemWithMediaMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs,
    const std::string& key_system);

// Returns a name for |key_system| suitable to UMA logging.
std::string KeySystemNameForUMA(const std::string& key_system);
std::string KeySystemNameForUMA(const WebKit::WebString& key_system);

// Returns whether AesDecryptor can be used for the given |key_system|.
bool CanUseAesDecryptor(const std::string& key_system);

#if defined(ENABLE_PEPPER_CDMS)
// Returns the Pepper MIME type for |key_system|.
// Returns an empty string if |key_system| is unknown or not Pepper-based.
std::string GetPepperType(const std::string& key_system);
#endif

#if defined(OS_ANDROID)
// Convert |key_system| to 16-byte Android UUID.
std::vector<uint8> GetUUID(const std::string& key_system);
#endif

}  // namespace webkit_media

#endif  // WEBKIT_RENDERER_MEDIA_CRYPTO_KEY_SYSTEMS_H_
