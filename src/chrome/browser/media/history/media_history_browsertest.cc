// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_store.h"

#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/media/history/media_history_feed_associated_origins_table.h"
#include "chrome/browser/media/history/media_history_feed_items_table.h"
#include "chrome/browser/media/history/media_history_feeds_table.h"
#include "chrome/browser/media/history/media_history_images_table.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "chrome/browser/media/history/media_history_keyed_service_factory.h"
#include "chrome/browser/media/history/media_history_origin_table.h"
#include "chrome/browser/media/history/media_history_session_images_table.h"
#include "chrome/browser/media/history/media_history_session_table.h"
#include "chrome/browser/media/history/media_history_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/history/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/media_session.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/test_utils.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "services/media_session/public/cpp/media_metadata.h"
#include "services/media_session/public/cpp/test/mock_media_session.h"

namespace media_history {

namespace {

constexpr base::TimeDelta kTestClipDuration =
    base::TimeDelta::FromMilliseconds(26771);

enum class TestState {
  kNormal,

  // Runs the test in incognito mode.
  kIncognito,

  // Runs the test with the "SavingBrowserHistoryDisabled" policy enabled.
  kSavingBrowserHistoryDisabled,
};

}  // namespace

// Runs the test with a param to signify the profile being incognito if true.
class MediaHistoryBrowserTest : public InProcessBrowserTest,
                                public testing::WithParamInterface<TestState> {
 public:
  MediaHistoryBrowserTest() = default;
  ~MediaHistoryBrowserTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {media::kUseMediaHistoryStore, media::kMediaFeeds}, {});

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    InProcessBrowserTest::SetUpOnMainThread();
  }

  static bool SetupPageAndStartPlaying(Browser* browser, const GURL& url) {
    ui_test_utils::NavigateToURL(browser, url);

    bool played = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        browser->tab_strip_model()->GetActiveWebContents(),
        "attemptPlayVideo();", &played));
    return played;
  }

  static bool SetupPageAndStartPlayingAudioOnly(Browser* browser,
                                                const GURL& url) {
    ui_test_utils::NavigateToURL(browser, url);

    bool played = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        browser->tab_strip_model()->GetActiveWebContents(),
        "attemptPlayAudioOnly();", &played));
    return played;
  }

  static bool SetupPageAndStartPlayingVideoOnly(Browser* browser,
                                                const GURL& url) {
    ui_test_utils::NavigateToURL(browser, url);

    bool played = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        browser->tab_strip_model()->GetActiveWebContents(),
        "attemptPlayVideoOnly();", &played));
    return played;
  }

  static bool SetMediaMetadata(Browser* browser) {
    return content::ExecuteScript(
        browser->tab_strip_model()->GetActiveWebContents(),
        "setMediaMetadata();");
  }

  static bool SetMediaMetadataWithArtwork(Browser* browser) {
    return content::ExecuteScript(
        browser->tab_strip_model()->GetActiveWebContents(),
        "setMediaMetadataWithArtwork();");
  }

  static bool FinishPlaying(Browser* browser) {
    return content::ExecuteScript(
        browser->tab_strip_model()->GetActiveWebContents(), "finishPlaying();");
  }

  static bool WaitForSignificantPlayback(Browser* browser) {
    bool seeked = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        browser->tab_strip_model()->GetActiveWebContents(),
        "waitForSignificantPlayback();", &seeked));
    return seeked;
  }

  static std::vector<mojom::MediaHistoryPlaybackSessionRowPtr>
  GetPlaybackSessionsSync(MediaHistoryKeyedService* service, int max_sessions) {
    return GetPlaybackSessionsSync(
        service, max_sessions,
        base::BindRepeating([](const base::TimeDelta& duration,
                               const base::TimeDelta& position) {
          return duration.InSeconds() != position.InSeconds();
        }));
  }

  static std::vector<mojom::MediaHistoryPlaybackSessionRowPtr>
  GetPlaybackSessionsSync(MediaHistoryKeyedService* service,
                          int max_sessions,
                          MediaHistoryStore::GetPlaybackSessionsFilter filter) {
    base::RunLoop run_loop;
    std::vector<mojom::MediaHistoryPlaybackSessionRowPtr> out;

    service->GetPlaybackSessions(
        max_sessions, std::move(filter),
        base::BindLambdaForTesting(
            [&](std::vector<mojom::MediaHistoryPlaybackSessionRowPtr>
                    sessions) {
              out = std::move(sessions);
              run_loop.Quit();
            }));

    run_loop.Run();
    return out;
  }

  static mojom::MediaHistoryStatsPtr GetStatsSync(
      MediaHistoryKeyedService* service) {
    base::RunLoop run_loop;
    mojom::MediaHistoryStatsPtr stats_out;

    service->GetMediaHistoryStats(
        base::BindLambdaForTesting([&](mojom::MediaHistoryStatsPtr stats) {
          stats_out = std::move(stats);
          run_loop.Quit();
        }));

    run_loop.Run();
    return stats_out;
  }

  static std::vector<mojom::MediaHistoryPlaybackRowPtr> GetPlaybacksSync(
      MediaHistoryKeyedService* service) {
    base::RunLoop run_loop;
    std::vector<mojom::MediaHistoryPlaybackRowPtr> out;

    service->GetMediaHistoryPlaybackRowsForDebug(base::BindLambdaForTesting(
        [&](std::vector<mojom::MediaHistoryPlaybackRowPtr> playbacks) {
          out = std::move(playbacks);
          run_loop.Quit();
        }));

    run_loop.Run();
    return out;
  }

  static std::vector<mojom::MediaHistoryOriginRowPtr> GetOriginsSync(
      MediaHistoryKeyedService* service) {
    base::RunLoop run_loop;
    std::vector<mojom::MediaHistoryOriginRowPtr> out;

    service->GetOriginRowsForDebug(base::BindLambdaForTesting(
        [&](std::vector<mojom::MediaHistoryOriginRowPtr> origins) {
          out = std::move(origins);
          run_loop.Quit();
        }));

    run_loop.Run();
    return out;
  }

  media_session::MediaMetadata GetExpectedMetadata() {
    media_session::MediaMetadata expected_metadata =
        GetExpectedDefaultMetadata();
    expected_metadata.title = base::ASCIIToUTF16("Big Buck Bunny");
    expected_metadata.artist = base::ASCIIToUTF16("Test Footage");
    expected_metadata.album = base::ASCIIToUTF16("The Chrome Collection");
    return expected_metadata;
  }

  std::vector<media_session::MediaImage> GetExpectedArtwork() {
    std::vector<media_session::MediaImage> images;

    {
      media_session::MediaImage image;
      image.src = embedded_test_server()->GetURL("/artwork-96.png");
      image.sizes.push_back(gfx::Size(96, 96));
      image.type = base::ASCIIToUTF16("image/png");
      images.push_back(image);
    }

    {
      media_session::MediaImage image;
      image.src = embedded_test_server()->GetURL("/artwork-128.png");
      image.sizes.push_back(gfx::Size(128, 128));
      image.type = base::ASCIIToUTF16("image/png");
      images.push_back(image);
    }

    {
      media_session::MediaImage image;
      image.src = embedded_test_server()->GetURL("/artwork-big.jpg");
      image.sizes.push_back(gfx::Size(192, 192));
      image.sizes.push_back(gfx::Size(256, 256));
      image.type = base::ASCIIToUTF16("image/jpg");
      images.push_back(image);
    }

    {
      media_session::MediaImage image;
      image.src = embedded_test_server()->GetURL("/artwork-any.jpg");
      image.sizes.push_back(gfx::Size(0, 0));
      image.type = base::ASCIIToUTF16("image/jpg");
      images.push_back(image);
    }

    {
      media_session::MediaImage image;
      image.src = embedded_test_server()->GetURL("/artwork-notype.jpg");
      image.sizes.push_back(gfx::Size(0, 0));
      images.push_back(image);
    }

    {
      media_session::MediaImage image;
      image.src = embedded_test_server()->GetURL("/artwork-nosize.jpg");
      image.type = base::ASCIIToUTF16("image/jpg");
      images.push_back(image);
    }

    return images;
  }

  media_session::MediaMetadata GetExpectedDefaultMetadata() {
    media_session::MediaMetadata expected_metadata;
    expected_metadata.title = base::ASCIIToUTF16("Media History");
    expected_metadata.source_title = base::ASCIIToUTF16(base::StringPrintf(
        "%s:%u", embedded_test_server()->GetIPLiteralString().c_str(),
        embedded_test_server()->port()));
    return expected_metadata;
  }

  void SimulateNavigationToCommit(Browser* browser) {
    // Navigate to trigger the session to be saved.
    ui_test_utils::NavigateToURL(browser, embedded_test_server()->base_url());

    // Wait until the session has finished saving.
    WaitForDB(GetMediaHistoryService(browser));
  }

  media_history::MediaHistoryKeyedService::MediaFeedFetchResult FetchResult(
      MediaHistoryKeyedService* service,
      const int64_t feed_id) {
    media_history::MediaHistoryKeyedService::MediaFeedFetchResult result;
    result.feed_id = feed_id;
    result.items = GetExpectedItems();
    result.status = media_feeds::mojom::FetchResult::kSuccess;
    result.associated_origins = GetExpectedAssociatedOrigins();
    result.display_name = "Test";
    result.reset_token = test::GetResetTokenSync(service, feed_id);
    return result;
  }

  const GURL GetTestURL() const {
    return embedded_test_server()->GetURL("/media/media_history.html");
  }

  const GURL GetTestAltURL() const {
    return embedded_test_server()->GetURL("/media/media_history.html?alt=1");
  }

  static content::MediaSession* GetMediaSession(Browser* browser) {
    return content::MediaSession::Get(
        browser->tab_strip_model()->GetActiveWebContents());
  }

  static MediaHistoryKeyedService* GetMediaHistoryService(Browser* browser) {
    return MediaHistoryKeyedServiceFactory::GetForProfile(browser->profile());
  }

  static MediaHistoryKeyedService* GetOTRMediaHistoryService(Browser* browser) {
    return MediaHistoryKeyedServiceFactory::GetForProfile(
        browser->profile()->GetOffTheRecordProfile());
  }

  static void WaitForDB(MediaHistoryKeyedService* service) {
    base::RunLoop run_loop;
    service->PostTaskToDBForTest(run_loop.QuitClosure());
    run_loop.Run();
  }

  static std::set<url::Origin> GetExpectedAssociatedOrigins() {
    std::set<url::Origin> origins;

    origins.insert(url::Origin::Create(GURL("https://www.google.com")));
    origins.insert(url::Origin::Create(GURL("https://www.google1.com")));
    origins.insert(url::Origin::Create(GURL("https://www.google2.com")));
    origins.insert(url::Origin::Create(GURL("https://www.google3.com")));
    origins.insert(url::Origin::Create(GURL("https://www.example.org")));

    return origins;
  }

  static std::vector<media_feeds::mojom::MediaFeedItemPtr> GetExpectedItems() {
    std::vector<media_feeds::mojom::MediaFeedItemPtr> items;

    {
      auto item = media_feeds::mojom::MediaFeedItem::New();
      item->type = media_feeds::mojom::MediaFeedItemType::kVideo;
      item->name = base::ASCIIToUTF16("The Video");
      item->date_published = base::Time::FromDeltaSinceWindowsEpoch(
          base::TimeDelta::FromMinutes(20));
      item->is_family_friendly = false;
      item->action_status =
          media_feeds::mojom::MediaFeedItemActionStatus::kActive;
      items.push_back(std::move(item));
    }

    return items;
  }

  Browser* CreateBrowserFromParam() {
    if (GetParam() == TestState::kIncognito) {
      return CreateIncognitoBrowser();
    } else if (GetParam() == TestState::kSavingBrowserHistoryDisabled) {
      browser()->profile()->GetPrefs()->SetBoolean(
          prefs::kSavingBrowserHistoryDisabled, true);
    }

    return browser();
  }

  bool IsReadOnly() const { return GetParam() != TestState::kNormal; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    MediaHistoryBrowserTest,
    testing::Values(TestState::kNormal,
                    TestState::kIncognito,
                    TestState::kSavingBrowserHistoryDisabled));

