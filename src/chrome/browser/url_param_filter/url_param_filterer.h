// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_URL_PARAM_FILTER_URL_PARAM_FILTERER_H_
#define CHROME_BROWSER_URL_PARAM_FILTER_URL_PARAM_FILTERER_H_

#include <unordered_map>
#include "chrome/browser/url_param_filter/url_param_filter_classification.pb.h"
#include "url/gurl.h"

namespace url_param_filter {
using ClassificationMap =
    std::unordered_map<std::string, url_param_filter::FilterClassification>;

// Represents the result of filtering; includes the resulting URL (which may be
// unmodified), along with the count of params filtered.
struct FilterResult {
  GURL filtered_url;
  int filtered_param_count;
};

// Filter the destination URL according to the parameter classifications for the
// source and destination URLs. Used internally by the 2-arg overload, and
// called directly from tests. Defers to caller for any metric writing.
// Currently experimental; not intended for broad consumption.
FilterResult FilterUrl(const GURL& source_url,
                       const GURL& destination_url,
                       const ClassificationMap& source_classification_map,
                       const ClassificationMap& destination_classification_map);

// Filter the destination URL according to the default parameter classifications
// for the source and destination URLs. Owns its own metric writing.
// Currently experimental; not intended for broad consumption.
GURL FilterUrl(const GURL& source_url, const GURL& destination_url);

}  // namespace url_param_filter
#endif  // CHROME_BROWSER_URL_PARAM_FILTER_URL_PARAM_FILTERER_H_
