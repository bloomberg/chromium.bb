// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"

#include <memory>

namespace blink {

void ResourceTimingInfo::AddRedirect(const ResourceResponse& redirect_response,
                                     bool cross_origin) {
  redirect_chain_.push_back(redirect_response);
  if (has_cross_origin_redirect_)
    return;
  if (cross_origin) {
    has_cross_origin_redirect_ = true;
    transfer_size_ = 0;
  } else {
    DCHECK_GE(redirect_response.EncodedDataLength(), 0);
    transfer_size_ += redirect_response.EncodedDataLength();
  }
}

}  // namespace blink