IN_PROC_BROWSER_TEST_P(MediaHistoryBrowserTest,
                       RecordMediaSession_OnNavigate_Incomplete) {
  auto* browser = CreateBrowserFromParam();

  EXPECT_TRUE(SetupPageAndStartPlaying(browser, GetTestURL()));
  EXPECT_TRUE(SetMediaMetadataWithArtwork(browser));

  auto expected_metadata = GetExpectedMetadata();
  auto expected_artwork = GetExpectedArtwork();

  {
    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession(browser));
    observer.WaitForState(
        media_session::mojom::MediaSessionInfo::SessionState::kActive);
    observer.WaitForExpectedMetadata(expected_metadata);
    observer.WaitForExpectedImagesOfType(
        media_session::mojom::MediaSessionImageType::kArtwork,
        expected_artwork);
    observer.WaitForAudioVideoState(
        media_session::mojom::MediaAudioVideoState::kAudioVideo);
  }

  SimulateNavigationToCommit(browser);

  // Verify the session in the database.
  auto sessions = GetPlaybackSessionsSync(GetMediaHistoryService(browser), 1);

  if (IsReadOnly()) {
    EXPECT_TRUE(sessions.empty());
  } else {
    EXPECT_EQ(1u, sessions.size());
    EXPECT_EQ(GetTestURL(), sessions[0]->url);
    EXPECT_EQ(kTestClipDuration, sessions[0]->duration);
    EXPECT_LT(base::TimeDelta(), sessions[0]->position);
    EXPECT_EQ(expected_metadata.title, sessions[0]->metadata.title);
    EXPECT_EQ(expected_metadata.artist, sessions[0]->metadata.artist);
    EXPECT_EQ(expected_metadata.album, sessions[0]->metadata.album);
    EXPECT_EQ(expected_metadata.source_title,
              sessions[0]->metadata.source_title);
    EXPECT_EQ(expected_artwork, sessions[0]->artwork);
  }

  // The OTR service should have the same data.
  EXPECT_EQ(sessions,
            GetPlaybackSessionsSync(GetOTRMediaHistoryService(browser), 1));

  {
    // Check the tables have the expected number of records
    mojom::MediaHistoryStatsPtr stats =
        GetStatsSync(GetMediaHistoryService(browser));

    if (IsReadOnly()) {
      EXPECT_EQ(0,
                stats->table_row_counts[MediaHistoryOriginTable::kTableName]);
      EXPECT_EQ(0,
                stats->table_row_counts[MediaHistorySessionTable::kTableName]);
      EXPECT_EQ(
          0,
          stats->table_row_counts[MediaHistorySessionImagesTable::kTableName]);
      EXPECT_EQ(0,
                stats->table_row_counts[MediaHistoryImagesTable::kTableName]);
    } else {
      EXPECT_EQ(1,
                stats->table_row_counts[MediaHistoryOriginTable::kTableName]);
      EXPECT_EQ(1,
                stats->table_row_counts[MediaHistorySessionTable::kTableName]);
      EXPECT_EQ(
          7,
          stats->table_row_counts[MediaHistorySessionImagesTable::kTableName]);
      EXPECT_EQ(6,
                stats->table_row_counts[MediaHistoryImagesTable::kTableName]);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(stats, GetStatsSync(GetOTRMediaHistoryService(browser)));
  }
}

