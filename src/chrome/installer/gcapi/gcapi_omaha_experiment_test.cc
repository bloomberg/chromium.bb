// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/gcapi/gcapi_omaha_experiment.h"

#include <stdint.h>

#include "base/test/test_reg_util_win.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/gcapi/gcapi.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const wchar_t kBrand[] = L"ABCD";

const wchar_t kSomeExperiments[] = L"myexp=1|Aug 2;yourexp=2|Sep 5";
const wchar_t kSomeOtherExperiments[] = L"anotherexp=joe|Jun 7 2008";
const wchar_t kSomeMoreExperiments[] = L"moreexp=foo|Jul 31 1999";

// The boolean parameter is true for system-level tests, or false for user-level
// tests.
class GCAPIOmahaExperimentTest : public ::testing::TestWithParam<bool> {
 protected:
  GCAPIOmahaExperimentTest()
      : system_level_(GetParam()),
        shell_mode_(system_level_ ? GCAPI_INVOKED_UAC_ELEVATION
                                  : GCAPI_INVOKED_STANDARD_SHELL),
        scoped_install_details_(system_level_),
        reactivation_label_(gcapi_internals::GetGCAPIExperimentLabel(
            kBrand,
            gcapi_internals::kReactivationLabel)),
        relaunch_label_(gcapi_internals::GetGCAPIExperimentLabel(
            kBrand,
            gcapi_internals::kRelaunchLabel)) {}

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
    ASSERT_NO_FATAL_FAILURE(
        override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE));
  }

  void VerifyExperimentLabels(const base::string16& expected_labels) {
    base::string16 actual_labels;
    EXPECT_TRUE(GoogleUpdateSettings::ReadExperimentLabels(&actual_labels));
    EXPECT_EQ(expected_labels, actual_labels);
  }

  int shell_mode() const { return shell_mode_; }

  const bool system_level_;
  const int shell_mode_;
  const install_static::ScopedInstallDetails scoped_install_details_;
  registry_util::RegistryOverrideManager override_manager_;
  base::string16 reactivation_label_;
  base::string16 relaunch_label_;
};

TEST_P(GCAPIOmahaExperimentTest, SetReactivationLabelFromEmptyExperiments) {
  ASSERT_TRUE(SetReactivationExperimentLabels(kBrand, shell_mode()));
  VerifyExperimentLabels(reactivation_label_);
}

// Test the relaunch label once; all other tests go more in depth, but since
// both labels use the same logic underneath there is no need to test both in
// depth.
TEST_P(GCAPIOmahaExperimentTest, SetRelaunchLabelFromEmptyExperiments) {
  ASSERT_TRUE(SetRelaunchExperimentLabels(kBrand, shell_mode()));
  VerifyExperimentLabels(relaunch_label_);
}

TEST_P(GCAPIOmahaExperimentTest, SetReactivationLabelWithExistingExperiments) {
  GoogleUpdateSettings::SetExperimentLabels(kSomeExperiments);

  ASSERT_TRUE(SetReactivationExperimentLabels(kBrand, shell_mode()));

  base::string16 expected_labels(kSomeExperiments);
  expected_labels += kExperimentLabelSeparator;
  expected_labels.append(reactivation_label_);
  VerifyExperimentLabels(expected_labels);
}

TEST_P(GCAPIOmahaExperimentTest,
       SetReactivationLabelWithExistingIdenticalExperiment) {
  base::string16 previous_labels(kSomeExperiments);
  previous_labels += kExperimentLabelSeparator;
  previous_labels.append(reactivation_label_);
  previous_labels += kExperimentLabelSeparator;
  previous_labels.append(kSomeOtherExperiments);
  GoogleUpdateSettings::SetExperimentLabels(previous_labels);

  ASSERT_TRUE(SetReactivationExperimentLabels(kBrand, shell_mode()));

  base::string16 expected_labels(kSomeExperiments);
  expected_labels += kExperimentLabelSeparator;
  expected_labels.append(kSomeOtherExperiments);
  expected_labels += kExperimentLabelSeparator;
  expected_labels.append(reactivation_label_);
  VerifyExperimentLabels(expected_labels);
}

