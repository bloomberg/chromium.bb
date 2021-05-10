// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/impression_conversions.h"

#include <algorithm>
#include <iterator>

#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"

namespace blink {

blink::Impression ConvertWebImpressionToImpression(
    const blink::WebImpression& web_impression) {
  blink::Impression result;

  result.impression_data = web_impression.impression_data;
  result.expiry = web_impression.expiry;
  result.reporting_origin = web_impression.reporting_origin;
  result.conversion_destination = web_impression.conversion_destination;

  return result;
}

}  // namespace blink
