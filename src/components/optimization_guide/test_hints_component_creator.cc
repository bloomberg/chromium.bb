// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/test_hints_component_creator.h"

#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/version.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace testing {

TestHintsComponentCreator::TestHintsComponentCreator()
    : scoped_temp_dir_(std::make_unique<base::ScopedTempDir>()),
      next_component_version_(1) {}

TestHintsComponentCreator::~TestHintsComponentCreator() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  scoped_temp_dir_.reset();
}

optimization_guide::HintsComponentInfo
TestHintsComponentCreator::CreateHintsComponentInfoWithPageHints(
    optimization_guide::proto::OptimizationType optimization_type,
    const std::vector<std::string>& page_hint_host_suffixes,
    const std::vector<std::string>& resource_blocking_patterns) {
  optimization_guide::proto::Configuration config;
  for (const auto& page_hint_site : page_hint_host_suffixes) {
    optimization_guide::proto::Hint* hint = config.add_hints();
    hint->set_key(page_hint_site);
    hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);

    optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
    page_hint->set_page_pattern("*");

    optimization_guide::proto::Optimization* optimization =
        page_hint->add_whitelisted_optimizations();
    optimization->set_optimization_type(optimization_type);

    for (auto resource_blocking_pattern : resource_blocking_patterns) {
      optimization_guide::proto::ResourceLoadingHint* resource_loading_hint =
          optimization->add_resource_loading_hints();
      resource_loading_hint->set_loading_optimization_type(
          optimization_guide::proto::LOADING_BLOCK_RESOURCE);
      resource_loading_hint->set_resource_pattern(resource_blocking_pattern);
    }
  }

  return WriteConfigToFileAndReturnHintsComponentInfo(config);
}

optimization_guide::HintsComponentInfo
TestHintsComponentCreator::CreateHintsComponentInfoWithExperimentalPageHints(
    optimization_guide::proto::OptimizationType optimization_type,
    const std::vector<std::string>& page_hint_host_suffixes,
    const std::vector<std::string>& experimental_resource_patterns) {
  optimization_guide::proto::Configuration config;
  for (const auto& page_hint_site : page_hint_host_suffixes) {
    optimization_guide::proto::Hint* hint = config.add_hints();
    hint->set_key(page_hint_site);
    hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);

    optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
    page_hint->set_page_pattern("*");

    optimization_guide::proto::Optimization* optimization =
        page_hint->add_whitelisted_optimizations();
    optimization->set_optimization_type(optimization_type);
    optimization->set_experiment_name(kFooExperimentName);

    for (auto resource_blocking_pattern : experimental_resource_patterns) {
      optimization_guide::proto::ResourceLoadingHint* resource_loading_hint =
          optimization->add_resource_loading_hints();
      resource_loading_hint->set_loading_optimization_type(
          optimization_guide::proto::LOADING_BLOCK_RESOURCE);
      resource_loading_hint->set_resource_pattern(resource_blocking_pattern);
    }
  }

  return WriteConfigToFileAndReturnHintsComponentInfo(config);
}

optimization_guide::HintsComponentInfo
TestHintsComponentCreator::CreateHintsComponentInfoWithMixPageHints(
    optimization_guide::proto::OptimizationType optimization_type,
    const std::vector<std::string>& page_hint_host_suffixes,
    const std::vector<std::string>& experimental_resource_patterns,
    const std::vector<std::string>& default_resource_patterns) {
  optimization_guide::proto::Configuration config;
  for (const auto& page_hint_site : page_hint_host_suffixes) {
    optimization_guide::proto::Hint* hint = config.add_hints();
    hint->set_key(page_hint_site);
    hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);

    optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
    page_hint->set_page_pattern("*");

    // Add experimental patterns first so they get higher priority.
    {
      optimization_guide::proto::Optimization* optimization =
          page_hint->add_whitelisted_optimizations();
      optimization->set_optimization_type(optimization_type);
      optimization->set_experiment_name(kFooExperimentName);

      for (auto resource_blocking_pattern : experimental_resource_patterns) {
        optimization_guide::proto::ResourceLoadingHint* resource_loading_hint =
            optimization->add_resource_loading_hints();
        resource_loading_hint->set_loading_optimization_type(
            optimization_guide::proto::LOADING_BLOCK_RESOURCE);
        resource_loading_hint->set_resource_pattern(resource_blocking_pattern);
      }
    }

    {
      optimization_guide::proto::Optimization* optimization =
          page_hint->add_whitelisted_optimizations();
      optimization->set_optimization_type(optimization_type);

      for (auto resource_blocking_pattern : default_resource_patterns) {
        optimization_guide::proto::ResourceLoadingHint* resource_loading_hint =
            optimization->add_resource_loading_hints();
        resource_loading_hint->set_loading_optimization_type(
            optimization_guide::proto::LOADING_BLOCK_RESOURCE);
        resource_loading_hint->set_resource_pattern(resource_blocking_pattern);
      }
    }
  }

  return WriteConfigToFileAndReturnHintsComponentInfo(config);
}

base::FilePath TestHintsComponentCreator::GetFilePath(
    std::string file_path_suffix) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(scoped_temp_dir_->IsValid() ||
              scoped_temp_dir_->CreateUniqueTempDir());
  return scoped_temp_dir_->GetPath().AppendASCII(file_path_suffix);
}

void TestHintsComponentCreator::WriteConfigToFile(
    const base::FilePath& file_path,
    const optimization_guide::proto::Configuration& config) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  std::string serialized_config;
  ASSERT_TRUE(config.SerializeToString(&serialized_config));

  ASSERT_EQ(static_cast<int32_t>(serialized_config.length()),
            base::WriteFile(file_path, serialized_config.data(),
                            serialized_config.length()));
}

optimization_guide::HintsComponentInfo
TestHintsComponentCreator::WriteConfigToFileAndReturnHintsComponentInfo(
    const optimization_guide::proto::Configuration& config) {
  std::string version_string = base::NumberToString(next_component_version_++);
  base::FilePath file_path = GetFilePath(version_string);
  WriteConfigToFile(file_path, config);
  return optimization_guide::HintsComponentInfo(base::Version(version_string),
                                                file_path);
}

}  // namespace testing
}  // namespace optimization_guide
