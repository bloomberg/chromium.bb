// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/tasks/wait_for_store_initialize_task.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/feed_stream.h"

namespace feed {

WaitForStoreInitializeTask::WaitForStoreInitializeTask(
    FeedStore* store,
    FeedStream* stream,
    base::OnceCallback<void(Result)> callback)
    : store_(*store), stream_(*stream), callback_(std::move(callback)) {}
WaitForStoreInitializeTask::~WaitForStoreInitializeTask() = default;

void WaitForStoreInitializeTask::Run() {
  // |this| stays alive as long as the |store_|, so Unretained is safe.
  store_.Initialize(base::BindOnce(
      &WaitForStoreInitializeTask::OnStoreInitialized, base::Unretained(this)));
}

void WaitForStoreInitializeTask::OnStoreInitialized() {
  store_.ReadStartupData(
      base::BindOnce(&WaitForStoreInitializeTask::ReadStartupDataDone,
                     base::Unretained(this)));
  store_.ReadWebFeedStartupData(
      base::BindOnce(&WaitForStoreInitializeTask::WebFeedStartupDataDone,
                     base::Unretained(this)));
}

void WaitForStoreInitializeTask::ReadStartupDataDone(
    FeedStore::StartupData startup_data) {
  if (startup_data.metadata &&
      startup_data.metadata->gaia() != stream_.GetSyncSignedInGaia()) {
    store_.ClearAll(base::BindOnce(&WaitForStoreInitializeTask::ClearAllDone,
                                   base::Unretained(this)));
    return;
  }
  result_.startup_data = std::move(startup_data);
  MaybeUpgradeStreamSchema();
}

void WaitForStoreInitializeTask::ClearAllDone(bool clear_ok) {
  DLOG_IF(ERROR, !clear_ok) << "FeedStore::ClearAll failed";
  // ClearAll just wiped metadata, so send nullptr.
  MaybeUpgradeStreamSchema();
}

void WaitForStoreInitializeTask::MaybeUpgradeStreamSchema() {
  feedstore::Metadata metadata;
  if (result_.startup_data.metadata)
    metadata = *result_.startup_data.metadata;

  if (metadata.stream_schema_version() != 1) {
    result_.startup_data.stream_data.clear();
    if (metadata.gaia().empty()) {
      metadata.set_gaia(stream_.GetSyncSignedInGaia());
    }
    store_.UpgradeFromStreamSchemaV0(
        std::move(metadata),
        base::BindOnce(&WaitForStoreInitializeTask::UpgradeDone,
                       base::Unretained(this)));
    return;
  }
  Done();
}

void WaitForStoreInitializeTask::UpgradeDone(feedstore::Metadata metadata) {
  result_.startup_data.metadata =
      std::make_unique<feedstore::Metadata>(std::move(metadata));
  Done();
}

void WaitForStoreInitializeTask::WebFeedStartupDataDone(
    FeedStore::WebFeedStartupData data) {
  result_.web_feed_startup_data = std::move(data);
  Done();
}

void WaitForStoreInitializeTask::Done() {
  if (++done_count_ == 2) {
    std::move(callback_).Run(std::move(result_));
    TaskComplete();
  }
}

}  // namespace feed
