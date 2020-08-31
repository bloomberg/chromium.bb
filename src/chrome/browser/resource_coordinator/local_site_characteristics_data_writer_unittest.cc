// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/local_site_characteristics_data_writer.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_impl.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_unittest_utils.h"
#include "components/performance_manager/persistence/site_data/feature_usage.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace resource_coordinator {

class LocalSiteCharacteristicsDataWriterTest : public ::testing::Test {
 protected:
  // The constructors needs to call 'new' directly rather than using the
  // base::MakeRefCounted helper function because the constructor of
  // LocalSiteCharacteristicsDataImpl is protected and not visible to
  // base::MakeRefCounted.
  LocalSiteCharacteristicsDataWriterTest()
      : test_impl_(
            base::WrapRefCounted(new internal::LocalSiteCharacteristicsDataImpl(
                url::Origin::Create(GURL("foo.com")),
                &delegate_,
                &database_))) {
    LocalSiteCharacteristicsDataWriter* writer =
        new LocalSiteCharacteristicsDataWriter(
            test_impl_.get(), performance_manager::TabVisibility::kBackground);
    writer_ = base::WrapUnique(writer);
  }

  ~LocalSiteCharacteristicsDataWriterTest() override = default;

  bool TabIsLoaded() { return test_impl_->IsLoaded(); }

  bool TabIsLoadedAndInBackground() {
    return test_impl_->loaded_tabs_in_background_count_for_testing() != 0U;
  }

  // The mock delegate used by the LocalSiteCharacteristicsDataImpl objects
  // created by this class, NiceMock is used to avoid having to set
  // expectations in test cases that don't care about this.
  ::testing::NiceMock<
      testing::MockLocalSiteCharacteristicsDataImplOnDestroyDelegate>
      delegate_;

  testing::NoopLocalSiteCharacteristicsDatabase database_;

  // The LocalSiteCharacteristicsDataImpl object used in these tests.
  scoped_refptr<internal::LocalSiteCharacteristicsDataImpl> test_impl_;

  // A LocalSiteCharacteristicsDataWriter object associated with the origin used
  // to create this object.
  std::unique_ptr<LocalSiteCharacteristicsDataWriter> writer_;

  DISALLOW_COPY_AND_ASSIGN(LocalSiteCharacteristicsDataWriterTest);
};

TEST_F(LocalSiteCharacteristicsDataWriterTest, TestModifiers) {
  // Make sure that we initially have no information about any of the features
  // and that the site is in an unloaded state.
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            test_impl_->UpdatesFaviconInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            test_impl_->UpdatesTitleInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            test_impl_->UsesAudioInBackground());

  // Test the OnTabLoaded function.
  EXPECT_FALSE(TabIsLoaded());
  writer_->NotifySiteLoaded();
  EXPECT_TRUE(TabIsLoaded());

  // Test all the modifiers.

  writer_->NotifyUpdatesFaviconInBackground();
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            test_impl_->UpdatesFaviconInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            test_impl_->UpdatesTitleInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            test_impl_->UsesAudioInBackground());

  writer_->NotifyUpdatesTitleInBackground();
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            test_impl_->UpdatesFaviconInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            test_impl_->UpdatesTitleInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureUsageUnknown,
            test_impl_->UsesAudioInBackground());

  writer_->NotifyUsesAudioInBackground();
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            test_impl_->UpdatesFaviconInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            test_impl_->UpdatesTitleInBackground());
  EXPECT_EQ(performance_manager::SiteFeatureUsage::kSiteFeatureInUse,
            test_impl_->UsesAudioInBackground());

  writer_->NotifyLoadTimePerformanceMeasurement(
      base::TimeDelta::FromMicroseconds(202),
      base::TimeDelta::FromMicroseconds(101), 1005);
  EXPECT_EQ(1u, test_impl_->load_duration().num_datums());
  EXPECT_EQ(202.0, test_impl_->load_duration().value());
  EXPECT_EQ(1u, test_impl_->cpu_usage_estimate().num_datums());
  EXPECT_EQ(101.0, test_impl_->cpu_usage_estimate().value());

  EXPECT_EQ(1u, test_impl_->private_footprint_kb_estimate().num_datums());
  EXPECT_EQ(1005.0, test_impl_->private_footprint_kb_estimate().value());

  writer_->NotifySiteUnloaded();
}