IN_PROC_BROWSER_TEST_P(MediaHistoryBrowserTest,
                       RecordMediaSession_DefaultMetadata) {
  auto* browser = CreateBrowserFromParam();

  EXPECT_TRUE(SetupPageAndStartPlaying(browser, GetTestURL()));

  media_session::MediaMetadata expected_metadata = GetExpectedDefaultMetadata();

  {
    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession(browser));
    observer.WaitForState(
        media_session::mojom::MediaSessionInfo::SessionState::kActive);
    observer.WaitForExpectedMetadata(expected_metadata);
    observer.WaitForAudioVideoState(
        media_session::mojom::MediaAudioVideoState::kAudioVideo);
  }

  SimulateNavigationToCommit(browser);

  // Verify the session in the database.
  auto sessions = GetPlaybackSessionsSync(GetMediaHistoryService(browser), 1);

  if (IsReadOnly()) {
    EXPECT_TRUE(sessions.empty());
  } else {
    EXPECT_EQ(1u, sessions.size());
    EXPECT_EQ(GetTestURL(), sessions[0]->url);
    EXPECT_EQ(kTestClipDuration, sessions[0]->duration);
    EXPECT_LT(base::TimeDelta(), sessions[0]->position);
    EXPECT_EQ(expected_metadata.title, sessions[0]->metadata.title);
    EXPECT_EQ(expected_metadata.artist, sessions[0]->metadata.artist);
    EXPECT_EQ(expected_metadata.album, sessions[0]->metadata.album);
    EXPECT_EQ(expected_metadata.source_title,
              sessions[0]->metadata.source_title);
    EXPECT_TRUE(sessions[0]->artwork.empty());
  }

  // The OTR service should have the same data.
  EXPECT_EQ(sessions,
            GetPlaybackSessionsSync(GetOTRMediaHistoryService(browser), 1));
}

// TODO(crbug.com/1078463): Flaky on Mac, Linux ASAN, and Win ASAN.
IN_PROC_BROWSER_TEST_P(MediaHistoryBrowserTest,
                       DISABLED_RecordMediaSession_OnNavigate_Complete) {
  auto* browser = CreateBrowserFromParam();

  EXPECT_TRUE(SetupPageAndStartPlaying(browser, GetTestURL()));
  EXPECT_TRUE(FinishPlaying(browser));

  media_session::MediaMetadata expected_metadata = GetExpectedDefaultMetadata();

  {
    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession(browser));
    observer.WaitForState(
        media_session::mojom::MediaSessionInfo::SessionState::kActive);
    observer.WaitForExpectedMetadata(expected_metadata);
    observer.WaitForAudioVideoState(
        media_session::mojom::MediaAudioVideoState::kAudioVideo);
  }

  SimulateNavigationToCommit(browser);

  {
    // The session will not be returned since it is complete.
    auto sessions = GetPlaybackSessionsSync(GetMediaHistoryService(browser), 1);
    EXPECT_TRUE(sessions.empty());

    // The OTR service should have the same data.
    EXPECT_TRUE(
        GetPlaybackSessionsSync(GetOTRMediaHistoryService(browser), 1).empty());
  }

  {
    // If we remove the filter when we get the sessions we should see a result.
    auto filter = base::BindRepeating(
        [](const base::TimeDelta& duration, const base::TimeDelta& position) {
          return true;
        });

    auto sessions =
        GetPlaybackSessionsSync(GetMediaHistoryService(browser), 1, filter);

    if (IsReadOnly()) {
      EXPECT_TRUE(sessions.empty());
    } else {
      EXPECT_EQ(1u, sessions.size());
      EXPECT_EQ(GetTestURL(), sessions[0]->url);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(sessions, GetPlaybackSessionsSync(
                            GetOTRMediaHistoryService(browser), 1, filter));
  }
}

