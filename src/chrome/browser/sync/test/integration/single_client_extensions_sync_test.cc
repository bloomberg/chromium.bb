// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "chrome/browser/sync/test/integration/await_match_status_change_checker.h"
#include "chrome/browser/sync/test/integration/extensions_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/test/fake_server/fake_server.h"

namespace {

using extensions_helper::AllProfilesHaveSameExtensionsAsVerifier;
using extensions_helper::DisableExtension;
using extensions_helper::GetInstalledExtensions;
using extensions_helper::InstallExtension;
using extensions_helper::InstallExtensionForAllProfiles;

class SingleClientExtensionsSyncTest : public SyncTest {
 public:
  SingleClientExtensionsSyncTest() : SyncTest(SINGLE_CLIENT) {}

  ~SingleClientExtensionsSyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientExtensionsSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientExtensionsSyncTest, StartWithNoExtensions) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());
}

// Flaky: https://crbug.com/1030556
IN_PROC_BROWSER_TEST_F(SingleClientExtensionsSyncTest,
                       DISABLED_StartWithSomeExtensions) {
  ASSERT_TRUE(SetupClients());

  const int kNumExtensions = 5;
  for (int i = 0; i < kNumExtensions; ++i) {
    InstallExtension(GetProfile(0), i);
    InstallExtension(verifier(), i);
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());
}

IN_PROC_BROWSER_TEST_F(SingleClientExtensionsSyncTest, InstallSomeExtensions) {
  ASSERT_TRUE(SetupSync());

  const int kNumExtensions = 5;
  for (int i = 0; i < kNumExtensions; ++i) {
    InstallExtension(GetProfile(0), i);
    InstallExtension(verifier(), i);
  }

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());
}

// Helper function for waiting to see the extension count in a profile
// become a specific number.
static bool ExtensionCountCheck(Profile* profile, size_t expected_count) {
  return GetInstalledExtensions(profile).size() == expected_count;
}

// Tests the case of an uninstall from the server conflicting with a local
// modification, which we expect to be resolved in favor of the uninstall.
IN_PROC_BROWSER_TEST_F(SingleClientExtensionsSyncTest, UninstallWinsConflicts) {
  ASSERT_TRUE(SetupClients());

  // Start with an extension installed, and setup sync.
  InstallExtensionForAllProfiles(0);
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameExtensionsAsVerifier());

  // Simulate a delete at the server.
  std::vector<sync_pb::SyncEntity> server_extensions =
      GetFakeServer()->GetSyncEntitiesByModelType(syncer::EXTENSIONS);
  ASSERT_EQ(1ul, server_extensions.size());
  std::unique_ptr<syncer::LoopbackServerEntity> tombstone(
      syncer::PersistentTombstoneEntity::CreateNew(
          server_extensions[0].id_string(),
          server_extensions[0].client_defined_unique_tag()));
  GetFakeServer()->InjectEntity(std::move(tombstone));

  // Modify the extension in the local profile to cause a conflict.
  DisableExtension(GetProfile(0), 0);
  ASSERT_EQ(1u, GetInstalledExtensions(GetProfile(0)).size());

  // Expect the extension to get uninstalled locally.
  AwaitMatchStatusChangeChecker checker(
      base::Bind(&ExtensionCountCheck, GetProfile(0), 0u),
      "Waiting for profile to have no extensions");
  EXPECT_TRUE(checker.Wait());
  EXPECT_TRUE(GetInstalledExtensions(GetProfile(0)).empty());

  // Expect the extension to remain uninstalled at the server.
  server_extensions =
      GetFakeServer()->GetSyncEntitiesByModelType(syncer::EXTENSIONS);
  EXPECT_EQ(0ul, server_extensions.size());
}

}  // namespace
