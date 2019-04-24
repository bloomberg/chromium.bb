// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/send_tab_to_self_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace {

class SingleClientSendTabToSelfSyncTest : public SyncTest {
 public:
  SingleClientSendTabToSelfSyncTest() : SyncTest(SINGLE_CLIENT) {
    scoped_list_.InitAndEnableFeature(switches::kSyncSendTabToSelf);
  }

  ~SingleClientSendTabToSelfSyncTest() override {}

 private:
  base::test::ScopedFeatureList scoped_list_;

  DISALLOW_COPY_AND_ASSIGN(SingleClientSendTabToSelfSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientSendTabToSelfSyncTest,
                       DownloadWhenSyncEnabled) {
  const std::string kUrl("https://www.example.com");
  const std::string kGuid("kGuid");

  sync_pb::EntitySpecifics specifics;
  sync_pb::SendTabToSelfSpecifics* send_tab_to_self =
      specifics.mutable_send_tab_to_self();
  send_tab_to_self->set_url(kUrl);
  send_tab_to_self->set_guid(kGuid);

  fake_server_->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          "non_unique_name", kGuid, specifics, /*creation_time=*/0,
          /*last_modified_time=*/0));

  ASSERT_TRUE(SetupSync());

  EXPECT_TRUE(send_tab_to_self_helper::SendTabToSelfUrlChecker(
                  SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0)),
                  GURL(kUrl))
                  .Wait());
}

}  // namespace
