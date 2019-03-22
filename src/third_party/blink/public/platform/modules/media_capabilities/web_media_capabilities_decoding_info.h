// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_MEDIA_CAPABILITIES_WEB_MEDIA_CAPABILITIES_DECODING_INFO_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_MEDIA_CAPABILITIES_WEB_MEDIA_CAPABILITIES_DECODING_INFO_H_

#include "third_party/blink/public/platform/modules/media_capabilities/web_media_capabilities_info.h"
#include "third_party/blink/public/platform/web_content_decryption_module_access.h"

namespace blink {

// Represents a MediaCapabilitiesDecodingInfo dictionary to be used outside of
// Blink. This is set by consumers and sent back to Blink.
struct WebMediaCapabilitiesDecodingInfo : WebMediaCapabilitiesInfo {
  std::unique_ptr<WebContentDecryptionModuleAccess>
      content_decryption_module_access;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_MEDIA_CAPABILITIES_WEB_MEDIA_CAPABILITIES_DECODING_INFO_H_
