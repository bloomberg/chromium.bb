// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/feature_toggler.h"
#include "chrome/browser/sync/test/integration/sync_arc_package_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/browser/ui/app_list/arc/arc_package_syncable_service.h"
#include "components/sync/driver/sync_driver_switches.h"

namespace arc {

namespace {

bool AllProfilesHaveSameArcPackageDetails() {
  return SyncArcPackageHelper::GetInstance()
      ->AllProfilesHaveSamePackageDetails();
}

}  // namespace

class SingleClientArcPackageSyncTest : public FeatureToggler, public SyncTest {
 public:
  SingleClientArcPackageSyncTest()
      : FeatureToggler(switches::kSyncPseudoUSSArcPackage),
        SyncTest(SINGLE_CLIENT) {}

  ~SingleClientArcPackageSyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientArcPackageSyncTest);
};

IN_PROC_BROWSER_TEST_P(SingleClientArcPackageSyncTest, ArcPackageEmpty) {
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AllProfilesHaveSameArcPackageDetails());
}

IN_PROC_BROWSER_TEST_P(SingleClientArcPackageSyncTest,
                       ArcPackageInstallSomePackages) {
  ASSERT_TRUE(SetupSync());

  constexpr size_t kNumPackages = 5;
  for (size_t i = 0; i < kNumPackages; ++i) {
    sync_arc_helper()->InstallPackageWithIndex(GetProfile(0), i);
    sync_arc_helper()->InstallPackageWithIndex(verifier(), i);
  }

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(AllProfilesHaveSameArcPackageDetails());
}

INSTANTIATE_TEST_SUITE_P(USS,
                         SingleClientArcPackageSyncTest,
                         ::testing::Values(false, true));

}  // namespace arc
