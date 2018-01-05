/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebRTCConfiguration_h
#define WebRTCConfiguration_h

#include "WebCommon.h"
#include "WebRTCCertificate.h"
#include "WebURL.h"
#include "WebVector.h"

#include <memory>

namespace blink {

class WebString;

struct WebRTCIceServer {
  WebURL url;
  WebString username;
  WebString credential;
};

enum class WebRTCIceTransportPolicy { kRelay, kAll };

enum class WebRTCBundlePolicy { kBalanced, kMaxCompat, kMaxBundle };

enum class WebRTCRtcpMuxPolicy { kNegotiate, kRequire };

enum class WebRTCSdpSemantics { kDefault, kPlanB, kUnifiedPlan };

struct WebRTCConfiguration {
  WebVector<WebRTCIceServer> ice_servers;
  WebRTCIceTransportPolicy ice_transport_policy =
      WebRTCIceTransportPolicy::kAll;
  WebRTCBundlePolicy bundle_policy = WebRTCBundlePolicy::kBalanced;
  WebRTCRtcpMuxPolicy rtcp_mux_policy = WebRTCRtcpMuxPolicy::kRequire;
  WebVector<std::unique_ptr<WebRTCCertificate>> certificates;
  int ice_candidate_pool_size = 0;
  WebRTCSdpSemantics sdp_semantics = WebRTCSdpSemantics::kDefault;
};

}  // namespace blink

#endif  // WebRTCConfiguration_h
