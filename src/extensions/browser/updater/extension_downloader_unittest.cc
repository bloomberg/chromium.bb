// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/extension_downloader.h"

#include "base/sequenced_task_runner.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/updater/extension_downloader_test_helper.h"
#include "extensions/common/extension.h"

using testing::_;
using testing::AnyNumber;
using testing::Return;
using testing::Sequence;
using testing::SetArgPointee;

namespace extensions {

class ExtensionDownloaderTest : public ExtensionsTest {
 protected:
  ExtensionDownloaderTest() {}

  std::unique_ptr<ManifestFetchData> CreateManifestFetchData(
      const GURL& update_url,
      ManifestFetchData::FetchPriority fetch_priority =
          ManifestFetchData::FetchPriority::BACKGROUND) {
    return std::make_unique<ManifestFetchData>(
        update_url, 0, "", "", ManifestFetchData::PING, fetch_priority);
  }

  void AddFetchDataToDownloader(ExtensionDownloaderTestHelper* helper,
                                std::unique_ptr<ManifestFetchData> fetch) {
    helper->downloader().StartUpdateCheck(std::move(fetch));
  }
};

TEST_F(ExtensionDownloaderTest, TestStageChanges) {
  ExtensionDownloaderTestHelper helper;

  GURL kUpdateUrl("http://localhost/manifest1");
  std::unique_ptr<ManifestFetchData> fetch(CreateManifestFetchData(kUpdateUrl));
  ManifestFetchData::PingData zero_days(0, 0, true, 0);
  fetch->AddExtension("test_app", "1.0", &zero_days, "", std::string(),
                      std::string(),
                      ManifestFetchData::FetchPriority::BACKGROUND);
  GURL fetch_url = fetch->full_url();

  const std::string kManifest =
      "<?xml version='1.0' encoding='UTF-8'?>"
      "<gupdate xmlns='http://www.google.com/update2/response'"
      "                protocol='2.0'>"
      " <app appid='test_app'>"
      "  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx'"
      "               version='1.1' prodversionmin='1.1' />"
      " </app>"
      "</gupdate>";
  helper.test_url_loader_factory().AddResponse(fetch_url.spec(), kManifest,
                                               net::HTTP_OK);
  GURL kCrxUrl("http://example.com/extension_1.2.3.4.crx");
  helper.test_url_loader_factory().AddResponse(kCrxUrl.spec(), "no data",
                                               net::HTTP_OK);

  MockExtensionDownloaderDelegate& delegate = helper.delegate();
  EXPECT_CALL(delegate, IsExtensionPending("test_app")).WillOnce(Return(true));
  Sequence sequence;
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  "test_app", ExtensionDownloaderDelegate::QUEUED_FOR_MANIFEST))
      .Times(AnyNumber());
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  "test_app", ExtensionDownloaderDelegate::QUEUED_FOR_CRX))
      .Times(AnyNumber());
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            "test_app",
                            ExtensionDownloaderDelegate::DOWNLOADING_MANIFEST))
      .InSequence(sequence);
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  "test_app", ExtensionDownloaderDelegate::PARSING_MANIFEST))
      .InSequence(sequence);
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  "test_app", ExtensionDownloaderDelegate::MANIFEST_LOADED))
      .InSequence(sequence);
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  "test_app", ExtensionDownloaderDelegate::DOWNLOADING_CRX))
      .InSequence(sequence);
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            "test_app", ExtensionDownloaderDelegate::FINISHED))
      .InSequence(sequence);

  AddFetchDataToDownloader(&helper, std::move(fetch));

  content::RunAllTasksUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&delegate);
}

