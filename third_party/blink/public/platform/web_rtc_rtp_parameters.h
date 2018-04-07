// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RTC_RTP_PARAMETERS_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RTC_RTP_PARAMETERS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/optional.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {

enum class WebRTCPriorityType {
  VeryLow,
  Low,
  Medium,
  High,
};

enum class WebRTCDegradationPreference {
  MaintainFramerate,
  MaintainResolution,
  Balanced,
};

enum class WebRTCDtxStatus {
  Disabled,
  Enabled,
};

class BLINK_PLATFORM_EXPORT WebRTCRtpEncodingParameters {
 public:
  WebRTCRtpEncodingParameters();
  WebRTCRtpEncodingParameters(
      const base::Optional<uint8_t>& codec_payload_type,
      const base::Optional<WebRTCDtxStatus>& dtx,
      bool active,
      WebRTCPriorityType priority,
      const base::Optional<uint32_t>& ptime,
      const base::Optional<uint32_t>& max_bitrate,
      const base::Optional<uint32_t>& max_framerate,
      const base::Optional<double>& scale_resolution_down_by,
      const base::Optional<WebString>& rid)
      : codec_payload_type_(codec_payload_type),
        dtx_(dtx),
        active_(active),
        priority_(priority),
        ptime_(ptime),
        max_bitrate_(max_bitrate),
        max_framerate_(max_framerate),
        scale_resolution_down_by_(scale_resolution_down_by),
        rid_(rid) {}

  const base::Optional<uint8_t>& CodecPayloadType() const {
    return codec_payload_type_;
  }
  const base::Optional<WebRTCDtxStatus>& Dtx() const { return dtx_; }
  bool Active() const { return active_; }
  WebRTCPriorityType Priority() const { return priority_; }
  const base::Optional<uint32_t>& Ptime() const { return ptime_; }
  const base::Optional<uint32_t>& MaxBitrate() const { return max_bitrate_; }
  const base::Optional<uint32_t>& MaxFramerate() const {
    return max_framerate_;
  }
  const base::Optional<double>& ScaleResolutionDownBy() const {
    return scale_resolution_down_by_;
  }
  const base::Optional<WebString> Rid() const { return rid_; }

 private:
  base::Optional<uint8_t> codec_payload_type_;
  base::Optional<WebRTCDtxStatus> dtx_;
  bool active_;
  WebRTCPriorityType priority_;
  base::Optional<uint32_t> ptime_;
  base::Optional<uint32_t> max_bitrate_;
  base::Optional<uint32_t> max_framerate_;
  base::Optional<double> scale_resolution_down_by_;
  base::Optional<WebString> rid_;
};

class BLINK_PLATFORM_EXPORT WebRTCRtpHeaderExtensionParameters {
 public:
  WebRTCRtpHeaderExtensionParameters();
  WebRTCRtpHeaderExtensionParameters(const base::Optional<WebString>& uri,
                                     const base::Optional<uint16_t>& id,
                                     const base::Optional<bool>& encrypted)
      : uri_(uri), id_(id), encrypted_(encrypted) {}

  const base::Optional<WebString>& URI() const { return uri_; }
  const base::Optional<uint16_t>& Id() const { return id_; }
  const base::Optional<bool>& Encrypted() const { return encrypted_; }

 private:
  base::Optional<WebString> uri_;
  base::Optional<uint16_t> id_;
  base::Optional<bool> encrypted_;
};

class BLINK_PLATFORM_EXPORT WebRTCRtcpParameters {
 public:
  const base::Optional<WebString>& Cname() const;
  const base::Optional<bool>& ReducedSize() const;

 private:
  base::Optional<WebString> cname_;
  base::Optional<bool> reduced_size_;
};

class BLINK_PLATFORM_EXPORT WebRTCRtpCodecParameters {
 public:
  WebRTCRtpCodecParameters() = default;
  WebRTCRtpCodecParameters(const base::Optional<uint8_t>& payload_type,
                           const base::Optional<WebString>& mime_type,
                           const base::Optional<uint32_t>& clock_rate,
                           const base::Optional<uint16_t>& channels,
                           const base::Optional<WebString>& sdp_fmtp_line)
      : payload_type_(payload_type),
        mime_type_(mime_type),
        clock_rate_(clock_rate),
        channels_(channels),
        sdp_fmtp_line_(sdp_fmtp_line) {}

  const base::Optional<uint8_t>& PayloadType() const { return payload_type_; }
  const base::Optional<WebString>& MimeType() const { return mime_type_; }
  const base::Optional<uint32_t>& ClockRate() const { return clock_rate_; }
  const base::Optional<uint16_t>& Channels() const { return channels_; }
  const base::Optional<WebString>& SdpFmtpLine() const {
    return sdp_fmtp_line_;
  }

 private:
  base::Optional<uint8_t> payload_type_;
  base::Optional<WebString> mime_type_;
  base::Optional<uint32_t> clock_rate_;
  base::Optional<uint16_t> channels_;
  base::Optional<WebString> sdp_fmtp_line_;
};

class BLINK_PLATFORM_EXPORT WebRTCRtpParameters {
 public:
  WebRTCRtpParameters(
      const base::Optional<WebString>& transaction_id,
      const WebRTCRtcpParameters& rtcp,
      const WebVector<WebRTCRtpEncodingParameters>& encodings,
      const WebVector<WebRTCRtpHeaderExtensionParameters>& header_extensions,
      const WebVector<WebRTCRtpCodecParameters>& codecs,
      const base::Optional<WebRTCDegradationPreference>& degradation_preference)
      : transaction_id_(transaction_id),
        rtcp_(rtcp),
        encodings_(encodings),
        header_extensions_(header_extensions),
        codecs_(codecs),
        degradation_preference_(degradation_preference) {}

  const base::Optional<WebString>& TransactionId() const {
    return transaction_id_;
  }
  const WebVector<WebRTCRtpEncodingParameters>& Encodings() const {
    return encodings_;
  }
  const WebVector<WebRTCRtpHeaderExtensionParameters>& HeaderExtensions()
      const {
    return header_extensions_;
  }
  const WebRTCRtcpParameters& Rtcp() const { return rtcp_; }
  const WebVector<WebRTCRtpCodecParameters>& Codecs() const { return codecs_; }
  const base::Optional<WebRTCDegradationPreference>& DegradationPreference()
      const {
    return degradation_preference_;
  }

 private:
  base::Optional<WebString> transaction_id_;
  WebRTCRtcpParameters rtcp_;
  WebVector<WebRTCRtpEncodingParameters> encodings_;
  WebVector<WebRTCRtpHeaderExtensionParameters> header_extensions_;
  WebVector<WebRTCRtpCodecParameters> codecs_;
  base::Optional<WebRTCDegradationPreference> degradation_preference_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RTC_RTP_PARAMETERS_H_