TEST_P(GCAPIOmahaExperimentTest,
       SetReactivationLabelWithExistingIdenticalAtBeginning) {
  base::string16 previous_labels(reactivation_label_);
  previous_labels += kExperimentLabelSeparator;
  previous_labels.append(kSomeExperiments);
  GoogleUpdateSettings::SetExperimentLabels(previous_labels);

  ASSERT_TRUE(SetReactivationExperimentLabels(kBrand, shell_mode()));

  base::string16 expected_labels(kSomeExperiments);
  expected_labels += kExperimentLabelSeparator;
  expected_labels.append(reactivation_label_);
  VerifyExperimentLabels(expected_labels);
}

TEST_P(GCAPIOmahaExperimentTest,
       SetReactivationLabelWithFakeMatchInAnExperiment) {
  base::string16 previous_labels(kSomeExperiments);
  previous_labels += kExperimentLabelSeparator;
  previous_labels.append(L"blah_");
  // Shouldn't match deletion criteria.
  previous_labels.append(reactivation_label_);
  previous_labels += kExperimentLabelSeparator;
  previous_labels.append(kSomeOtherExperiments);
  previous_labels += kExperimentLabelSeparator;
  // Should match the deletion criteria.
  previous_labels.append(reactivation_label_);
  previous_labels += kExperimentLabelSeparator;
  previous_labels.append(kSomeMoreExperiments);
  GoogleUpdateSettings::SetExperimentLabels(previous_labels);

  ASSERT_TRUE(SetReactivationExperimentLabels(kBrand, shell_mode()));

  base::string16 expected_labels(kSomeExperiments);
  expected_labels += kExperimentLabelSeparator;
  expected_labels.append(L"blah_");
  expected_labels.append(reactivation_label_);
  expected_labels += kExperimentLabelSeparator;
  expected_labels.append(kSomeOtherExperiments);
  expected_labels += kExperimentLabelSeparator;
  expected_labels.append(kSomeMoreExperiments);
  expected_labels += kExperimentLabelSeparator;
  expected_labels.append(reactivation_label_);
  VerifyExperimentLabels(expected_labels);
}

TEST_P(GCAPIOmahaExperimentTest,
       SetReactivationLabelWithFakeMatchInAnExperimentAndNoRealMatch) {
  base::string16 previous_labels(kSomeExperiments);
  previous_labels += kExperimentLabelSeparator;
  previous_labels.append(L"blah_");
  // Shouldn't match deletion criteria.
  previous_labels.append(reactivation_label_);
  previous_labels += kExperimentLabelSeparator;
  previous_labels.append(kSomeOtherExperiments);
  GoogleUpdateSettings::SetExperimentLabels(previous_labels);

  ASSERT_TRUE(SetReactivationExperimentLabels(kBrand, shell_mode()));

  base::string16 expected_labels(kSomeExperiments);
  expected_labels += kExperimentLabelSeparator;
  expected_labels.append(L"blah_");
  expected_labels.append(reactivation_label_);
  expected_labels += kExperimentLabelSeparator;
  expected_labels.append(kSomeOtherExperiments);
  expected_labels += kExperimentLabelSeparator;
  expected_labels.append(reactivation_label_);
  VerifyExperimentLabels(expected_labels);
}

TEST_P(GCAPIOmahaExperimentTest,
       SetReactivationLabelWithExistingEntryWithLabelAsPrefix) {
  base::string16 previous_labels(kSomeExperiments);
  previous_labels += kExperimentLabelSeparator;
  // Append prefix matching the label, but not followed by '='.
  previous_labels.append(gcapi_internals::kReactivationLabel);
  // Shouldn't match deletion criteria.
  previous_labels.append(kSomeOtherExperiments);
  GoogleUpdateSettings::SetExperimentLabels(previous_labels);

  ASSERT_TRUE(SetReactivationExperimentLabels(kBrand, shell_mode()));

  base::string16 expected_labels(kSomeExperiments);
  expected_labels += kExperimentLabelSeparator;
  expected_labels.append(gcapi_internals::kReactivationLabel);
  expected_labels.append(kSomeOtherExperiments);
  expected_labels += kExperimentLabelSeparator;
  expected_labels.append(reactivation_label_);
  VerifyExperimentLabels(expected_labels);
}

