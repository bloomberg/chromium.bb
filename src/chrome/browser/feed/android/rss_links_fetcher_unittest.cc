// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feed/android/rss_links_fetcher.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/feed/core/v2/test/callback_receiver.h"
#include "components/feed/mojom/rss_link_reader.mojom.h"
#include "mojo/core/embedder/embedder.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

GURL TestPageUrl() {
  return GURL("https://page");
}
std::vector<GURL> TestRssUrls() {
  return {GURL("https://somerss1"), GURL("https://somerss2")};
}

class StubRssLinkReader : public mojom::RssLinkReader {
 public:
  StubRssLinkReader() {}
  ~StubRssLinkReader() override {}

  // mojom::RssLinkReader
  void GetRssLinks(GetRssLinksCallback callback) override {
    if (on_call_)
      on_call_.Run();
    callback_ = std::move(callback);
  }

  void WaitForCall() {
    if (!callback_) {
      base::RunLoop run_loop;
      on_call_ = run_loop.QuitClosure();
      run_loop.Run();
    }
  }

  void Respond(mojom::RssLinksPtr result) {
    std::move(callback_).Run(std::move(result));
  }

  std::unique_ptr<mojo::Receiver<mojom::RssLinkReader>> MakeReceiver() {
    return std::make_unique<mojo::Receiver<mojom::RssLinkReader>>(this);
  }

 private:
  GetRssLinksCallback callback_;
  base::RepeatingClosure on_call_;
};

class RssLinksFetcherUnitTest : public ::testing::Test {
 public:
  RssLinksFetcherUnitTest() = default;
  void SetUp() override { mojo::core::Init(); }

 protected:
  base::test::TaskEnvironment task_env_;
};

TEST_F(RssLinksFetcherUnitTest, Success) {
  CallbackReceiver<std::vector<GURL>> rss_links;
  StubRssLinkReader link_reader;
  auto receiver = link_reader.MakeReceiver();
  FetchRssLinks(TestPageUrl(),
                mojo::Remote<feed::mojom::RssLinkReader>(
                    receiver->BindNewPipeAndPassRemote()),
                rss_links.Bind());
  link_reader.WaitForCall();

  {
    // Have link reader return TestRssUrls as well as some invalid URLs which
    // are filtered out.
    std::vector<GURL> returned_urls = TestRssUrls();
    returned_urls.push_back(GURL());
    returned_urls.push_back(GURL("chrome://non-http-url-is-ignored"));
    link_reader.Respond(
        feed::mojom::RssLinks::New(TestPageUrl(), returned_urls));
  }

  EXPECT_EQ(TestRssUrls(), rss_links.RunAndGetResult());
}

TEST_F(RssLinksFetcherUnitTest, Disconnected) {
  CallbackReceiver<std::vector<GURL>> rss_links;
  StubRssLinkReader link_reader;
  auto receiver = link_reader.MakeReceiver();
  FetchRssLinks(TestPageUrl(),
                mojo::Remote<feed::mojom::RssLinkReader>(
                    receiver->BindNewPipeAndPassRemote()),
                rss_links.Bind());
  link_reader.WaitForCall();
  receiver.reset();
  link_reader.Respond(feed::mojom::RssLinks::New(TestPageUrl(), TestRssUrls()));
  EXPECT_EQ(std::vector<GURL>(), rss_links.RunAndGetResult());
}

TEST_F(RssLinksFetcherUnitTest, PageUrlMismatch) {
  CallbackReceiver<std::vector<GURL>> rss_links;
  StubRssLinkReader link_reader;
  auto receiver = link_reader.MakeReceiver();
  FetchRssLinks(TestPageUrl(),
                mojo::Remote<feed::mojom::RssLinkReader>(
                    receiver->BindNewPipeAndPassRemote()),
                rss_links.Bind());
  link_reader.WaitForCall();
  link_reader.Respond(
      feed::mojom::RssLinks::New(GURL("https://someotherpage"), TestRssUrls()));
  EXPECT_EQ(std::vector<GURL>(), rss_links.RunAndGetResult());
}

}  // namespace
}  // namespace feed
