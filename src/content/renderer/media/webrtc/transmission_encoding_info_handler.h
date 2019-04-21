// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_TRANSMISSION_ENCODING_INFO_HANDLER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_TRANSMISSION_ENCODING_INFO_HANDLER_H_

#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/platform/web_transmission_encoding_info_handler.h"

namespace blink {
struct WebVideoConfiguration;
}  // namespace blink

namespace webrtc {
class VideoEncoderFactory;
}  // namespace webrtc

namespace content {

// blink::WebTransmissionEncodingInfoHandler implementation.
class CONTENT_EXPORT TransmissionEncodingInfoHandler final
    : public blink::WebTransmissionEncodingInfoHandler {
 public:
  TransmissionEncodingInfoHandler();
  // Constructor for unittest to inject VideoEncodeFactory instance and
  // |cpu_hd_smooth|.
  explicit TransmissionEncodingInfoHandler(
      std::unique_ptr<webrtc::VideoEncoderFactory> video_encoder_factory,
      bool cpu_hd_smooth);
  ~TransmissionEncodingInfoHandler() override;

  // blink::WebTransmissionEncodingInfoHandler implementation.
  void EncodingInfo(const blink::WebMediaConfiguration& configuration,
                    OnMediaCapabilitiesEncodingInfoCallback cb) const override;

 private:
  // Extracts supported video/audio codec name from |mime_type|. Returns "" if
  // it is not supported.
  std::string ExtractSupportedCodecFromMimeType(
      const std::string& mime_type) const;

  // True if it can encode |configuration| smoothly via CPU.
  bool CanCpuEncodeSmoothly(
      const blink::WebVideoConfiguration& configuration) const;

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

}  // namespace content
#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_TRANSMISSION_ENCODING_INFO_HANDLER_H_
