// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_IMPRESSION_H_
#define CONTENT_PUBLIC_COMMON_IMPRESSION_H_

#include <stdint.h>
#include <string>

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

// An impression represents a click on an anchor tag that has special Conversion
// Measurement attributes declared. When the anchor is clicked, an impression is
// generated from these attributes and associated with the resulting navigation.
// When an action is performed on the linked site at a later date, the
// impression information is used to provide context about the initial
// navigation that resulted in that action.
//
// Used for IPC transport of WebImpression. WebImpression cannot be used
// directly as it contains non-header-only blink types.
struct CONTENT_EXPORT Impression {
  Impression();
  Impression(const Impression& other);
  Impression& operator=(const Impression& other);
  ~Impression();

  // Intended committed top-level origin of the resulting navigation. Must match
  // the committed navigation's origin to be a valid impression. Declared by
  // the impression tag.
  url::Origin conversion_destination;

  // Optional origin that will receive all conversion measurement reports
  // associated with this impression. Declared by the impression tag.
  base::Optional<url::Origin> reporting_origin;

  // Data that will be sent in conversion reports to identify this impression.
  // Declared by the impression tag.
  uint64_t impression_data;

  // Optional expiry specifying the amount of time this impression can convert.
  // Declared by the impression tag.
  base::Optional<base::TimeDelta> expiry;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_IMPRESSION_H_
