// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_BROWSER_SRC_CLIENT_MAC_H_
#define CONTENT_ANALYSIS_BROWSER_SRC_CLIENT_MAC_H_

#include "client_base.h"

namespace content_analysis {
namespace sdk {

// Client implementaton for Mac.
class ClientMac : public ClientBase {
 public:
  ClientMac(Config config);

  // Client:
  int Send(const ContentAnalysisRequest& request,
                 ContentAnalysisResponse* response) override;
};

}  // namespace sdk
}  // namespace content_analysis

#endif  // CONTENT_ANALYSIS_BROWSER_SRC_CLIENT_MAC_H_