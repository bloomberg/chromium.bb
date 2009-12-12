// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/request_tracker.h"

#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

static const int kMaxNumLoadLogEntries = 1;

class TestRequest {
 public:
  explicit TestRequest(const GURL& url)
      : url_(url),
        load_log_(new net::LoadLog(kMaxNumLoadLogEntries)),
        ALLOW_THIS_IN_INITIALIZER_LIST(request_tracker_node_(this)) {}
  ~TestRequest() {}

  // This method is used in RequestTrackerTest::Basic test.
  const GURL& original_url() const { return url_; }

 private:
  // RequestTracker<T> will access GetRecentRequestInfo() and
  // |request_tracker_node_|.
  friend class RequestTracker<TestRequest>;

  void GetInfoForTracker(
      RequestTracker<TestRequest>::RecentRequestInfo* info) const {
    info->original_url = url_;
    info->load_log = load_log_;
  }

  const GURL url_;
  scoped_refptr<net::LoadLog> load_log_;

  RequestTracker<TestRequest>::Node request_tracker_node_;

  DISALLOW_COPY_AND_ASSIGN(TestRequest);
};


TEST(RequestTrackerTest, Basic) {
  RequestTracker<TestRequest> tracker;
  EXPECT_EQ(0u, tracker.GetLiveRequests().size());
  EXPECT_EQ(0u, tracker.GetRecentlyDeceased().size());

  TestRequest req1(GURL("http://req1"));
  TestRequest req2(GURL("http://req2"));
  TestRequest req3(GURL("http://req3"));
  TestRequest req4(GURL("http://req4"));
  TestRequest req5(GURL("http://req5"));

  tracker.Add(&req1);
  tracker.Add(&req2);
  tracker.Add(&req3);
  tracker.Add(&req4);
  tracker.Add(&req5);

  std::vector<TestRequest*> live_reqs = tracker.GetLiveRequests();

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

TEST(RequestTrackerTest, GraveyardBounded) {
  RequestTracker<TestRequest> tracker;
  EXPECT_EQ(0u, tracker.GetLiveRequests().size());
  EXPECT_EQ(0u, tracker.GetRecentlyDeceased().size());

  // Add twice as many requests as will fit in the graveyard.
  for (size_t i = 0;
       i < RequestTracker<TestRequest>::kMaxGraveyardSize * 2;
       ++i) {
    TestRequest req(GURL(StringPrintf("http://req%" PRIuS, i).c_str()));
    tracker.Add(&req);
    tracker.Remove(&req);
  }

  // Check that only the last |kMaxGraveyardSize| requests are in-memory.

  RequestTracker<TestRequest>::RecentRequestInfoList recent_reqs =
      tracker.GetRecentlyDeceased();

  ASSERT_EQ(RequestTracker<TestRequest>::kMaxGraveyardSize, recent_reqs.size());

  for (size_t i = 0; i < RequestTracker<TestRequest>::kMaxGraveyardSize; ++i) {
    size_t req_number = i + RequestTracker<TestRequest>::kMaxGraveyardSize;
    GURL url(StringPrintf("http://req%" PRIuS, req_number).c_str());
    EXPECT_EQ(url, recent_reqs[i].original_url);
  }
}

// Check that very long URLs are truncated.
TEST(RequestTrackerTest, GraveyardURLBounded) {
  RequestTracker<TestRequest> tracker;

  std::string big_url_spec("http://");
  big_url_spec.resize(2 * RequestTracker<TestRequest>::kMaxGraveyardURLSize,
                      'x');
  GURL big_url(big_url_spec);
  TestRequest req(big_url);

  tracker.Add(&req);
  tracker.Remove(&req);

  ASSERT_EQ(1u, tracker.GetRecentlyDeceased().size());
  // The +1 is because GURL canonicalizes with a trailing '/' ... maybe
  // we should just save the std::string rather than the GURL.
  EXPECT_EQ(RequestTracker<TestRequest>::kMaxGraveyardURLSize + 1,
            tracker.GetRecentlyDeceased()[0].original_url.spec().size());
}

// Test the doesn't fail if the URL was invalid. http://crbug.com/21423.
TEST(URLRequestTrackerTest, TrackingInvalidURL) {
  RequestTracker<TestRequest> tracker;

  EXPECT_EQ(0u, tracker.GetLiveRequests().size());
  EXPECT_EQ(0u, tracker.GetRecentlyDeceased().size());

  GURL invalid_url("xabc");
  EXPECT_FALSE(invalid_url.is_valid());
  TestRequest req(invalid_url);

  tracker.Add(&req);
  tracker.Remove(&req);

  // Check that the invalid URL made it into graveyard.
  ASSERT_EQ(1u, tracker.GetRecentlyDeceased().size());
  EXPECT_FALSE(tracker.GetRecentlyDeceased()[0].original_url.is_valid());
}

bool ShouldRequestBeAddedToGraveyard(const GURL& url) {
  return !url.SchemeIs("chrome") && !url.SchemeIs("data");
}

// Check that we can exclude "chrome://" URLs and "data:" URLs from being
// saved into the recent requests list (graveyard), by using a filter.
TEST(RequestTrackerTest, GraveyardCanBeFiltered) {
  RequestTracker<TestRequest> tracker;

  tracker.SetGraveyardFilter(ShouldRequestBeAddedToGraveyard);

  // This will be excluded.
  TestRequest req1(GURL("chrome://dontcare"));
  tracker.Add(&req1);
  tracker.Remove(&req1);

  // This will be be added to graveyard.
  TestRequest req2(GURL("chrome2://dontcare"));
  tracker.Add(&req2);
  tracker.Remove(&req2);

  // This will be be added to graveyard.
  TestRequest req3(GURL("http://foo"));
  tracker.Add(&req3);
  tracker.Remove(&req3);

  // This will be be excluded.
  TestRequest req4(GURL("data:sup"));
  tracker.Add(&req4);
  tracker.Remove(&req4);

  ASSERT_EQ(2u, tracker.GetRecentlyDeceased().size());
  EXPECT_EQ("chrome2://dontcare/",
            tracker.GetRecentlyDeceased()[0].original_url.spec());
  EXPECT_EQ("http://foo/",
            tracker.GetRecentlyDeceased()[1].original_url.spec());
}

}  // namespace