TEST_F(ExtensionDownloaderTest, TestStageChangesNoUpdates) {
  ExtensionDownloaderTestHelper helper;

  GURL kUpdateUrl("http://localhost/manifest1");
  std::unique_ptr<ManifestFetchData> fetch(CreateManifestFetchData(kUpdateUrl));
  ManifestFetchData::PingData zero_days(0, 0, true, 0);
  fetch->AddExtension("test_app", "1.0", &zero_days, "", std::string(),
                      std::string(),
                      ManifestFetchData::FetchPriority::BACKGROUND);
  GURL fetch_url = fetch->full_url();

  const std::string kManifest =
      "<?xml version='1.0' encoding='UTF-8'?>"
      "<gupdate xmlns='http://www.google.com/update2/response'"
      "                protocol='2.0'>"
      " <app appid='test_app'>"
      "  <updatecheck codebase='http://example.com/extension_1.2.3.4.crx'"
      "               version='1.1' prodversionmin='1.1' />"
      " </app>"
      "</gupdate>";
  helper.test_url_loader_factory().AddResponse(fetch_url.spec(), kManifest,
                                               net::HTTP_OK);
  GURL kCrxUrl("http://example.com/extension_1.2.3.4.crx");
  helper.test_url_loader_factory().AddResponse(kCrxUrl.spec(), "no data",
                                               net::HTTP_OK);

  MockExtensionDownloaderDelegate& delegate = helper.delegate();
  EXPECT_CALL(delegate, IsExtensionPending("test_app")).WillOnce(Return(false));
  EXPECT_CALL(delegate, GetExtensionExistingVersion("test_app", _))
      .WillOnce(DoAll(SetArgPointee<1>("1.1"), Return(true)));
  Sequence sequence;
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  "test_app", ExtensionDownloaderDelegate::QUEUED_FOR_MANIFEST))
      .Times(AnyNumber());
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            "test_app",
                            ExtensionDownloaderDelegate::DOWNLOADING_MANIFEST))
      .InSequence(sequence);
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  "test_app", ExtensionDownloaderDelegate::PARSING_MANIFEST))
      .InSequence(sequence);
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  "test_app", ExtensionDownloaderDelegate::MANIFEST_LOADED))
      .InSequence(sequence);
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  "test_app", ExtensionDownloaderDelegate::DOWNLOADING_CRX))
      .Times(0);
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            "test_app", ExtensionDownloaderDelegate::FINISHED))
      .InSequence(sequence);

  AddFetchDataToDownloader(&helper, std::move(fetch));

  content::RunAllTasksUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&delegate);
}

TEST_F(ExtensionDownloaderTest, TestStageChangesBadManifest) {
  ExtensionDownloaderTestHelper helper;

  GURL kUpdateUrl("http://localhost/manifest1");
  std::unique_ptr<ManifestFetchData> fetch(CreateManifestFetchData(kUpdateUrl));
  ManifestFetchData::PingData zero_days(0, 0, true, 0);
  fetch->AddExtension("test_app", "1.0", &zero_days, "", std::string(),
                      std::string(),
                      ManifestFetchData::FetchPriority::BACKGROUND);
  GURL fetch_url = fetch->full_url();

  const std::string kManifest = "invalid xml";
  helper.test_url_loader_factory().AddResponse(fetch_url.spec(), kManifest,
                                               net::HTTP_OK);

  MockExtensionDownloaderDelegate& delegate = helper.delegate();
  Sequence sequence;
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  "test_app", ExtensionDownloaderDelegate::QUEUED_FOR_MANIFEST))
      .Times(AnyNumber());
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            "test_app",
                            ExtensionDownloaderDelegate::DOWNLOADING_MANIFEST))
      .InSequence(sequence);
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  "test_app", ExtensionDownloaderDelegate::PARSING_MANIFEST))
      .InSequence(sequence);
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  "test_app", ExtensionDownloaderDelegate::MANIFEST_LOADED))
      .Times(0);
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            "test_app", ExtensionDownloaderDelegate::FINISHED))
      .InSequence(sequence);

  AddFetchDataToDownloader(&helper, std::move(fetch));

  content::RunAllTasksUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&delegate);
}

TEST_F(ExtensionDownloaderTest, TestStageChangesBadQuery) {
  ExtensionDownloaderTestHelper helper;

  GURL kUpdateUrl("http://localhost/manifest1");
  std::unique_ptr<ManifestFetchData> fetch(CreateManifestFetchData(kUpdateUrl));
  ManifestFetchData::PingData zero_days(0, 0, true, 0);
  fetch->AddExtension("test_app", "1.0", &zero_days, "", std::string(),
                      std::string(),
                      ManifestFetchData::FetchPriority::BACKGROUND);
  GURL fetch_url = fetch->full_url();

  helper.test_url_loader_factory().AddResponse(fetch_url.spec(), "",
                                               net::HTTP_BAD_REQUEST);

  MockExtensionDownloaderDelegate& delegate = helper.delegate();
  Sequence sequence;
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  "test_app", ExtensionDownloaderDelegate::QUEUED_FOR_MANIFEST))
      .Times(AnyNumber());
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            "test_app",
                            ExtensionDownloaderDelegate::DOWNLOADING_MANIFEST))
      .InSequence(sequence);
  EXPECT_CALL(delegate,
              OnExtensionDownloadStageChanged(
                  "test_app", ExtensionDownloaderDelegate::PARSING_MANIFEST))
      .Times(0);
  EXPECT_CALL(delegate, OnExtensionDownloadStageChanged(
                            "test_app", ExtensionDownloaderDelegate::FINISHED))
      .InSequence(sequence);

  AddFetchDataToDownloader(&helper, std::move(fetch));

  content::RunAllTasksUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&delegate);
}

}  // namespace extensions
