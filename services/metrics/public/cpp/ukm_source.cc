// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/public/cpp/ukm_source.h"

#include <utility>

#include "base/atomicops.h"
#include "base/hash.h"
#include "base/logging.h"
#include "third_party/metrics_proto/ukm/source.pb.h"

namespace ukm {

namespace {

// The maximum length of a URL we will record.
constexpr int kMaxURLLength = 2 * 1024;

// The string sent in place of a URL if the real URL was too long.
constexpr char kMaxUrlLengthMessage[] = "URLTooLong";

// Using a simple global assumes that all access to it will be done on the same
// thread, namely the UI thread. If this becomes not the case then it can be
// changed to an Atomic32 (make CustomTabState derive from int32_t) and accessed
// with no-barrier loads and stores.
UkmSource::CustomTabState g_custom_tab_state = UkmSource::kCustomTabUnset;

// Returns a URL that is under the length limit, by returning a constant
// string when the URl is too long.
std::string GetShortenedURL(const GURL& url) {
  if (url.spec().length() > kMaxURLLength)
    return kMaxUrlLengthMessage;
  return url.spec();
}

}  // namespace

// static
void UkmSource::SetCustomTabVisible(bool visible) {
  g_custom_tab_state = visible ? kCustomTabTrue : kCustomTabFalse;
}

UkmSource::NavigationData::NavigationData() = default;
UkmSource::NavigationData::~NavigationData() = default;

UkmSource::NavigationData::NavigationData(const NavigationData& other) =
    default;

UkmSource::NavigationData UkmSource::NavigationData::CopyWithSanitizedUrls(
    std::vector<GURL> sanitized_urls) const {
  DCHECK_LE(sanitized_urls.size(), 2u);
  DCHECK(!sanitized_urls.empty());
  DCHECK(!sanitized_urls.back().is_empty());
  DCHECK(!sanitized_urls.front().is_empty());

  NavigationData sanitized_navigation_data;
  sanitized_navigation_data.urls = std::move(sanitized_urls);
  sanitized_navigation_data.previous_source_id = previous_source_id;
  sanitized_navigation_data.opener_source_id = opener_source_id;
  sanitized_navigation_data.tab_id = tab_id;
  return sanitized_navigation_data;
}

UkmSource::UkmSource(ukm::SourceId id, const GURL& url)
    : id_(id),
      custom_tab_state_(g_custom_tab_state),
      creation_time_(base::TimeTicks::Now()) {
  navigation_data_.urls = {url};
  DCHECK(!url.is_empty());
}

UkmSource::UkmSource(ukm::SourceId id, const NavigationData& navigation_data)
    : id_(id),
      navigation_data_(navigation_data),
      custom_tab_state_(g_custom_tab_state),
      creation_time_(base::TimeTicks::Now()) {
  DCHECK(GetSourceIdType(id_) == SourceIdType::NAVIGATION_ID);
  DCHECK(!navigation_data.urls.empty());
  DCHECK(!navigation_data.urls.back().is_empty());
}

UkmSource::~UkmSource() = default;

void UkmSource::UpdateUrl(const GURL& new_url) {
  DCHECK(!new_url.is_empty());
  DCHECK_EQ(1u, navigation_data_.urls.size());
  if (url() == new_url)
    return;
  navigation_data_.urls = {new_url};
}

void UkmSource::PopulateProto(Source* proto_source) const {
  DCHECK(!proto_source->has_id());
  DCHECK(!proto_source->has_url());
  DCHECK(!proto_source->has_initial_url());

  proto_source->set_id(id_);
  proto_source->set_url(GetShortenedURL(url()));
  if (urls().size() > 1u) {
    DCHECK_EQ(SourceIdType::NAVIGATION_ID, GetSourceIdType(id_));
    const GURL& initial_url = urls().front();
    proto_source->set_initial_url(GetShortenedURL(initial_url));
  }

  if (custom_tab_state_ != kCustomTabUnset)
    proto_source->set_is_custom_tab(custom_tab_state_ == kCustomTabTrue);

  if (navigation_data_.previous_source_id != kInvalidSourceId)
    proto_source->set_previous_source_id(navigation_data_.previous_source_id);

  if (navigation_data_.opener_source_id != kInvalidSourceId)
    proto_source->set_opener_source_id(navigation_data_.opener_source_id);

  // Tab ids will always be greater than 0. See CreateUniqueTabId in
  // source_url_recorder.cc
  if (navigation_data_.tab_id != 0)
    proto_source->set_tab_id(navigation_data_.tab_id);
}

}  // namespace ukm
