// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_CRYPTO_KEY_SYSTEMS_H_
#define WEBKIT_MEDIA_CRYPTO_KEY_SYSTEMS_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"

namespace WebKit {
class WebString;
}

namespace media {
class Decryptor;
class DecryptorClient;
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

// Creates and returns a decryptor that corresponds to the |key_system|.
// Returns NULL if the |key_system| is not supported.
scoped_ptr<media::Decryptor> CreateDecryptor(const std::string& key_system,
                                             media::DecryptorClient* client);

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_CRYPTO_KEY_SYSTEMS_H_
