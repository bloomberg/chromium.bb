// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_tracker.h"

#include "base/string_util.h"
#include "net/url_request/url_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(URLRequestTrackerTest, Basic) {
  URLRequestTracker tracker;
  EXPECT_EQ(0u, tracker.GetLiveRequests().size());
  EXPECT_EQ(0u, tracker.GetRecentlyDeceased().size());

  URLRequest req1(GURL("http://req1"), NULL);
  URLRequest req2(GURL("http://req2"), NULL);
  URLRequest req3(GURL("http://req3"), NULL);
  URLRequest req4(GURL("http://req4"), NULL);
  URLRequest req5(GURL("http://req5"), NULL);

  tracker.Add(&req1);
  tracker.Add(&req2);
  tracker.Add(&req3);
  tracker.Add(&req4);
  tracker.Add(&req5);

  std::vector<URLRequest*> live_reqs = tracker.GetLiveRequests();

  ASSERT_EQ(5u, live_reqs.size());
  EXPECT_EQ(GURL("http://req1"), live_reqs[0]->original_url());
  EXPECT_EQ(GURL("http://req2"), live_reqs[1]->original_url());
  EXPECT_EQ(GURL("http://req3"), live_reqs[2]->original_url());
  EXPECT_EQ(GURL("http://req4"), live_reqs[3]->original_url());
  EXPECT_EQ(GURL("http://req5"), live_reqs[4]->original_url());

  tracker.Remove(&req1);
  tracker.Remove(&req5);
  tracker.Remove(&req3);

  ASSERT_EQ(3u, tracker.GetRecentlyDeceased().size());

  live_reqs = tracker.GetLiveRequests();

  ASSERT_EQ(2u, live_reqs.size());
  EXPECT_EQ(GURL("http://req2"), live_reqs[0]->original_url());
  EXPECT_EQ(GURL("http://req4"), live_reqs[1]->original_url());
}

TEST(URLRequestTrackerTest, GraveyardBounded) {
  URLRequestTracker tracker;
  EXPECT_EQ(0u, tracker.GetLiveRequests().size());
  EXPECT_EQ(0u, tracker.GetRecentlyDeceased().size());

  // Add twice as many requests as will fit in the graveyard.
  for (size_t i = 0; i < URLRequestTracker::kMaxGraveyardSize * 2; ++i) {
    URLRequest req(GURL(StringPrintf("http://req%d", i).c_str()), NULL);
    tracker.Add(&req);
    tracker.Remove(&req);
  }

  // Check that only the last |kMaxGraveyardSize| requests are in-memory.

  URLRequestTracker::RecentRequestInfoList recent_reqs =
      tracker.GetRecentlyDeceased();

  ASSERT_EQ(URLRequestTracker::kMaxGraveyardSize, recent_reqs.size());

  for (size_t i = 0; i < URLRequestTracker::kMaxGraveyardSize; ++i) {
    size_t req_number = i + URLRequestTracker::kMaxGraveyardSize;
    GURL url(StringPrintf("http://req%d", req_number).c_str());
    EXPECT_EQ(url, recent_reqs[i].original_url);
  }
}

// Check that very long URLs are truncated.
TEST(URLRequestTrackerTest, GraveyardURLBounded) {
  URLRequestTracker tracker;

  std::string big_url_spec("http://");
  big_url_spec.resize(2 * URLRequestTracker::kMaxGraveyardURLSize, 'x');
  GURL big_url(big_url_spec);
  URLRequest req(big_url, NULL);

  tracker.Add(&req);
  tracker.Remove(&req);

  ASSERT_EQ(1u, tracker.GetRecentlyDeceased().size());
  // The +1 is because GURL canonicalizes with a trailing '/' ... maybe
  // we should just save the std::string rather than the GURL.
  EXPECT_EQ(URLRequestTracker::kMaxGraveyardURLSize + 1,
            tracker.GetRecentlyDeceased()[0].original_url.spec().size());
}

// Test the doesn't fail if the URL was invalid. http://crbug.com/21423.
TEST(URLRequestTrackerTest, TrackingInvalidURL) {
  URLRequestTracker tracker;

  EXPECT_EQ(0u, tracker.GetLiveRequests().size());
  EXPECT_EQ(0u, tracker.GetRecentlyDeceased().size());

  GURL invalid_url("xabc");
  EXPECT_FALSE(invalid_url.is_valid());
  URLRequest req(invalid_url, NULL);

  tracker.Add(&req);
  tracker.Remove(&req);

  // Check that the invalid URL made it into graveyard.
  ASSERT_EQ(1u, tracker.GetRecentlyDeceased().size());
  EXPECT_FALSE(tracker.GetRecentlyDeceased()[0].original_url.is_valid());
}

}  // namespace