IN_PROC_BROWSER_TEST_P(MediaHistoryBrowserTest, DoNotRecordSessionIfNotActive) {
  auto* browser = CreateBrowserFromParam();

  ui_test_utils::NavigateToURL(browser, GetTestURL());
  EXPECT_TRUE(SetMediaMetadata(browser));

  media_session::MediaMetadata expected_metadata = GetExpectedDefaultMetadata();

  {
    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession(browser));
    observer.WaitForState(
        media_session::mojom::MediaSessionInfo::SessionState::kInactive);
    observer.WaitForExpectedMetadata(expected_metadata);
  }

  SimulateNavigationToCommit(browser);

  // Verify the session has not been stored in the database.
  auto sessions = GetPlaybackSessionsSync(GetMediaHistoryService(browser), 1);
  EXPECT_TRUE(sessions.empty());

  // The OTR service should have the same data.
  EXPECT_TRUE(
      GetPlaybackSessionsSync(GetOTRMediaHistoryService(browser), 1).empty());
}

// Flaky failures: crbug.com/1066853
IN_PROC_BROWSER_TEST_P(MediaHistoryBrowserTest, DISABLED_GetPlaybackSessions) {
  auto* browser = CreateBrowserFromParam();
  auto expected_default_metadata = GetExpectedDefaultMetadata();

  {
    // Start a session.
    EXPECT_TRUE(SetupPageAndStartPlaying(browser, GetTestURL()));
    EXPECT_TRUE(SetMediaMetadataWithArtwork(browser));

    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession(browser));
    observer.WaitForState(
        media_session::mojom::MediaSessionInfo::SessionState::kActive);
    observer.WaitForExpectedMetadata(GetExpectedMetadata());
    observer.WaitForAudioVideoState(
        media_session::mojom::MediaAudioVideoState::kAudioVideo);
  }

  SimulateNavigationToCommit(browser);

  {
    // Start a second session on a different URL.
    EXPECT_TRUE(SetupPageAndStartPlaying(browser, GetTestAltURL()));

    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession(browser));
    observer.WaitForState(
        media_session::mojom::MediaSessionInfo::SessionState::kActive);
    observer.WaitForExpectedMetadata(expected_default_metadata);
    observer.WaitForAudioVideoState(
        media_session::mojom::MediaAudioVideoState::kAudioVideo);
  }

  SimulateNavigationToCommit(browser);

  {
    // Get the two most recent playback sessions and check they are in order.
    auto sessions = GetPlaybackSessionsSync(GetMediaHistoryService(browser), 2);

    if (IsReadOnly()) {
      EXPECT_TRUE(sessions.empty());
    } else {
      ASSERT_EQ(2u, sessions.size());
      EXPECT_EQ(GetTestAltURL(), sessions[0]->url);
      EXPECT_EQ(GetTestURL(), sessions[1]->url);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(sessions,
              GetPlaybackSessionsSync(GetOTRMediaHistoryService(browser), 2));
  }

  {
    // Get the last playback session.
    auto sessions = GetPlaybackSessionsSync(GetMediaHistoryService(browser), 1);

    if (IsReadOnly()) {
      EXPECT_TRUE(sessions.empty());
    } else {
      EXPECT_EQ(1u, sessions.size());
      EXPECT_EQ(GetTestAltURL(), sessions[0]->url);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(sessions,
              GetPlaybackSessionsSync(GetOTRMediaHistoryService(browser), 1));
  }

  {
    // Start the first page again and seek to 4 seconds in with different
    // metadata.
    EXPECT_TRUE(SetupPageAndStartPlaying(browser, GetTestURL()));
    EXPECT_TRUE(content::ExecuteScript(
        browser->tab_strip_model()->GetActiveWebContents(), "seekToFour()"));

    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession(browser));
    observer.WaitForState(
        media_session::mojom::MediaSessionInfo::SessionState::kActive);
    observer.WaitForExpectedMetadata(expected_default_metadata);
  }

  SimulateNavigationToCommit(browser);

  {
    // Check that recent playback sessions only returns two playback sessions
    // because the first one was collapsed into the third one since they
    // have the same URL. We should also use the data from the most recent
    // playback.
    auto sessions = GetPlaybackSessionsSync(GetMediaHistoryService(browser), 3);

    if (IsReadOnly()) {
      EXPECT_TRUE(sessions.empty());
    } else {
      ASSERT_EQ(2u, sessions.size());
      EXPECT_EQ(GetTestURL(), sessions[0]->url);
      EXPECT_EQ(GetTestAltURL(), sessions[1]->url);

      EXPECT_EQ(kTestClipDuration, sessions[0]->duration);
      EXPECT_EQ(4, sessions[0]->position.InSeconds());
      EXPECT_EQ(expected_default_metadata.title, sessions[0]->metadata.title);
      EXPECT_EQ(expected_default_metadata.artist, sessions[0]->metadata.artist);
      EXPECT_EQ(expected_default_metadata.album, sessions[0]->metadata.album);
      EXPECT_EQ(expected_default_metadata.source_title,
                sessions[0]->metadata.source_title);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(sessions,
              GetPlaybackSessionsSync(GetOTRMediaHistoryService(browser), 3));
  }

  {
    // Start the first page again and finish playing.
    EXPECT_TRUE(SetupPageAndStartPlaying(browser, GetTestURL()));
    EXPECT_TRUE(FinishPlaying(browser));

    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession(browser));
    observer.WaitForState(
        media_session::mojom::MediaSessionInfo::SessionState::kActive);
    observer.WaitForExpectedMetadata(expected_default_metadata);
    observer.WaitForAudioVideoState(
        media_session::mojom::MediaAudioVideoState::kAudioVideo);
  }

  SimulateNavigationToCommit(browser);

  {
    // Get the recent playbacks and the test URL should not appear at all
    // because playback has completed for that URL.
    auto sessions = GetPlaybackSessionsSync(GetMediaHistoryService(browser), 4);

    if (IsReadOnly()) {
      EXPECT_TRUE(sessions.empty());
    } else {
      EXPECT_EQ(1u, sessions.size());
      EXPECT_EQ(GetTestAltURL(), sessions[0]->url);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(sessions,
              GetPlaybackSessionsSync(GetOTRMediaHistoryService(browser), 4));
  }

  {
    // Start the first session again.
    EXPECT_TRUE(SetupPageAndStartPlaying(browser, GetTestURL()));
    EXPECT_TRUE(SetMediaMetadata(browser));

    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession(browser));
    observer.WaitForState(
        media_session::mojom::MediaSessionInfo::SessionState::kActive);
    observer.WaitForExpectedMetadata(GetExpectedMetadata());
    observer.WaitForAudioVideoState(
        media_session::mojom::MediaAudioVideoState::kAudioVideo);
  }

  SimulateNavigationToCommit(browser);

  {
    // The test URL should now appear in the recent playbacks list again since
    // it is incomplete again.
    auto sessions = GetPlaybackSessionsSync(GetMediaHistoryService(browser), 2);

    if (IsReadOnly()) {
      EXPECT_TRUE(sessions.empty());
    } else {
      ASSERT_EQ(2u, sessions.size());
      EXPECT_EQ(GetTestURL(), sessions[0]->url);
      EXPECT_EQ(GetTestAltURL(), sessions[1]->url);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(sessions,
              GetPlaybackSessionsSync(GetOTRMediaHistoryService(browser), 2));
  }
}

IN_PROC_BROWSER_TEST_P(MediaHistoryBrowserTest,
                       SaveImagesWithDifferentSessions) {
  auto* browser = CreateBrowserFromParam();
  auto expected_metadata = GetExpectedMetadata();
  auto expected_artwork = GetExpectedArtwork();

  {
    // Start a session.
    EXPECT_TRUE(SetupPageAndStartPlaying(browser, GetTestURL()));
    EXPECT_TRUE(SetMediaMetadataWithArtwork(browser));

    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession(browser));
    observer.WaitForState(
        media_session::mojom::MediaSessionInfo::SessionState::kActive);
    observer.WaitForExpectedMetadata(expected_metadata);
    observer.WaitForExpectedImagesOfType(
        media_session::mojom::MediaSessionImageType::kArtwork,
        expected_artwork);
  }

  SimulateNavigationToCommit(browser);

  std::vector<media_session::MediaImage> expected_alt_artwork;

  {
    media_session::MediaImage image;
    image.src = embedded_test_server()->GetURL("/artwork-96.png");
    image.sizes.push_back(gfx::Size(96, 96));
    image.type = base::ASCIIToUTF16("image/png");
    expected_alt_artwork.push_back(image);
  }

  {
    media_session::MediaImage image;
    image.src = embedded_test_server()->GetURL("/artwork-alt.png");
    image.sizes.push_back(gfx::Size(128, 128));
    image.type = base::ASCIIToUTF16("image/png");
    expected_alt_artwork.push_back(image);
  }

  {
    // Start a second session on a different URL.
    EXPECT_TRUE(SetupPageAndStartPlaying(browser, GetTestAltURL()));
    EXPECT_TRUE(content::ExecuteScript(
        browser->tab_strip_model()->GetActiveWebContents(),
        "setMediaMetadataWithAltArtwork();"));

    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession(browser));
    observer.WaitForState(
        media_session::mojom::MediaSessionInfo::SessionState::kActive);
    observer.WaitForExpectedMetadata(expected_metadata);
    observer.WaitForExpectedImagesOfType(
        media_session::mojom::MediaSessionImageType::kArtwork,
        expected_alt_artwork);
  }

  SimulateNavigationToCommit(browser);

  // Verify the session in the database.
  auto sessions = GetPlaybackSessionsSync(GetMediaHistoryService(browser), 2);

  if (IsReadOnly()) {
    EXPECT_TRUE(sessions.empty());
  } else {
    ASSERT_EQ(2u, sessions.size());
    EXPECT_EQ(GetTestAltURL(), sessions[0]->url);
    EXPECT_EQ(expected_alt_artwork, sessions[0]->artwork);
    EXPECT_EQ(GetTestURL(), sessions[1]->url);
    EXPECT_EQ(expected_artwork, sessions[1]->artwork);
  }

  // The OTR service should have the same data.
  EXPECT_EQ(sessions,
            GetPlaybackSessionsSync(GetOTRMediaHistoryService(browser), 2));
}

