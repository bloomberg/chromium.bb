// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_tracker.h"

#include "base/logging.h"
#include "net/url_request/url_request.h"

const size_t URLRequestTracker::kMaxGraveyardSize = 25;
const size_t URLRequestTracker::kMaxGraveyardURLSize = 1000;

URLRequestTracker::URLRequestTracker() : next_graveyard_index_(0) {}

URLRequestTracker::~URLRequestTracker() {}

std::vector<URLRequest*> URLRequestTracker::GetLiveRequests() {
  std::vector<URLRequest*> list;
  for (base::LinkNode<Node>* node = live_instances_.head();
       node != live_instances_.end();
       node = node->next()) {
    URLRequest* url_request = node->value()->url_request();
    list.push_back(url_request);
  }
  return list;
}

void URLRequestTracker::ClearRecentlyDeceased() {
  next_graveyard_index_ = 0;
  graveyard_.clear();
}

const URLRequestTracker::RecentRequestInfoList
URLRequestTracker::GetRecentlyDeceased() {
  RecentRequestInfoList list;

  // Copy the items from |graveyard_| (our circular queue of recently
  // deceased request infos) into a vector, ordered from oldest to
  // newest.
  for (size_t i = 0; i < graveyard_.size(); ++i) {
    size_t index = (next_graveyard_index_ + i) % graveyard_.size();
    list.push_back(graveyard_[index]);
  }
  return list;
}

void URLRequestTracker::Add(URLRequest* url_request) {
  live_instances_.Append(&url_request->url_request_tracker_node_);
}

void URLRequestTracker::Remove(URLRequest* url_request) {
  // Remove from |live_instances_|.
  url_request->url_request_tracker_node_.RemoveFromList();

  // Add into |graveyard_|.
  InsertIntoGraveyard(ExtractInfo(url_request));
}

// static
const URLRequestTracker::RecentRequestInfo
URLRequestTracker::ExtractInfo(URLRequest* url_request) {
  RecentRequestInfo info;
  info.original_url = url_request->original_url();
  info.load_log = url_request->load_log();

  // Paranoia check: truncate |info.original_url| if it is really big.
  const std::string& spec = info.original_url.possibly_invalid_spec();
  if (spec.size() > kMaxGraveyardURLSize)
    info.original_url = GURL(spec.substr(0, kMaxGraveyardURLSize));
  return info;
}

void URLRequestTracker::InsertIntoGraveyard(
    const RecentRequestInfo& info) {
  if (graveyard_.size() < kMaxGraveyardSize) {
    // Still growing to maximum capacity.
    DCHECK_EQ(next_graveyard_index_, graveyard_.size());
    graveyard_.push_back(info);
  } else {
    // At maximum capacity, overwrite the oldest entry.
    graveyard_[next_graveyard_index_] = info;
  }

  next_graveyard_index_ = (next_graveyard_index_ + 1) % kMaxGraveyardSize;
}
