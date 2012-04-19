// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_KEY_SYSTEMS_H_
#define WEBKIT_MEDIA_KEY_SYSTEMS_H_

#include <string>
#include <vector>

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

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_KEY_SYSTEMS_H_