IN_PROC_BROWSER_TEST_P(MediaHistoryBrowserTest, RecordWatchtime_AudioVideo) {
  auto* browser = CreateBrowserFromParam();

  // Start a page and wait for significant playback so we record watchtime.
  EXPECT_TRUE(SetupPageAndStartPlaying(browser, GetTestURL()));
  EXPECT_TRUE(WaitForSignificantPlayback(browser));
  SimulateNavigationToCommit(browser);

  {
    auto playbacks = GetPlaybacksSync(GetMediaHistoryService(browser));
    auto origins = GetOriginsSync(GetMediaHistoryService(browser));

    if (IsReadOnly()) {
      EXPECT_TRUE(playbacks.empty());
      EXPECT_TRUE(origins.empty());
    } else {
      EXPECT_EQ(1u, playbacks.size());
      EXPECT_TRUE(playbacks[0]->has_audio);
      EXPECT_TRUE(playbacks[0]->has_video);
      EXPECT_EQ(GetTestURL(), playbacks[0]->url);
      EXPECT_GE(base::TimeDelta::FromSeconds(7), playbacks[0]->watchtime);

      EXPECT_EQ(1u, origins.size());
      EXPECT_EQ(url::Origin::Create(GetTestURL()), origins[0]->origin);
      EXPECT_EQ(playbacks[0]->watchtime,
                origins[0]->cached_audio_video_watchtime);
      EXPECT_EQ(playbacks[0]->watchtime,
                origins[0]->actual_audio_video_watchtime);
    }

    // The OTR service should have the same data.
    EXPECT_EQ(playbacks, GetPlaybacksSync(GetOTRMediaHistoryService(browser)));
    EXPECT_EQ(origins, GetOriginsSync(GetOTRMediaHistoryService(browser)));
  }

  // Start playing again.
  EXPECT_TRUE(SetupPageAndStartPlaying(browser, GetTestURL()));
  EXPECT_TRUE(WaitForSignificantPlayback(browser));
  SimulateNavigationToCommit(browser);

  {
    auto playbacks = GetPlaybacksSync(GetMediaHistoryService(browser));
    auto origins = GetOriginsSync(GetMediaHistoryService(browser));

    if (IsReadOnly()) {
      EXPECT_TRUE(playbacks.empty());
      EXPECT_TRUE(origins.empty());
    } else {
      EXPECT_EQ(2u, playbacks.size());

      // Calculate the total watchtime across the playbacks.
      base::TimeDelta total_time;
      for (auto& playback : playbacks) {
        EXPECT_EQ(GetTestURL(), playback->url);
        total_time += playback->watchtime;
      }

      // The total aggregate watchtime should have increased.
      EXPECT_EQ(1u, origins.size());
      EXPECT_EQ(url::Origin::Create(GetTestURL()), origins[0]->origin);
      EXPECT_EQ(total_time, origins[0]->cached_audio_video_watchtime);
      EXPECT_EQ(total_time, origins[0]->actual_audio_video_watchtime);

      // The OTR service should have the same data.
      EXPECT_EQ(playbacks,
                GetPlaybacksSync(GetOTRMediaHistoryService(browser)));
      EXPECT_EQ(origins, GetOriginsSync(GetOTRMediaHistoryService(browser)));
    }
  }
}

