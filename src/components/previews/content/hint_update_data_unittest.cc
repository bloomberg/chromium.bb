// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/hint_update_data.h"

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/previews/content/proto/hint_cache.pb.h"
#include "components/previews/core/previews_experiments.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace previews {

namespace {

TEST(HintUpdateDataTest, BuildComponentHintUpdateData) {
  // Verify creating a Component Hint update package.
  base::Version v1("1.2.3.4");
  optimization_guide::proto::Hint hint1;
  hint1.set_key("foo.org");
  hint1.set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint1.add_page_hints();
  page_hint1->set_page_pattern("slowpage");
  optimization_guide::proto::Hint hint2;
  hint2.set_key("bar.com");
  hint2.set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint2 = hint2.add_page_hints();
  page_hint2->set_page_pattern("slowpagealso");

  std::unique_ptr<HintUpdateData> component_update =
      HintUpdateData::CreateComponentHintUpdateData(v1);
  component_update->MoveHintIntoUpdateData(std::move(hint1));
  component_update->MoveHintIntoUpdateData(std::move(hint2));
  EXPECT_TRUE(component_update->component_version().has_value());
  EXPECT_FALSE(component_update->fetch_update_time().has_value());
  EXPECT_EQ(v1, *component_update->component_version());
  // Verify there are 3 store entries: 1 for the metadata entry plus
  // the 2 added hint entries.
  EXPECT_EQ(3ul, component_update->TakeUpdateEntries()->size());
}

TEST(HintUpdateDataTest, BuildFetchUpdateData) {
  // Verify creating a Fetched Hint update package.
  base::Time update_time = base::Time::Now();
  optimization_guide::proto::Hint hint1;
  hint1.set_key("foo.org");
  hint1.set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint1.add_page_hints();
  page_hint1->set_page_pattern("slowpage");

  std::unique_ptr<HintUpdateData> fetch_update =
      HintUpdateData::CreateFetchedHintUpdateData(
          update_time,
          update_time + params::StoredFetchedHintsFreshnessDuration());
  fetch_update->MoveHintIntoUpdateData(std::move(hint1));
  EXPECT_FALSE(fetch_update->component_version().has_value());
  EXPECT_TRUE(fetch_update->fetch_update_time().has_value());
  EXPECT_EQ(update_time, *fetch_update->fetch_update_time());
  // Verify there are 2 store entries: 1 for the metadata entry plus
  // the 1 added hint entries.
  // TODO(dougarnett): Increase expected count once metadata support added.
  EXPECT_EQ(1ul, fetch_update->TakeUpdateEntries()->size());
}

}  // namespace

}  // namespace previews
