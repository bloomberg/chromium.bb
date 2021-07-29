// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_STORABLE_CONVERSION_H_
#define CONTENT_BROWSER_CONVERSIONS_STORABLE_CONVERSION_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "net/base/schemeful_site.h"
#include "url/origin.h"

namespace content {

// Struct which represents a conversion registration event that was observed in
// the renderer and is now being used by the browser process.
class CONTENT_EXPORT StorableConversion {
 public:
  // Should only be created with values that the browser process has already
  // validated. At creation time, |conversion_data_| should already be stripped
  // to a lower entropy. |conversion_destination| should be filled by a
  // navigation origin known by the browser process.
  StorableConversion(uint64_t conversion_data,
                     net::SchemefulSite conversion_destination,
                     url::Origin reporting_origin,
                     uint64_t event_source_trigger_data,
                     int64_t priority);
  StorableConversion(const StorableConversion& other);
  StorableConversion& operator=(const StorableConversion& other);
  StorableConversion(StorableConversion&& other);
  StorableConversion& operator=(StorableConversion&& other);
  ~StorableConversion();

  uint64_t conversion_data() const WARN_UNUSED_RESULT {
    return conversion_data_;
  }

  const net::SchemefulSite& conversion_destination() const WARN_UNUSED_RESULT {
    return conversion_destination_;
  }

  const url::Origin& reporting_origin() const WARN_UNUSED_RESULT {
    return reporting_origin_;
  }

  uint64_t event_source_trigger_data() const WARN_UNUSED_RESULT {
    return event_source_trigger_data_;
  }

  int64_t priority() const WARN_UNUSED_RESULT { return priority_; }

 private:
  // Conversion data associated with conversion registration event.
  uint64_t conversion_data_;

  // Schemeful site that this conversion event occurred on.
  net::SchemefulSite conversion_destination_;

  // Origin of the conversion redirect url, and the origin that will receive any
  // reports.
  url::Origin reporting_origin_;

  // Event source trigger data specified in conversion redirect. Defaults to 0
  // if not provided.
  uint64_t event_source_trigger_data_;

  // Priority specified in conversion redirect. Used to prioritize which reports
  // to send among multiple different reports for the same attribution source.
  // Defaults to 0 if not provided.
  int64_t priority_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_STORABLE_CONVERSION_H_
