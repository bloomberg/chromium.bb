// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_TRANSMISSION_ENCODING_INFO_HANDLER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_TRANSMISSION_ENCODING_INFO_HANDLER_H_

#include <memory>

#include "third_party/blink/public/platform/modules/media_capabilities/web_media_capabilities_callbacks.h"
#include "third_party/blink/public/platform/web_common.h"

namespace blink {

struct WebMediaConfiguration;

// Platform interface of a TransmissionEncodingInfoHandler.
// It handle MediaCapabilities.encodingInfo() API with
// "transmission" type.
class BLINK_PLATFORM_EXPORT WebTransmissionEncodingInfoHandler {
 public:
  virtual ~WebTransmissionEncodingInfoHandler() = default;

  // Queries the capabilities of the given encoding configuration and passes
  // WebMediaCapabilitiesInfo result via callbacks.
  // It implements WICG Media Capabilities encodingInfo() call for transmission
  // encoding.
  // https://wicg.github.io/media-capabilities/#media-capabilities-interface
  using OnMediaCapabilitiesEncodingInfoCallback =
      base::OnceCallback<void(std::unique_ptr<WebMediaCapabilitiesInfo>)>;
  virtual void EncodingInfo(const WebMediaConfiguration&,
                            OnMediaCapabilitiesEncodingInfoCallback) const = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_TRANSMISSION_ENCODING_INFO_HANDLER_H_
