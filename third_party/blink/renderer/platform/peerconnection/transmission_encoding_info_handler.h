// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_TRANSMISSION_ENCODING_INFO_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_TRANSMISSION_ENCODING_INFO_HANDLER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "third_party/blink/public/platform/modules/media_capabilities/web_media_capabilities_info.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {
struct WebMediaConfiguration;
struct WebVideoConfiguration;
}  // namespace blink

namespace webrtc {
class VideoEncoderFactory;
}  // namespace webrtc

namespace blink {

// blink::WebTransmissionEncodingInfoHandler implementation.
class PLATFORM_EXPORT TransmissionEncodingInfoHandler {
 public:
  static TransmissionEncodingInfoHandler* Instance();

  TransmissionEncodingInfoHandler();
  // Constructor for unittest to inject VideoEncodeFactory instance and
  // |cpu_hd_smooth|.
  explicit TransmissionEncodingInfoHandler(
      std::unique_ptr<webrtc::VideoEncoderFactory> video_encoder_factory,
      bool cpu_hd_smooth);
  ~TransmissionEncodingInfoHandler();

  // Queries the capabilities of the given encoding configuration and passes
  // WebMediaCapabilitiesInfo result via callbacks.
  // It implements WICG Media Capabilities encodingInfo() call for transmission
  // encoding.
  // https://wicg.github.io/media-capabilities/#media-capabilities-interface
  using OnMediaCapabilitiesEncodingInfoCallback =
      base::OnceCallback<void(std::unique_ptr<WebMediaCapabilitiesInfo>)>;
  void EncodingInfo(const blink::WebMediaConfiguration& configuration,
                    OnMediaCapabilitiesEncodingInfoCallback cb) const;

 private:
  // Extracts supported video/audio codec name from |mime_type|. Returns "" if
  // it is not supported.
  std::string ExtractSupportedCodecFromMimeType(
      const std::string& mime_type) const;

  // True if it can encode |configuration| smoothly via CPU.
  bool CanCpuEncodeSmoothly(
      const blink::WebVideoConfiguration& configuration) const;

  // TODO(crbug.com/787254): Replace the use of std::string.
  //
  // List of supported video codecs.
  base::flat_set<std::string> supported_video_codecs_;
  // List of hardware accelerated codecs.
  base::flat_set<std::string> hardware_accelerated_video_codecs_;
  // List of supported audio codecs.
  base::flat_set<std::string> supported_audio_codecs_;

  // True if CPU is capable to encode 720p video smoothly.
  bool cpu_hd_smooth_;

  DISALLOW_COPY_AND_ASSIGN(TransmissionEncodingInfoHandler);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_TRANSMISSION_ENCODING_INFO_HANDLER_H_