IN_PROC_BROWSER_TEST_P(MediaHistoryBrowserTest, RecordWatchtime_AudioOnly) {
  auto* browser = CreateBrowserFromParam();

  // Start a page and wait for significant playback so we record watchtime.
  EXPECT_TRUE(SetupPageAndStartPlayingAudioOnly(browser, GetTestURL()));
  EXPECT_TRUE(WaitForSignificantPlayback(browser));

  SimulateNavigationToCommit(browser);

  {
    auto playbacks = GetPlaybacksSync(GetMediaHistoryService(browser));
    auto origins = GetOriginsSync(GetMediaHistoryService(browser));

    if (IsReadOnly()) {
      EXPECT_TRUE(playbacks.empty());
      EXPECT_TRUE(origins.empty());
    } else {
      EXPECT_EQ(1u, playbacks.size());
      EXPECT_TRUE(playbacks[0]->has_audio);
      EXPECT_FALSE(playbacks[0]->has_video);
      EXPECT_EQ(GetTestURL(), playbacks[0]->url);
      EXPECT_GE(base::TimeDelta::FromSeconds(7), playbacks[0]->watchtime);

      EXPECT_EQ(1u, origins.size());
      EXPECT_EQ(url::Origin::Create(GetTestURL()), origins[0]->origin);
      EXPECT_TRUE(origins[0]->cached_audio_video_watchtime.is_zero());
      EXPECT_TRUE(origins[0]->actual_audio_video_watchtime.is_zero());

      // The OTR service should have the same data.
      EXPECT_EQ(playbacks,
                GetPlaybacksSync(GetOTRMediaHistoryService(browser)));
      EXPECT_EQ(origins, GetOriginsSync(GetOTRMediaHistoryService(browser)));
    }
  }

  // Start playing again.
  EXPECT_TRUE(SetupPageAndStartPlayingAudioOnly(browser, GetTestURL()));
  EXPECT_TRUE(WaitForSignificantPlayback(browser));
  SimulateNavigationToCommit(browser);

  {
    auto playbacks = GetPlaybacksSync(GetMediaHistoryService(browser));
    auto origins = GetOriginsSync(GetMediaHistoryService(browser));

    if (IsReadOnly()) {
      EXPECT_TRUE(playbacks.empty());
      EXPECT_TRUE(origins.empty());
    } else {
      EXPECT_EQ(2u, playbacks.size());

      // The total aggregate watchtime should not have increased..
      EXPECT_EQ(1u, origins.size());
      EXPECT_EQ(url::Origin::Create(GetTestURL()), origins[0]->origin);
      EXPECT_TRUE(origins[0]->cached_audio_video_watchtime.is_zero());
      EXPECT_TRUE(origins[0]->actual_audio_video_watchtime.is_zero());

      // The OTR service should have the same data.
      EXPECT_EQ(playbacks,
                GetPlaybacksSync(GetOTRMediaHistoryService(browser)));
      EXPECT_EQ(origins, GetOriginsSync(GetOTRMediaHistoryService(browser)));
    }
  }
}

