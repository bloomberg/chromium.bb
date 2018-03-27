// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebRTCStatsReportCallbackResolver_h
#define WebRTCStatsReportCallbackResolver_h

#include <memory>

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/peerconnection/RTCStatsReport.h"
#include "public/platform/WebRTCStats.h"

namespace blink {

class WebRTCStatsReportCallbackResolver : public WebRTCStatsReportCallback {
 public:
  // Takes ownership of |resolver|.
  static std::unique_ptr<WebRTCStatsReportCallback> Create(
      ScriptPromiseResolver*);
  ~WebRTCStatsReportCallbackResolver() override;

 private:
  explicit WebRTCStatsReportCallbackResolver(ScriptPromiseResolver*);

  void OnStatsDelivered(std::unique_ptr<WebRTCStatsReport>) override;

  Persistent<ScriptPromiseResolver> resolver_;
};

}  // namespace blink

#endif  // WebRTCStatsReportCallbackResolver_h
