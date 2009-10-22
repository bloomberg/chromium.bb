// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_TRACKER_H_
#define NET_URL_REQUEST_URL_REQUEST_TRACKER_H_

#include <vector>

#include "base/ref_counted.h"
#include "base/linked_list.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_log.h"

class URLRequest;

// Class to track all of the live instances of URLRequest associated with a
// particular URLRequestContext.  It keep a circular queue of the LoadLogs
// for recently deceased requests.
class URLRequestTracker {
 public:
  struct RecentRequestInfo {
    GURL original_url;
    scoped_refptr<net::LoadLog> load_log;
  };

  // Helper class to make URLRequest insertable into a base::LinkedList,
  // without making the public interface expose base::LinkNode.
  class Node : public base::LinkNode<Node> {
   public:
    Node(URLRequest* url_request) : url_request_(url_request) {}
    ~Node() {}

    URLRequest* url_request() const { return url_request_; }

   private:
    URLRequest* url_request_;
  };

  typedef std::vector<RecentRequestInfo> RecentRequestInfoList;

  // The maximum number of entries for |graveyard_|.
  static const size_t kMaxGraveyardSize;

  // The maximum size of URLs to stuff into RecentRequestInfo.
  static const size_t kMaxGraveyardURLSize;

  URLRequestTracker();
  ~URLRequestTracker();

  // Returns a list of URLRequests that are alive.
  std::vector<URLRequest*> GetLiveRequests();

  // Clears the circular buffer of RecentRequestInfos.
  void ClearRecentlyDeceased();

  // Returns a list of recently completed URLRequests.
  const RecentRequestInfoList GetRecentlyDeceased();

  void Add(URLRequest* url_request);
  void Remove(URLRequest* url_request);

 private:
  // Copy the goodies out of |url_request| that we want to show the
  // user later on the about:net-internal page.
  static const RecentRequestInfo ExtractInfo(URLRequest* url_request);

  void InsertIntoGraveyard(const RecentRequestInfo& info);

  base::LinkedList<Node> live_instances_;

  size_t next_graveyard_index_;
  RecentRequestInfoList graveyard_;
};

#endif  // NET_URL_REQUEST_URL_REQUEST_TRACKER_H_
