// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PROTO_UTIL_H_
#define COMPONENTS_FEED_CORE_V2_PROTO_UTIL_H_

#include <string>

#include "base/time/time.h"
#include "components/feed/core/proto/v2/wire/client_info.pb.h"
#include "components/feed/core/proto/v2/wire/content_id.pb.h"
#include "components/feed/core/proto/v2/wire/feed_query.pb.h"
#include "components/feed/core/v2/types.h"

namespace feedwire {
class Request;
}  // namespace feedwire
namespace feedstore {
class StreamData;
}  // namespace feedstore

// Helper functions/classes for dealing with feed proto messages.

namespace feed {
using ContentId = feedwire::ContentId;

std::string ContentIdString(const feedwire::ContentId&);
bool Equal(const feedwire::ContentId& a, const feedwire::ContentId& b);
bool CompareContentId(const feedwire::ContentId& a,
                      const feedwire::ContentId& b);

class ContentIdCompareFunctor {
 public:
  bool operator()(const feedwire::ContentId& a,
                  const feedwire::ContentId& b) const {
    return CompareContentId(a, b);
  }
};

feedwire::ClientInfo CreateClientInfo(const RequestMetadata& request_metadata);

feedwire::Request CreateFeedQueryRefreshRequest(
    feedwire::FeedQuery::RequestReason request_reason,
    const RequestMetadata& request_metadata,
    const std::string& consistency_token);

feedwire::Request CreateFeedQueryLoadMoreRequest(
    const RequestMetadata& request_metadata,
    const std::string& consistency_token,
    const std::string& next_page_token);

}  // namespace feed

namespace feedstore {

void SetLastAddedTime(base::Time t, feedstore::StreamData* data);
base::Time GetLastAddedTime(const feedstore::StreamData& data);

}  // namespace feedstore

#endif  // COMPONENTS_FEED_CORE_V2_PROTO_UTIL_H_
