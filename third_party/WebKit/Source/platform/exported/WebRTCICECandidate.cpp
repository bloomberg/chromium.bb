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

#include "public/platform/WebRTCICECandidate.h"

#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefPtr.h"
#include "public/platform/WebString.h"

namespace blink {

class WebRTCICECandidatePrivate final
    : public RefCounted<WebRTCICECandidatePrivate> {
 public:
  static RefPtr<WebRTCICECandidatePrivate> Create(
      const WebString& candidate,
      const WebString& sdp_mid,
      unsigned short sdp_m_line_index) {
    return AdoptRef(
        new WebRTCICECandidatePrivate(candidate, sdp_mid, sdp_m_line_index));
  }

  const WebString& Candidate() const { return candidate_; }
  const WebString& SdpMid() const { return sdp_mid_; }
  unsigned short SdpMLineIndex() const { return sdp_m_line_index_; }

  BLINK_PLATFORM_EXPORT void SetCandidate(WebString candidate) {
    candidate_ = candidate;
  }
  BLINK_PLATFORM_EXPORT void SetSdpMid(WebString sdp_mid) {
    sdp_mid_ = sdp_mid;
  }
  BLINK_PLATFORM_EXPORT void SetSdpMLineIndex(unsigned short sdp_m_line_index) {
    sdp_m_line_index_ = sdp_m_line_index;
  }

 private:
  WebRTCICECandidatePrivate(const WebString& candidate,
                            const WebString& sdp_mid,
                            unsigned short sdp_m_line_index);

  WebString candidate_;
  WebString sdp_mid_;
  unsigned short sdp_m_line_index_;
};

WebRTCICECandidatePrivate::WebRTCICECandidatePrivate(
    const WebString& candidate,
    const WebString& sdp_mid,
    unsigned short sdp_m_line_index)
    : candidate_(candidate),
      sdp_mid_(sdp_mid),
      sdp_m_line_index_(sdp_m_line_index) {}

void WebRTCICECandidate::Assign(const WebRTCICECandidate& other) {
  private_ = other.private_;
}

void WebRTCICECandidate::Reset() {
  private_.Reset();
}

void WebRTCICECandidate::Initialize(const WebString& candidate,
                                    const WebString& sdp_mid,
                                    unsigned short sdp_m_line_index) {
  private_ =
      WebRTCICECandidatePrivate::Create(candidate, sdp_mid, sdp_m_line_index);
}

WebString WebRTCICECandidate::Candidate() const {
  DCHECK(!private_.IsNull());
  return private_->Candidate();
}

WebString WebRTCICECandidate::SdpMid() const {
  DCHECK(!private_.IsNull());
  return private_->SdpMid();
}

unsigned short WebRTCICECandidate::SdpMLineIndex() const {
  DCHECK(!private_.IsNull());
  return private_->SdpMLineIndex();
}

void WebRTCICECandidate::SetCandidate(WebString candidate) {
  DCHECK(!private_.IsNull());
  private_->SetCandidate(candidate);
}

void WebRTCICECandidate::SetSdpMid(WebString sdp_mid) {
  DCHECK(!private_.IsNull());
  private_->SetSdpMid(sdp_mid);
}

void WebRTCICECandidate::SetSdpMLineIndex(unsigned short sdp_m_line_index) {
  DCHECK(!private_.IsNull());
  private_->SetSdpMLineIndex(sdp_m_line_index);
}

}  // namespace blink
