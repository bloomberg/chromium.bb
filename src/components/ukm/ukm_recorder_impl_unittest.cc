// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_recorder_impl.h"

#include "base/bind.h"
#include "base/metrics/ukm_source_id.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/ukm/report.pb.h"

namespace ukm {

namespace {

class TestUkmRecorderImpl : public UkmRecorderImpl {
 public:
  TestUkmRecorderImpl() {}
  ~TestUkmRecorderImpl() override {}

  void set_sampled_in(bool sampled_in) { sampled_in_ = sampled_in; }

  size_t sampled_callback_count() { return sampled_callback_count_; }

 protected:
  // UkmRecorderImpl:
  bool ShouldRestrictToWhitelistedSourceIds() const override { return false; }
  bool ShouldRestrictToWhitelistedEntries() const override { return false; }
  bool IsSampledIn(int sampling_rate) override {
    ++sampled_callback_count_;
    return sampled_in_;
  }

 private:
  bool sampled_in_ = true;
  size_t sampled_callback_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestUkmRecorderImpl);
};

}  // namespace

class UkmRecorderImplTest : public testing::Test {
 public:
  UkmRecorderImplTest() {
    impl_.StoreWhitelistedEntries();
    impl_.EnableRecording(/*extensions=*/true);
  }

  UkmRecorderImpl& impl() { return impl_; }

  void set_sampled_in(bool sampled_in) { impl_.set_sampled_in(sampled_in); }

  size_t sampled_callback_count() { return impl_.sampled_callback_count(); }

  size_t source_sampling_count() { return impl_.source_event_sampling_.size(); }

  void RecordNavigation(SourceId source_id,
                        const UkmSource::NavigationData& nav_data) {
    impl_.RecordNavigation(source_id, nav_data);
  }

  void AddEntry(mojom::UkmEntryPtr entry) { impl_.AddEntry(std::move(entry)); }

  void StoreRecordingsInReport(Report* report) {
    impl_.StoreRecordingsInReport(report);
  }

 private:
  TestUkmRecorderImpl impl_;

  DISALLOW_COPY_AND_ASSIGN(UkmRecorderImplTest);
};

TEST_F(UkmRecorderImplTest, PageSampling) {
  SourceId page1_source =
      ConvertToSourceId(101, base::UkmSourceId::Type::NAVIGATION_ID);
  UkmSource::NavigationData page1_nav;
  page1_nav.urls.push_back(GURL("https://www.google.com/"));

  // First event always has to do sampled in/out callback.
  RecordNavigation(page1_source, page1_nav);
  AddEntry(builders::PageLoad(page1_source)
               .SetDocumentTiming_NavigationToLoadEventFired(1000)
               .TakeEntry());
  EXPECT_EQ(1U, sampled_callback_count());

  // Second event will use what was already determined.
  AddEntry(builders::PageLoad(page1_source)
               .SetDocumentTiming_NavigationToLoadEventFired(2000)
               .TakeEntry());
  EXPECT_EQ(1U, sampled_callback_count());

  // Different event will again do the callback.
  AddEntry(builders::Memory_Experimental(page1_source)
               .SetCommandBuffer(3000)
               .TakeEntry());
  EXPECT_EQ(2U, sampled_callback_count());

  SourceId page2_source =
      ConvertToSourceId(102, base::UkmSourceId::Type::NAVIGATION_ID);
  UkmSource::NavigationData page2_nav;
  page2_nav.urls.push_back(GURL("https://www.example.com/"));

  // New page will again have to do sampling.
  RecordNavigation(page2_source, page2_nav);
  AddEntry(builders::PageLoad(page2_source)
               .SetDocumentTiming_NavigationToLoadEventFired(1000)
               .TakeEntry());
  EXPECT_EQ(3U, sampled_callback_count());

  // First report won't clear this information.
  EXPECT_EQ(2U, source_sampling_count());
  Report report;
  StoreRecordingsInReport(&report);
  EXPECT_EQ(2U, source_sampling_count());

  // Second report will clear sampling info because they weren't modified.
  StoreRecordingsInReport(&report);
  EXPECT_EQ(0U, source_sampling_count());
}

}  // namespace ukm