IN_PROC_BROWSER_TEST_P(MediaHistoryBrowserTest, RecordWatchtime_VideoOnly) {
  auto* browser = CreateBrowserFromParam();

  // Start a page and wait for significant playback so we record watchtime.
  EXPECT_TRUE(SetupPageAndStartPlayingVideoOnly(browser, GetTestURL()));
  EXPECT_TRUE(WaitForSignificantPlayback(browser));

  SimulateNavigationToCommit(browser);

  {
    auto playbacks = GetPlaybacksSync(GetMediaHistoryService(browser));
    auto origins = GetOriginsSync(GetMediaHistoryService(browser));

    if (IsReadOnly()) {
      EXPECT_TRUE(playbacks.empty());
      EXPECT_TRUE(origins.empty());
    } else {
      EXPECT_EQ(1u, playbacks.size());
      EXPECT_FALSE(playbacks[0]->has_audio);
      EXPECT_TRUE(playbacks[0]->has_video);
      EXPECT_EQ(GetTestURL(), playbacks[0]->url);
      EXPECT_GE(base::TimeDelta::FromSeconds(7), playbacks[0]->watchtime);

      EXPECT_EQ(1u, origins.size());
      EXPECT_EQ(url::Origin::Create(GetTestURL()), origins[0]->origin);
      EXPECT_TRUE(origins[0]->cached_audio_video_watchtime.is_zero());
      EXPECT_TRUE(origins[0]->actual_audio_video_watchtime.is_zero());

      // The OTR service should have the same data.
      EXPECT_EQ(playbacks,
                GetPlaybacksSync(GetOTRMediaHistoryService(browser)));
      EXPECT_EQ(origins, GetOriginsSync(GetOTRMediaHistoryService(browser)));
    }
  }

  // Start playing again.
  EXPECT_TRUE(SetupPageAndStartPlayingVideoOnly(browser, GetTestURL()));
  EXPECT_TRUE(WaitForSignificantPlayback(browser));
  SimulateNavigationToCommit(browser);

  {
    auto playbacks = GetPlaybacksSync(GetMediaHistoryService(browser));
    auto origins = GetOriginsSync(GetMediaHistoryService(browser));

    if (IsReadOnly()) {
      EXPECT_TRUE(playbacks.empty());
      EXPECT_TRUE(origins.empty());
    } else {
      EXPECT_EQ(2u, playbacks.size());

      // The total aggregate watchtime should not have increased.
      EXPECT_EQ(1u, origins.size());
      EXPECT_EQ(url::Origin::Create(GetTestURL()), origins[0]->origin);
      EXPECT_TRUE(origins[0]->cached_audio_video_watchtime.is_zero());
      EXPECT_TRUE(origins[0]->actual_audio_video_watchtime.is_zero());

      // The OTR service should have the same data.
      EXPECT_EQ(playbacks,
                GetPlaybacksSync(GetOTRMediaHistoryService(browser)));
      EXPECT_EQ(origins, GetOriginsSync(GetOTRMediaHistoryService(browser)));
    }
  }
}

IN_PROC_BROWSER_TEST_P(MediaHistoryBrowserTest,
                       DoNotRecordSessionForAudioOnly) {
  auto* browser = CreateBrowserFromParam();

  SetupPageAndStartPlayingAudioOnly(browser, GetTestURL());

  {
    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession(browser));
    observer.WaitForState(
        media_session::mojom::MediaSessionInfo::SessionState::kActive);
    observer.WaitForAudioVideoState(
        media_session::mojom::MediaAudioVideoState::kAudioOnly);
  }

  SimulateNavigationToCommit(browser);

  // Verify the session was not recorded.
  auto sessions = GetPlaybackSessionsSync(GetMediaHistoryService(browser), 1);
  EXPECT_TRUE(sessions.empty());
}

IN_PROC_BROWSER_TEST_P(MediaHistoryBrowserTest,
                       DoNotRecordSessionForVideoOnly) {
  auto* browser = CreateBrowserFromParam();

  SetupPageAndStartPlayingVideoOnly(browser, GetTestURL());
  WaitForSignificantPlayback(browser);

  SimulateNavigationToCommit(browser);

  // Verify the session was not recorded.
  auto sessions = GetPlaybackSessionsSync(GetMediaHistoryService(browser), 1);
  EXPECT_TRUE(sessions.empty());
}

