// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/wire_response_translator.h"

namespace feed {

RefreshResponseData WireResponseTranslator::TranslateWireResponse(
    feedwire::Response response,
    StreamModelUpdateRequest::Source source,
    bool was_signed_in_request,
    base::Time current_time) const {
  return ::feed::TranslateWireResponse(std::move(response), source,
                                       was_signed_in_request, current_time);
}

}  // namespace feed
