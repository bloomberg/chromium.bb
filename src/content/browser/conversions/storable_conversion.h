// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_STORABLE_CONVERSION_H_
#define CONTENT_BROWSER_CONVERSIONS_STORABLE_CONVERSION_H_

#include <string>

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

// Struct which represents a conversion registration event that was observed in
// the renderer and is now being used by the browser process.
class CONTENT_EXPORT StorableConversion {
 public:
  // Should only be created with values that the browser process has already
  // validated. At creation time, |conversion_data_| should already be stripped
  // to a lower entropy. |conversion_origin| should be filled by a navigation
  // origin known by the browser process.
  StorableConversion(const std::string& conversion_data,
                     const url::Origin& conversion_origin,
                     const url::Origin& reporting_origin);
  StorableConversion(const StorableConversion& other);
  StorableConversion& operator=(const StorableConversion& other) = delete;
  ~StorableConversion();

  const std::string& conversion_data() const { return conversion_data_; }

  const url::Origin& conversion_origin() const { return conversion_origin_; }

  const url::Origin& reporting_origin() const { return reporting_origin_; }

 private:
  // Conversion data associated with conversion registration event. String
  // representing a valid hexadecimal number.
  std::string conversion_data_;

  // Origin this conversion event occurred on.
  url::Origin conversion_origin_;

  // Origin of the conversion redirect url, and the origin that will receive any
  // reports.
  url::Origin reporting_origin_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_STORABLE_CONVERSION_H_
