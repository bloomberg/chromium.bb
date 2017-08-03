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

#ifndef WebRTCICECandidate_h
#define WebRTCICECandidate_h

#include "WebCommon.h"
#include "WebPrivatePtr.h"
#include "WebString.h"

namespace blink {

class WebString;
class WebRTCICECandidatePrivate;

class WebRTCICECandidate {
 public:
  WebRTCICECandidate() {}
  WebRTCICECandidate(const WebRTCICECandidate& other) { Assign(other); }
  ~WebRTCICECandidate() { Reset(); }

  WebRTCICECandidate& operator=(const WebRTCICECandidate& other) {
    Assign(other);
    return *this;
  }

  BLINK_PLATFORM_EXPORT void Assign(const WebRTCICECandidate&);

  BLINK_PLATFORM_EXPORT void Initialize(const WebString& candidate,
                                        const WebString& sdp_mid,
                                        unsigned short sdp_m_line_index);
  BLINK_PLATFORM_EXPORT void Reset();
  bool IsNull() const { return private_.IsNull(); }

  BLINK_PLATFORM_EXPORT WebString Candidate() const;
  BLINK_PLATFORM_EXPORT WebString SdpMid() const;
  BLINK_PLATFORM_EXPORT unsigned short SdpMLineIndex() const;
  BLINK_PLATFORM_EXPORT void SetCandidate(WebString);
  BLINK_PLATFORM_EXPORT void SetSdpMid(WebString);
  BLINK_PLATFORM_EXPORT void SetSdpMLineIndex(unsigned short);

#if INSIDE_BLINK
  // TODO(guidou): Support setting sdp_m_line_index to -1 to indicate the
  // absence of a value for sdp_m_line_index. crbug.com/614958
  WebRTCICECandidate(WebString candidate,
                     WebString sdp_mid,
                     unsigned short sdp_m_line_index) {
    this->Initialize(candidate, sdp_mid, sdp_m_line_index);
  }
#endif

 private:
  WebPrivatePtr<WebRTCICECandidatePrivate> private_;
};

}  // namespace blink

#endif  // WebRTCICECandidate_h