TEST_P(GCAPIOmahaExperimentTest, BuildExperimentDateString) {
  // Sat, 1 Jan 2000 00:00:00 UTC
  base::Time::Exploded kTestTimeExploded = {2000, 1, 6, 1, 0, 0, 0, 0};
  base::Time kTestTime;
  EXPECT_TRUE(base::Time::FromUTCExploded(kTestTimeExploded, &kTestTime));
  EXPECT_EQ(STRING16_LITERAL("Sat, 01 Jan 2001 00:00:00 GMT"),
            BuildExperimentDateString(kTestTime));
  EXPECT_EQ(STRING16_LITERAL("Sat, 01 Jan 2001 00:00:00 GMT"),
            BuildExperimentDateString(kTestTime +
                                      base::TimeDelta::FromMilliseconds(1)));
  EXPECT_EQ(
      STRING16_LITERAL("Sat, 01 Jan 2001 00:00:01 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromSeconds(1)));
  EXPECT_EQ(
      STRING16_LITERAL("Sat, 01 Jan 2001 00:00:59 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromSeconds(59)));
  EXPECT_EQ(
      STRING16_LITERAL("Sat, 01 Jan 2001 00:01:00 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromSeconds(60)));
  EXPECT_EQ(
      STRING16_LITERAL("Sat, 01 Jan 2001 00:01:00 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromMinutes(1)));
  EXPECT_EQ(
      STRING16_LITERAL("Sat, 01 Jan 2001 00:59:00 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromMinutes(59)));
  EXPECT_EQ(
      STRING16_LITERAL("Sat, 01 Jan 2001 01:00:00 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromMinutes(60)));
  EXPECT_EQ(
      STRING16_LITERAL("Sat, 01 Jan 2001 01:00:00 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromHours(1)));
  EXPECT_EQ(
      STRING16_LITERAL("Sat, 01 Jan 2001 23:00:00 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromHours(23)));
  EXPECT_EQ(
      STRING16_LITERAL("Sun, 02 Jan 2001 00:00:00 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromHours(24)));
  EXPECT_EQ(
      STRING16_LITERAL("Sun, 02 Jan 2001 00:00:00 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromDays(1)));
  EXPECT_EQ(
      STRING16_LITERAL("Mon, 03 Jan 2001 00:00:00 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromDays(2)));
  EXPECT_EQ(
      STRING16_LITERAL("Tue, 04 Jan 2001 00:00:00 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromDays(3)));
  EXPECT_EQ(
      STRING16_LITERAL("Wed, 05 Jan 2001 00:00:00 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromDays(4)));
  EXPECT_EQ(
      STRING16_LITERAL("Thu, 06 Jan 2001 00:00:00 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromDays(5)));
  EXPECT_EQ(
      STRING16_LITERAL("Fri, 07 Jan 2001 00:00:00 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromDays(6)));
  EXPECT_EQ(
      STRING16_LITERAL("Sat, 08 Jan 2001 00:00:00 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromDays(7)));
  EXPECT_EQ(
      STRING16_LITERAL("Mon, 31 Jan 2001 00:00:00 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromDays(30)));
  EXPECT_EQ(
      STRING16_LITERAL("Tue, 01 Feb 2001 00:00:00 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromDays(31)));
  EXPECT_EQ(
      STRING16_LITERAL("Wed, 01 Mar 2001 00:00:00 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromDays(60)));
  EXPECT_EQ(
      STRING16_LITERAL("Sat, 01 Apr 2001 00:00:00 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromDays(91)));
  EXPECT_EQ(
      STRING16_LITERAL("Mon, 01 May 2001 00:00:00 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromDays(121)));
  EXPECT_EQ(
      STRING16_LITERAL("Thu, 01 Jun 2001 00:00:00 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromDays(152)));
  EXPECT_EQ(
      STRING16_LITERAL("Sat, 01 Jul 2001 00:00:00 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromDays(182)));
  EXPECT_EQ(
      STRING16_LITERAL("Tue, 01 Aug 2001 00:00:00 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromDays(213)));
  EXPECT_EQ(
      STRING16_LITERAL("Fri, 01 Sep 2001 00:00:00 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromDays(244)));
  EXPECT_EQ(
      STRING16_LITERAL("Sun, 01 Oct 2001 00:00:00 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromDays(274)));
  EXPECT_EQ(
      STRING16_LITERAL("Wed, 01 Nov 2001 00:00:00 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromDays(305)));
  EXPECT_EQ(
      STRING16_LITERAL("Fri, 01 Dec 2001 00:00:00 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromDays(335)));
  EXPECT_EQ(
      STRING16_LITERAL("Mon, 01 Jan 2002 00:00:00 GMT"),
      BuildExperimentDateString(kTestTime + base::TimeDelta::FromDays(366)));
}

INSTANTIATE_TEST_SUITE_P(All, GCAPIOmahaExperimentTest, ::testing::Bool());

}  // namespace
