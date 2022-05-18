// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/component_updater/url_param_classification_component_installer.h"
#include "chrome/browser/url_param_filter/url_param_classifications_loader.h"
#include "chrome/browser/url_param_filter/url_param_filter_test_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace url_param_filter {

namespace {

enum ShouldFilterState { kUnset, kTrue, kFalse };

std::string ShouldFilterStateToString(ShouldFilterState state) {
  switch (state) {
    case ShouldFilterState::kUnset:
      return "";
    case ShouldFilterState::kTrue:
      return "true";
    case ShouldFilterState::kFalse:
      return "false";
  }
}

}  // namespace

// A base class for other classes to extend.
//
// Several subclasses must be created in order to test the ClassificationsLoader
// since it relies on the kIncognitoParamFilterEnabled base::Feature and the
// UrlParamClassification Component, and these must be instantiated within
// SetUpInProcessBrowserTextFixture() rather than from within a test.
class ClassificationsLoaderBrowserTest : public InProcessBrowserTest {
 public:
  ClassificationsLoaderBrowserTest() = default;

 protected:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    base::CommandLine default_command_line(base::CommandLine::NO_PROGRAM);
    InProcessBrowserTest::SetUpDefaultCommandLine(&default_command_line);
    // Remove this switch to allow components to be updated.
    test_launcher_utils::RemoveCommandLineSwitch(
        default_command_line, switches::kDisableComponentUpdate, command_line);
  }

  void TearDown() override {
    ClassificationsLoader::GetInstance()->ResetListsForTesting();
    component_updater::UrlParamClassificationComponentInstallerPolicy::
        ResetForTesting();
    InProcessBrowserTest::TearDown();
  }

  ClassificationsLoader* loader() {
    return ClassificationsLoader::GetInstance();
  }

  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir component_dir_;
};