IN_PROC_BROWSER_TEST_P(MediaHistoryBrowserTest,
                       ResetFeedsWhenBrowsingDataCleared) {
  auto* browser = CreateBrowserFromParam();
  auto* service = GetMediaHistoryService(browser);

  // Discover a test feed.
  service->DiscoverMediaFeed(GURL("https://www.google.com/media-feed.json"));
  WaitForDB(service);

  // Store the feed data.
  service->StoreMediaFeedFetchResult(FetchResult(service, 1),
                                     base::DoNothing());
  WaitForDB(service);

  {
    // Check that the tables have the right count in them.
    auto stats = GetStatsSync(service);

    if (IsReadOnly()) {
      EXPECT_EQ(0, stats->table_row_counts[MediaHistoryFeedsTable::kTableName]);
      EXPECT_EQ(
          0, stats->table_row_counts[MediaHistoryFeedItemsTable::kTableName]);
      EXPECT_EQ(0, stats->table_row_counts
                       [MediaHistoryFeedAssociatedOriginsTable::kTableName]);
    } else {
      EXPECT_EQ(1, stats->table_row_counts[MediaHistoryFeedsTable::kTableName]);
      EXPECT_EQ(
          1, stats->table_row_counts[MediaHistoryFeedItemsTable::kTableName]);
      EXPECT_EQ(5, stats->table_row_counts
                       [MediaHistoryFeedAssociatedOriginsTable::kTableName]);
    }
  }

  // Clear the browsing data.
  content::BrowsingDataRemover* remover =
      content::BrowserContext::GetBrowsingDataRemover(browser->profile());
  content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
  remover->RemoveAndReply(
      base::Time(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_CACHE,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
      &completion_observer);
  completion_observer.BlockUntilCompletion();

  {
    // Check that the tables have the right count in them.
    auto stats = GetStatsSync(service);

    if (IsReadOnly()) {
      EXPECT_EQ(0, stats->table_row_counts[MediaHistoryFeedsTable::kTableName]);
      EXPECT_EQ(
          0, stats->table_row_counts[MediaHistoryFeedItemsTable::kTableName]);
      EXPECT_EQ(0, stats->table_row_counts
                       [MediaHistoryFeedAssociatedOriginsTable::kTableName]);
    } else {
      EXPECT_EQ(1, stats->table_row_counts[MediaHistoryFeedsTable::kTableName]);
      EXPECT_EQ(
          0, stats->table_row_counts[MediaHistoryFeedItemsTable::kTableName]);
      EXPECT_EQ(1, stats->table_row_counts
                       [MediaHistoryFeedAssociatedOriginsTable::kTableName]);
    }
  }
}

IN_PROC_BROWSER_TEST_P(MediaHistoryBrowserTest,
                       ResetFeedsWhenBrowsingDataClearedWithFilter) {
  const GURL feed_url("https://www.google.com/media-feed.json");

  auto* browser = CreateBrowserFromParam();
  auto* service = GetMediaHistoryService(browser);

  // Discover a test feed.
  service->DiscoverMediaFeed(feed_url);
  WaitForDB(service);

  // Store the feed data.
  service->StoreMediaFeedFetchResult(FetchResult(service, 1),
                                     base::DoNothing());
  WaitForDB(service);

  {
    // Check that the tables have the right count in them.
    auto stats = GetStatsSync(service);

    if (IsReadOnly()) {
      EXPECT_EQ(0, stats->table_row_counts[MediaHistoryFeedsTable::kTableName]);
      EXPECT_EQ(
          0, stats->table_row_counts[MediaHistoryFeedItemsTable::kTableName]);
      EXPECT_EQ(0, stats->table_row_counts
                       [MediaHistoryFeedAssociatedOriginsTable::kTableName]);
    } else {
      EXPECT_EQ(1, stats->table_row_counts[MediaHistoryFeedsTable::kTableName]);
      EXPECT_EQ(
          1, stats->table_row_counts[MediaHistoryFeedItemsTable::kTableName]);
      EXPECT_EQ(5, stats->table_row_counts
                       [MediaHistoryFeedAssociatedOriginsTable::kTableName]);
    }
  }

  {
    // Clear the browsing data for another origin.
    auto filter = content::BrowsingDataFilterBuilder::Create(
        content::BrowsingDataFilterBuilder::WHITELIST);
    filter->AddOrigin(url::Origin::Create(GURL("https://www.example.org")));
    content::BrowsingDataRemover* remover =
        content::BrowserContext::GetBrowsingDataRemover(browser->profile());
    content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
    remover->RemoveWithFilterAndReply(
        base::Time(), base::Time::Max(),
        content::BrowsingDataRemover::DATA_TYPE_CACHE,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
        std::move(filter), &completion_observer);
    completion_observer.BlockUntilCompletion();
  }

  {
    // Check that the tables have the right count in them (nothing should have
    // been deleted).
    auto stats = GetStatsSync(service);

    if (IsReadOnly()) {
      EXPECT_EQ(0, stats->table_row_counts[MediaHistoryFeedsTable::kTableName]);
      EXPECT_EQ(
          0, stats->table_row_counts[MediaHistoryFeedItemsTable::kTableName]);
      EXPECT_EQ(0, stats->table_row_counts
                       [MediaHistoryFeedAssociatedOriginsTable::kTableName]);
    } else {
      EXPECT_EQ(1, stats->table_row_counts[MediaHistoryFeedsTable::kTableName]);
      EXPECT_EQ(
          1, stats->table_row_counts[MediaHistoryFeedItemsTable::kTableName]);
      EXPECT_EQ(5, stats->table_row_counts
                       [MediaHistoryFeedAssociatedOriginsTable::kTableName]);
    }
  }

  {
    // Clear the browsing data for the feed origin.
    auto filter = content::BrowsingDataFilterBuilder::Create(
        content::BrowsingDataFilterBuilder::WHITELIST);
    filter->AddOrigin(url::Origin::Create(feed_url));
    content::BrowsingDataRemover* remover =
        content::BrowserContext::GetBrowsingDataRemover(browser->profile());
    content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
    remover->RemoveWithFilterAndReply(
        base::Time(), base::Time::Max(),
        content::BrowsingDataRemover::DATA_TYPE_CACHE,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
        std::move(filter), &completion_observer);
    completion_observer.BlockUntilCompletion();
  }

  {
    // Check that the tables have the right count in them.
    auto stats = GetStatsSync(service);

    if (IsReadOnly()) {
      EXPECT_EQ(0, stats->table_row_counts[MediaHistoryFeedsTable::kTableName]);
      EXPECT_EQ(
          0, stats->table_row_counts[MediaHistoryFeedItemsTable::kTableName]);
      EXPECT_EQ(0, stats->table_row_counts
                       [MediaHistoryFeedAssociatedOriginsTable::kTableName]);
    } else {
      EXPECT_EQ(1, stats->table_row_counts[MediaHistoryFeedsTable::kTableName]);
      EXPECT_EQ(
          0, stats->table_row_counts[MediaHistoryFeedItemsTable::kTableName]);
      EXPECT_EQ(1, stats->table_row_counts
                       [MediaHistoryFeedAssociatedOriginsTable::kTableName]);
    }
  }
}

IN_PROC_BROWSER_TEST_P(MediaHistoryBrowserTest,
                       DoNotRecordWatchtime_Background) {
  auto* browser = CreateBrowserFromParam();
  auto* service = GetMediaHistoryService(browser);

  // Setup the test page.
  auto* web_contents = browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(SetupPageAndStartPlaying(browser, GetTestURL()));

  // Hide the web contents.
  web_contents->WasHidden();

  // Wait for significant playback in the background tab.
  bool seeked = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents, "waitForSignificantPlayback();", &seeked));
  ASSERT_TRUE(seeked);

  // Close all the tabs to trigger any saving.
  browser->tab_strip_model()->CloseAllTabs();

  // Wait until the session has finished saving.
  WaitForDB(service);

  // We should either have not saved any playback or it should be short.
  auto playbacks = GetPlaybacksSync(service);
  if (!playbacks.empty()) {
    ASSERT_EQ(1u, playbacks.size());
    EXPECT_GE(base::TimeDelta::FromSeconds(2), playbacks[0]->watchtime);
  }
}

IN_PROC_BROWSER_TEST_P(MediaHistoryBrowserTest, DoNotRecordWatchtime_Muted) {
  auto* browser = CreateBrowserFromParam();
  auto* service = GetMediaHistoryService(browser);

  // Setup the test page and mute the player.
  auto* web_contents = browser->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser, GetTestURL());
  ASSERT_TRUE(content::ExecuteScript(web_contents, "mute();"));

  // Start playing the video.
  bool played = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents, "attemptPlayVideo();", &played));
  ASSERT_TRUE(played);

  // Wait for significant playback in the muted tab.
  WaitForSignificantPlayback(browser);

  // Close all the tabs to trigger any saving.
  browser->tab_strip_model()->CloseAllTabs();

  // Wait until the session has finished saving.
  WaitForDB(service);

  // No playbacks should have been saved since we were muted.
  auto playbacks = GetPlaybacksSync(service);
  EXPECT_TRUE(playbacks.empty());
}

}  // namespace media_history
