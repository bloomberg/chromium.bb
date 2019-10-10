// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_CERTIFICATE_GENERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_CERTIFICATE_GENERATOR_H_

#include "base/macros.h"
#include "third_party/blink/public/platform/web_rtc_certificate_generator.h"
#include "third_party/blink/public/platform/web_rtc_key_params.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/webrtc/api/peer_connection_interface.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {

// Chromium's WebRTCCertificateGenerator implementation; uses the
// PeerConnectionIdentityStore/SSLIdentity::Generate to generate the identity,
// rtc::RTCCertificate and blink::RTCCertificate.
class MODULES_EXPORT RTCCertificateGenerator
    : public blink::WebRTCCertificateGenerator {
 public:
  RTCCertificateGenerator() {}
  ~RTCCertificateGenerator() override {}

  // blink::WebRTCCertificateGenerator implementation.
  void GenerateCertificate(
      const blink::WebRTCKeyParams& key_params,
      blink::WebRTCCertificateCallback completion_callback,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  void GenerateCertificateWithExpiration(
      const blink::WebRTCKeyParams& key_params,
      uint64_t expires_ms,
      blink::WebRTCCertificateCallback completion_callback,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  bool IsSupportedKeyParams(const blink::WebRTCKeyParams& key_params) override;
  rtc::scoped_refptr<rtc::RTCCertificate> FromPEM(
      blink::WebString pem_private_key,
      blink::WebString pem_certificate) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(RTCCertificateGenerator);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_CERTIFICATE_GENERATOR_H_