// Feature fully disabled, component not installed.
class ClassificationsLoaderFeatureDisabledAndComponentNotInstalled
    : public ClassificationsLoaderBrowserTest {
  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitAndDisableFeature(features::kIncognitoParamFilterEnabled);
    ClassificationsLoaderBrowserTest::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_F(
    ClassificationsLoaderFeatureDisabledAndComponentNotInstalled,
    FeatureDisabled_NoClassificationsLoaded) {
  // ClassificationLoader has no classifications since the feature is
  // disabled.
  EXPECT_THAT(loader()->GetSourceClassifications(), IsEmpty());
  EXPECT_THAT(loader()->GetDestinationClassifications(), IsEmpty());
}

// Feature fully disabled, component installed.
class ClassificationsLoaderFeatureDisabledAndComponentInstalled
    : public ClassificationsLoaderBrowserTest {
  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitAndDisableFeature(features::kIncognitoParamFilterEnabled);
    // Force install the component.
    CHECK(component_dir_.CreateUniqueTempDir());
    base::ScopedAllowBlockingForTesting allow_blocking;

    component_updater::UrlParamClassificationComponentInstallerPolicy::
        WriteComponentForTesting(
            component_dir_.GetPath(),
            CreateSerializedUrlParamFilterClassificationForTesting(
                {{"source.test", {"plzblock_src"}}},
                {{"dest.test", {"plzblock_dest"}}}));
    ClassificationsLoaderBrowserTest::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_F(
    ClassificationsLoaderFeatureDisabledAndComponentInstalled,
    FeatureDisabled_NoClassificationsLoaded) {
  // ClassificationLoader has no classifications since the feature is
  // disabled.
  EXPECT_THAT(loader()->GetSourceClassifications(), IsEmpty());
  EXPECT_THAT(loader()->GetDestinationClassifications(), IsEmpty());
}

// Feature enabled without params, component not installed.
class ClassificationsLoaderFeatureEnabledAndComponentNotInstalled
    : public ClassificationsLoaderBrowserTest {
  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitAndEnableFeature(features::kIncognitoParamFilterEnabled);
    ClassificationsLoaderBrowserTest::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_F(
    ClassificationsLoaderFeatureEnabledAndComponentNotInstalled,
    NeitherSourceProvidesClassifications_NoClassificationsLoaded) {
  // ClassificationLoader has no classifications since neither feature
  // classifications nor Component Updater classifications were provided.
  EXPECT_THAT(loader()->GetSourceClassifications(), IsEmpty());
  EXPECT_THAT(loader()->GetDestinationClassifications(), IsEmpty());
}

// Feature enabled without params, component installed.
class ClassificationsLoaderFeatureEnabledAndComponentInstalled
    : public ClassificationsLoaderBrowserTest {
  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitAndEnableFeature(features::kIncognitoParamFilterEnabled);
    // Force install the component.
    CHECK(component_dir_.CreateUniqueTempDir());
    base::ScopedAllowBlockingForTesting allow_blocking;

    component_updater::UrlParamClassificationComponentInstallerPolicy::
        WriteComponentForTesting(
            component_dir_.GetPath(),
            CreateSerializedUrlParamFilterClassificationForTesting(
                {{"source.test", {"plzblock_src"}}},
                {{"dest.test", {"plzblock_dest"}}}));
    ClassificationsLoaderBrowserTest::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_F(ClassificationsLoaderFeatureEnabledAndComponentInstalled,
                       LoaderUsesComponentClassifications) {
  // Since no feature classifications are provided, the expected
  // classifications should be the component classifications.
  FilterClassification expected_source = MakeFilterClassification(
      "source.test", FilterClassification_SiteRole_SOURCE, {"plzblock_src"});
  FilterClassification expected_dest = MakeFilterClassification(
      "dest.test", FilterClassification_SiteRole_DESTINATION,
      {"plzblock_dest"});
  EXPECT_THAT(
      loader()->GetSourceClassifications(),
      UnorderedElementsAre(Pair("source.test", EqualsProto(expected_source))));
  EXPECT_THAT(
      loader()->GetDestinationClassifications(),
      UnorderedElementsAre(Pair("dest.test", EqualsProto(expected_dest))));
}

// Feature enabled with just "should_filter"= unset/true/false, and the
// component is not installed.
class
    ClassificationsLoaderFeatureEnabledWithShouldFilterAndComponentNotInstalled
    : public ClassificationsLoaderBrowserTest,
      public testing::WithParamInterface<ShouldFilterState> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kIncognitoParamFilterEnabled,
        {{"should_filter", ShouldFilterStateToString(GetParam())}});
    ClassificationsLoaderBrowserTest::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_P(
    ClassificationsLoaderFeatureEnabledWithShouldFilterAndComponentNotInstalled,
    NeitherSourceProvidesClassifications) {
  // ClassificationLoader has no classifications since neither feature
  // classifications nor Component Updater classifications were provided.
  EXPECT_THAT(loader()->GetSourceClassifications(), IsEmpty());
  EXPECT_THAT(loader()->GetDestinationClassifications(), IsEmpty());
}

INSTANTIATE_TEST_CASE_P(
    /* no label */,
    ClassificationsLoaderFeatureEnabledWithShouldFilterAndComponentNotInstalled,
    ::testing::Values(ShouldFilterState::kUnset,
                      ShouldFilterState::kTrue,
                      ShouldFilterState::kFalse));

// Feature enabled with just "should_filter"= unset/true/false, and the
// component is installed.
class ClassificationsLoaderFeatureEnabledWithShouldFilterAndComponentInstalled
    : public ClassificationsLoaderBrowserTest,
      public testing::WithParamInterface<ShouldFilterState> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kIncognitoParamFilterEnabled,
        {{"should_filter", ShouldFilterStateToString(GetParam())}});
    // Force install the component.
    CHECK(component_dir_.CreateUniqueTempDir());
    base::ScopedAllowBlockingForTesting allow_blocking;

    component_updater::UrlParamClassificationComponentInstallerPolicy::
        WriteComponentForTesting(
            component_dir_.GetPath(),
            CreateSerializedUrlParamFilterClassificationForTesting(
                {{"source.test", {"plzblock_src"}}},
                {{"dest.test", {"plzblock_dest"}}}));
    ClassificationsLoaderBrowserTest::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_P(
    ClassificationsLoaderFeatureEnabledWithShouldFilterAndComponentInstalled,
    LoaderUsesComponentClassifications) {
  // Since no feature classifications are provided, the expected
  // classifications should be the component classifications.
  FilterClassification expected_source = MakeFilterClassification(
      "source.test", FilterClassification_SiteRole_SOURCE, {"plzblock_src"});
  FilterClassification expected_dest = MakeFilterClassification(
      "dest.test", FilterClassification_SiteRole_DESTINATION,
      {"plzblock_dest"});
  EXPECT_THAT(
      loader()->GetSourceClassifications(),
      UnorderedElementsAre(Pair("source.test", EqualsProto(expected_source))));
  EXPECT_THAT(
      loader()->GetDestinationClassifications(),
      UnorderedElementsAre(Pair("dest.test", EqualsProto(expected_dest))));
}

INSTANTIATE_TEST_CASE_P(
    /* no label */,
    ClassificationsLoaderFeatureEnabledWithShouldFilterAndComponentInstalled,
    ::testing::Values(ShouldFilterState::kUnset,
                      ShouldFilterState::kTrue,
                      ShouldFilterState::kFalse));

// Feature enabled with just "classifications" param, and the
// component is not installed.
class
    ClassificationsLoaderFeatureEnabledWithClassificationsAndComponentNotInstalled
    : public ClassificationsLoaderBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kIncognitoParamFilterEnabled,
        {{"classifications",
          CreateBase64EncodedFilterParamClassificationForTesting(
              {{"feature-src.test", {"plzblock1"}}},
              {{"feature-dst.test", {"plzblock2"}}})}});
    ClassificationsLoaderBrowserTest::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_F(
    ClassificationsLoaderFeatureEnabledWithClassificationsAndComponentNotInstalled,
    LoaderUsesClassificationsFromFeature) {
  // ClassificationLoader uses the feature parameters
  FilterClassification expected_source = MakeFilterClassification(
      "feature-src.test", FilterClassification_SiteRole_SOURCE, {"plzblock1"});
  FilterClassification expected_dest = MakeFilterClassification(
      "feature-dst.test", FilterClassification_SiteRole_DESTINATION,
      {"plzblock2"});
  EXPECT_THAT(loader()->GetSourceClassifications(),
              UnorderedElementsAre(
                  Pair("feature-src.test", EqualsProto(expected_source))));
  EXPECT_THAT(loader()->GetDestinationClassifications(),
              UnorderedElementsAre(
                  Pair("feature-dst.test", EqualsProto(expected_dest))));
}

// Feature enabled with just "classifications" param, and the component is
// installed.
class
    ClassificationsLoaderFeatureEnabledWithClassificationsAndComponentInstalled
    : public ClassificationsLoaderBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kIncognitoParamFilterEnabled,
        {{"classifications",
          CreateBase64EncodedFilterParamClassificationForTesting(
              {{"feature-src.test", {"plzblock1"}}},
              {{"feature-dst.test", {"plzblock2"}}})}});
    // Force install the component.
    CHECK(component_dir_.CreateUniqueTempDir());
    base::ScopedAllowBlockingForTesting allow_blocking;

    component_updater::UrlParamClassificationComponentInstallerPolicy::
        WriteComponentForTesting(
            component_dir_.GetPath(),
            CreateSerializedUrlParamFilterClassificationForTesting(
                {{"source.test", {"plzblock_src"}}},
                {{"dest.test", {"plzblock_dest"}}}));
    ClassificationsLoaderBrowserTest::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_F(
    ClassificationsLoaderFeatureEnabledWithClassificationsAndComponentInstalled,
    LoaderUsesFeatureClassifications) {
  // Since both feature and component classifications are provided, the feature
  // classifications take precedence.
  FilterClassification expected_source = MakeFilterClassification(
      "feature-src.test", FilterClassification_SiteRole_SOURCE, {"plzblock1"});
  FilterClassification expected_dest = MakeFilterClassification(
      "feature-dst.test", FilterClassification_SiteRole_DESTINATION,
      {"plzblock2"});
  EXPECT_THAT(loader()->GetSourceClassifications(),
              UnorderedElementsAre(
                  Pair("feature-src.test", EqualsProto(expected_source))));
  EXPECT_THAT(loader()->GetDestinationClassifications(),
              UnorderedElementsAre(
                  Pair("feature-dst.test", EqualsProto(expected_dest))));
}

