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
  WebRTCError() : type_(WebRTCErrorType::kNone) {}
  explicit WebRTCError(WebRTCErrorType type) : type_(type) {}

  WebRTCErrorType GetType() const { return type_; }
  void SetType(WebRTCErrorType type) { type_ = type; }

 private:
  WebRTCErrorType type_;
};

}  // namespace blink

#endif  // WebRTCError_h