TEST_F(LocalSiteCharacteristicsDataWriterTest,
       LoadAndBackgroundStateTransitions) {
  // There's 4 different states a tab can be in:
  //   - Unloaded + Background
  //   - Unloaded + Foreground (might not be possible in practice but this
  //     will depend on the order of the events when an unloaded background tab
  //     gets foregrounded, so it's safer to consider this state).
  //   - Loaded + Background
  //   - Loaded + Foreground
  //
  // Only one of these parameter can change at a time, so you have the following
  // possible transitions:
  //
  //   +-------------+           +-------------+
  //   |Unloaded + Bg|<--------->|Unloaded + Fg|
  //   +-------------+ 1       2 +-------------+
  //         /|\ 3                     /|\ 5
  //          |                         |
  //         \|/ 4                     \|/ 6
  //   +-------------+           +-------------+
  //   | Loaded + Bg |<--------->| Loaded + Fg |
  //   +-------------+ 7       8 +-------------+
  //
  //   - 1,2: There's nothing to do, the tab is already unloaded so |impl_|
  //       shouldn't count it as a background tab anyway.
  //   - 3: The tab gets unloaded while in background, |impl_| should be
  //       notified so it can *decrement* the counter of loaded AND backgrounded
  //       tabs.
  //   - 4: The tab gets loaded while in background, |impl_| should be notified
  //       so it can *increment* the counter of loaded AND backgrounded tabs.
  //   - 5: The tab gets unloaded while in foreground, this should theorically
  //       not happen, but if it does then |impl_| should just be notified about
  //       the unload event so it can update its last loaded timestamp.
  //   - 6: The tab gets loaded while in foreground, |impl_| should only be
  //       notified about the load event, the background state hasn't changed.
  //   - 7: A loaded foreground tab gets backgrounded, |impl_| should be
  //       notified that the tab has been background so it can *increment* the
  //       counter of loaded AND backgrounded tabs.
  //   - 8: A loaded background tab gets foregrounded, |impl_| should be
  //       notified that the tab has been background so it can *decrement* the
  //       counter of loaded AND backgrounded tabs.
  EXPECT_FALSE(TabIsLoaded());

  // Transition #4: Unloaded + Bg -> Loaded + Bg.
  writer_->NotifySiteLoaded();
  EXPECT_TRUE(TabIsLoadedAndInBackground());

  // Transition #8: Loaded + Bg -> Loaded + Fg.
  writer_->NotifySiteVisibilityChanged(
      performance_manager::TabVisibility::kForeground);
  EXPECT_TRUE(TabIsLoaded());
  EXPECT_FALSE(TabIsLoadedAndInBackground());

  // Transition #5: Loaded + Fg -> Unloaded + Fg.
  writer_->NotifySiteUnloaded();
  EXPECT_FALSE(TabIsLoaded());

  // Transition #1: Unloaded + Fg -> Unloaded + Bg.
  writer_->NotifySiteVisibilityChanged(
      performance_manager::TabVisibility::kBackground);
  EXPECT_FALSE(TabIsLoaded());

  // Transition #2: Unloaded + Bg -> Unloaded + Fg.
  writer_->NotifySiteVisibilityChanged(
      performance_manager::TabVisibility::kForeground);
  EXPECT_FALSE(TabIsLoaded());

  // Transition #6: Unloaded + Fg -> Loaded + Fg.
  writer_->NotifySiteLoaded();
  EXPECT_TRUE(TabIsLoaded());
  EXPECT_FALSE(TabIsLoadedAndInBackground());

  // Transition #7: Loaded + Fg -> Loaded + Bg.
  writer_->NotifySiteVisibilityChanged(
      performance_manager::TabVisibility::kBackground);
  EXPECT_TRUE(TabIsLoaded());
  EXPECT_TRUE(TabIsLoadedAndInBackground());

  // Transition #3: Loaded + Bg -> Unloaded + Bg.
  writer_->NotifySiteUnloaded();
  EXPECT_FALSE(TabIsLoaded());
  EXPECT_FALSE(TabIsLoadedAndInBackground());
}

}  // namespace resource_coordinator
