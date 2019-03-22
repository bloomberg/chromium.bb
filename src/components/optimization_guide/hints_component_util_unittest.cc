// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/hints_component_util.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/version.h"
#include "components/optimization_guide/hints_component_info.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

const base::FilePath::CharType kFileName[] = FILE_PATH_LITERAL("somefile.pb");

class HintsComponentUtilTest : public testing::Test {
 public:
  HintsComponentUtilTest() {}

  ~HintsComponentUtilTest() override {}

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void WriteConfigToFile(const base::FilePath& filePath,
                         const proto::Configuration& config) {
    std::string serialized_config;
    ASSERT_TRUE(config.SerializeToString(&serialized_config));
    ASSERT_EQ(static_cast<int32_t>(serialized_config.length()),
              base::WriteFile(filePath, serialized_config.data(),
                              serialized_config.length()));
  }

  base::FilePath temp_dir() const { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(HintsComponentUtilTest);
};

TEST_F(HintsComponentUtilTest, ProcessHintsComponentInvalidVersion) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<proto::Configuration> config = ProcessHintsComponent(
      HintsComponentInfo(base::Version(""), base::FilePath(kFileName)));

  EXPECT_FALSE(config);
  histogram_tester.ExpectUniqueSample(
      kProcessHintsComponentResultHistogramString,
      static_cast<int>(ProcessHintsComponentResult::FAILED_INVALID_PARAMETERS),
      1);
}

TEST_F(HintsComponentUtilTest, ProcessHintsComponentInvalidPath) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<proto::Configuration> config = ProcessHintsComponent(
      HintsComponentInfo(base::Version("1.0.0.0"), base::FilePath()));

  EXPECT_FALSE(config);
  histogram_tester.ExpectUniqueSample(
      kProcessHintsComponentResultHistogramString,
      static_cast<int>(ProcessHintsComponentResult::FAILED_INVALID_PARAMETERS),
      1);
}

TEST_F(HintsComponentUtilTest, ProcessHintsComponentInvalidFile) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<proto::Configuration> config = ProcessHintsComponent(
      HintsComponentInfo(base::Version("1.0.0"), base::FilePath(kFileName)));

  EXPECT_FALSE(config);
  histogram_tester.ExpectUniqueSample(
      kProcessHintsComponentResultHistogramString,
      static_cast<int>(ProcessHintsComponentResult::FAILED_READING_FILE), 1);
}

TEST_F(HintsComponentUtilTest, ProcessHintsComponentNotAConfigInFile) {
  base::HistogramTester histogram_tester;

  const base::FilePath filePath = temp_dir().Append(kFileName);
  ASSERT_EQ(static_cast<int32_t>(3), base::WriteFile(filePath, "boo", 3));

  std::unique_ptr<proto::Configuration> config = ProcessHintsComponent(
      HintsComponentInfo(base::Version("1.0.0"), filePath));

  EXPECT_FALSE(config);
  histogram_tester.ExpectUniqueSample(
      kProcessHintsComponentResultHistogramString,
      static_cast<int>(
          ProcessHintsComponentResult::FAILED_INVALID_CONFIGURATION),
      1);
}

TEST_F(HintsComponentUtilTest, ProcessHintsComponentSuccess) {
  base::HistogramTester histogram_tester;

  const base::FilePath filePath = temp_dir().Append(kFileName);
  proto::Configuration config;
  proto::Hint* hint = config.add_hints();
  hint->set_key("google.com");
  ASSERT_NO_FATAL_FAILURE(WriteConfigToFile(filePath, config));

  std::unique_ptr<proto::Configuration> processed_config =
      ProcessHintsComponent(
          HintsComponentInfo(base::Version("1.0.0"), filePath));

  ASSERT_TRUE(processed_config);
  EXPECT_EQ(1, processed_config->hints_size());
  EXPECT_EQ("google.com", processed_config->hints()[0].key());
  histogram_tester.ExpectUniqueSample(
      kProcessHintsComponentResultHistogramString,
      static_cast<int>(ProcessHintsComponentResult::SUCCESS), 1);
}

}  // namespace optimization_guide
