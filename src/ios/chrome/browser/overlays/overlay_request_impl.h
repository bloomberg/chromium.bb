// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_REQUEST_IMPL_H_
#define IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_REQUEST_IMPL_H_

#include <memory>

#include "ios/chrome/browser/overlays/overlay_request.h"

// Internal implementation of OverlayRequest.
class OverlayRequestImpl : public OverlayRequest,
                           public base::SupportsUserData {
 public:
  OverlayRequestImpl();
  ~OverlayRequestImpl() override;

 private:
  // OverlayRequest:
  void set_response(std::unique_ptr<OverlayResponse> response) override;
  OverlayResponse* response() const override;
  base::SupportsUserData& data() override;

  // The response containing the user interaction information for the overlay
  // resulting from this response.
  std::unique_ptr<OverlayResponse> response_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_REQUEST_IMPL_H_