// Feature enabled with "should_filter"= unset/true/false and a classifications
// param, and the component isn't installed.
class ClassificationsLoaderFeatureEnabledWithAllParamsAndComponentNotInstalled
    : public ClassificationsLoaderBrowserTest,
      public testing::WithParamInterface<ShouldFilterState> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kIncognitoParamFilterEnabled,
        {{"should_filter", ShouldFilterStateToString(GetParam())},
         {"classifications",
          CreateBase64EncodedFilterParamClassificationForTesting(
              {{"feature-src.test", {"plzblock1"}}},
              {{"feature-dst.test", {"plzblock2"}}})}});
    ClassificationsLoaderBrowserTest::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_P(
    ClassificationsLoaderFeatureEnabledWithAllParamsAndComponentNotInstalled,
    LoaderUsesFeatureClassifications) {
  // Since both feature and component classifications are provided, the feature
  // classifications take precedence.
  FilterClassification expected_source = MakeFilterClassification(
      "feature-src.test", FilterClassification_SiteRole_SOURCE, {"plzblock1"});
  FilterClassification expected_dest = MakeFilterClassification(
      "feature-dst.test", FilterClassification_SiteRole_DESTINATION,
      {"plzblock2"});
  EXPECT_THAT(loader()->GetSourceClassifications(),
              UnorderedElementsAre(
                  Pair("feature-src.test", EqualsProto(expected_source))));
  EXPECT_THAT(loader()->GetDestinationClassifications(),
              UnorderedElementsAre(
                  Pair("feature-dst.test", EqualsProto(expected_dest))));
}

