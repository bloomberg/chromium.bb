// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_REQUEST_TRACKER_H_
#define NET_URL_REQUEST_REQUEST_TRACKER_H_

#include <vector>

#include "base/ref_counted.h"
#include "base/linked_list.h"
#include "base/logging.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_log.h"

// Class to track all of the live instances of Request associated with a
// particular URLRequestContext.  It keeps a circular queue of the LoadLogs
// for recently deceased requests.
template<typename Request>
class RequestTracker {
 public:
  struct RecentRequestInfo {
    GURL original_url;
    scoped_refptr<net::LoadLog> load_log;
  };

  // Helper class to make Request insertable into a base::LinkedList,
  // without making the public interface expose base::LinkNode.
  class Node : public base::LinkNode<Node> {
   public:
    Node(Request* request) : request_(request) {}
    ~Node() {}

    Request* request() const { return request_; }

   private:
    Request* request_;
  };

  typedef std::vector<RecentRequestInfo> RecentRequestInfoList;
  typedef bool (*RecentRequestsFilterFunc)(const GURL&);

  // The maximum number of entries for |graveyard_|.
  static const size_t kMaxGraveyardSize;

  // The maximum size of URLs to stuff into RecentRequestInfo.
  static const size_t kMaxGraveyardURLSize;

  RequestTracker() : next_graveyard_index_(0), graveyard_filter_func_(NULL) {}
  ~RequestTracker() {}

  // Returns a list of Requests that are alive.
  std::vector<Request*> GetLiveRequests() {
    std::vector<Request*> list;
    for (base::LinkNode<Node>* node = live_instances_.head();
         node != live_instances_.end();
         node = node->next()) {
      Request* request = node->value()->request();
      list.push_back(request);
    }
    return list;
  }

  // Clears the circular buffer of RecentRequestInfos.
  void ClearRecentlyDeceased() {
    next_graveyard_index_ = 0;
    graveyard_.clear();
  }

  // Returns a list of recently completed Requests.
  const RecentRequestInfoList GetRecentlyDeceased() {
    RecentRequestInfoList list;

    // Copy the items from |graveyard_| (our circular queue of recently
    // deceased request infos) into a vector, ordered from oldest to newest.
    for (size_t i = 0; i < graveyard_.size(); ++i) {
      size_t index = (next_graveyard_index_ + i) % graveyard_.size();
      list.push_back(graveyard_[index]);
    }
    return list;
  }

  void Add(Request* request) {
    live_instances_.Append(&request->request_tracker_node_);
  }

  void Remove(Request* request) {
    // Remove from |live_instances_|.
    request->request_tracker_node_.RemoveFromList();

    RecentRequestInfo info;
    request->GetInfoForTracker(&info);
    // Paranoia check: truncate |info.original_url| if it is really big.
    const std::string& spec = info.original_url.possibly_invalid_spec();
    if (spec.size() > kMaxGraveyardURLSize)
      info.original_url = GURL(spec.substr(0, kMaxGraveyardURLSize));

    if (ShouldInsertIntoGraveyard(info)) {
      // Add into |graveyard_|.
      InsertIntoGraveyard(info);
    }
  }

  // This function lets you exclude requests from being saved to the graveyard.
  // The graveyard is a circular buffer of the most recently completed
  // requests.  Pass NULL turn off filtering. Otherwise pass in a function
  // returns false to exclude requests, true otherwise.
  void SetGraveyardFilter(RecentRequestsFilterFunc filter_func) {
    graveyard_filter_func_ = filter_func;
  }

 private:
  bool ShouldInsertIntoGraveyard(const RecentRequestInfo& info) {
    if (!graveyard_filter_func_)
      return true;
    return graveyard_filter_func_(info.original_url);
  }

  void InsertIntoGraveyard(const RecentRequestInfo& info) {
    if (graveyard_.size() < kMaxGraveyardSize) {
      // Still growing to maximum capacity.
      DCHECK_EQ(next_graveyard_index_, graveyard_.size());
      graveyard_.push_back(info);
    } else {
      // At maximum capacity, overwite the oldest entry.
      graveyard_[next_graveyard_index_] = info;
    }
    next_graveyard_index_ = (next_graveyard_index_ + 1) % kMaxGraveyardSize;
  }

  base::LinkedList<Node> live_instances_;

  size_t next_graveyard_index_;
  RecentRequestInfoList graveyard_;
  RecentRequestsFilterFunc graveyard_filter_func_;
};

template<typename Request>
const size_t RequestTracker<Request>::kMaxGraveyardSize = 25;

template<typename Request>
const size_t RequestTracker<Request>::kMaxGraveyardURLSize = 1000;

#endif  // NET_URL_REQUEST_REQUEST_TRACKER_H_
