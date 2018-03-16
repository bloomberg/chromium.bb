// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebRTCAPIName_h
#define WebRTCAPIName_h

namespace blink {

// Helper enum used for histogramming calls to WebRTC APIs from JavaScript.
// The entries are linked to UMA values in //tools/metrics/histograms/enums.xml
// and shouldn't be renumbered or removed.
enum class WebRTCAPIName {
  kGetUserMedia,
  kPeerConnection,
  kDeprecatedPeerConnection,
  kRTCPeerConnection,
  kEnumerateDevices,
  kMediaStreamRecorder,
  kCanvasCaptureStream,
  kVideoCaptureStream,
  kInvalidName
};

}  // namespace blink

#endif  // WebRTCAPIName_h