INSTANTIATE_TEST_CASE_P(
    /* no label */,
    ClassificationsLoaderFeatureEnabledWithAllParamsAndComponentNotInstalled,
    ::testing::Values(ShouldFilterState::kUnset,
                      ShouldFilterState::kTrue,
                      ShouldFilterState::kFalse));

// Feature enabled with both the "should_filter" param unset/true/false and
// "classifications" param, and the component is also installed.
class ClassificationsLoaderFeatureEnabledWithAllParamsAndComponentInstalled
    : public ClassificationsLoaderBrowserTest,
      public testing::WithParamInterface<ShouldFilterState> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kIncognitoParamFilterEnabled,
        {{"should_filter", ShouldFilterStateToString(GetParam())},
         {"classifications",
          CreateBase64EncodedFilterParamClassificationForTesting(
              {{"feature-src.test", {"plzblock1"}}},
              {{"feature-dst.test", {"plzblock2"}}})}});
    // Force install the component.
    CHECK(component_dir_.CreateUniqueTempDir());
    base::ScopedAllowBlockingForTesting allow_blocking;

    component_updater::UrlParamClassificationComponentInstallerPolicy::
        WriteComponentForTesting(
            component_dir_.GetPath(),
            CreateSerializedUrlParamFilterClassificationForTesting(
                {{"source.test", {"plzblock_src"}}},
                {{"dest.test", {"plzblock_dest"}}}));
    ClassificationsLoaderBrowserTest::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_P(
    ClassificationsLoaderFeatureEnabledWithAllParamsAndComponentInstalled,
    LoaderUsesFeatureClassifications) {
  // Since both feature and component classifications are provided, the feature
  // classifications take precedence.
  FilterClassification expected_source = MakeFilterClassification(
      "feature-src.test", FilterClassification_SiteRole_SOURCE, {"plzblock1"});
  FilterClassification expected_dest = MakeFilterClassification(
      "feature-dst.test", FilterClassification_SiteRole_DESTINATION,
      {"plzblock2"});
  EXPECT_THAT(loader()->GetSourceClassifications(),
              UnorderedElementsAre(
                  Pair("feature-src.test", EqualsProto(expected_source))));
  EXPECT_THAT(loader()->GetDestinationClassifications(),
              UnorderedElementsAre(
                  Pair("feature-dst.test", EqualsProto(expected_dest))));
}

INSTANTIATE_TEST_CASE_P(
    /* no label */,
    ClassificationsLoaderFeatureEnabledWithAllParamsAndComponentInstalled,
    ::testing::Values(ShouldFilterState::kUnset,
                      ShouldFilterState::kTrue,
                      ShouldFilterState::kFalse));

}  // namespace url_param_filter
