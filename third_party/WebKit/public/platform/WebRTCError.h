// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebRTCError_h
#define WebRTCError_h

// This header provides a Blink equivalent of webrtc::RTCError from
// third_party/webrtc. It's used to communicate errors between Blink and the
// WebRTCPeerConnectionHandler implementation.

#include "WebCommon.h"

namespace blink {

enum class WebRTCErrorType {
  kNone,
  kUnsupportedParameter,
  kInvalidParameter,
  kInvalidRange,
  kSyntaxError,
  kInvalidState,
  kInvalidModification,
  kNetworkError,
  kInternalError,
};

class WebRTCError {
 public:
  WebRTCError() : m_type(WebRTCErrorType::kNone) {}
  explicit WebRTCError(WebRTCErrorType type) : m_type(type) {}

  WebRTCErrorType type() const { return m_type; }
  void setType(WebRTCErrorType type) { m_type = type; }

 private:
  WebRTCErrorType m_type;
};

}  // namespace blink

#endif  // WebRTCError_h
