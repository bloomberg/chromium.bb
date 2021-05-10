// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/translate/translate_model_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/translate/core/common/translate_util.h"
#include "components/translate/core/language_detection/language_detection_model.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Fetch and calculate the total number of samples from all the bins for
// |histogram_name|. Note: from some browertests run (such as chromeos) there
// might be two profiles created, and this will return the total sample count
// across profiles.
int GetTotalHistogramSamples(const base::HistogramTester* histogram_tester,
                             const std::string& histogram_name) {
  std::vector<base::Bucket> buckets =
      histogram_tester->GetAllSamples(histogram_name);
  int total = 0;
  for (const auto& bucket : buckets)
    total += bucket.count;

  return total;
}

// Retries fetching |histogram_name| until it contains at least |count| samples.
int RetryForHistogramUntilCountReached(
    const base::HistogramTester* histogram_tester,
    const std::string& histogram_name,
    int count) {
  while (true) {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();

    int total = GetTotalHistogramSamples(histogram_tester, histogram_name);
    if (total >= count)
      return total;

    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    base::RunLoop().RunUntilIdle();
  }
}

}  // namespace

using TranslateModelServiceDisabledBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(TranslateModelServiceDisabledBrowserTest,
                       TranslateModelServiceDisabled) {
  EXPECT_FALSE(TranslateModelServiceFactory::GetOrBuildForKey(
      browser()->profile()->GetProfileKey()));
}

class TranslateModelServiceWithoutOptimizationGuideBrowserTest
    : public TranslateModelServiceDisabledBrowserTest {
 public:
  TranslateModelServiceWithoutOptimizationGuideBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {translate::kTFLiteLanguageDetectionEnabled}, {});
  }

  ~TranslateModelServiceWithoutOptimizationGuideBrowserTest() override =
      default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This test confirms the translate model service is not available if
// the optimization guide does not exist.
IN_PROC_BROWSER_TEST_F(TranslateModelServiceWithoutOptimizationGuideBrowserTest,
                       TranslateModelServiceEnabled) {
  EXPECT_FALSE(TranslateModelServiceFactory::GetOrBuildForKey(
      browser()->profile()->GetProfileKey()));
}

IN_PROC_BROWSER_TEST_F(TranslateModelServiceDisabledBrowserTest,
                       LanguageDetectionModelNotCreated) {
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), GURL("https://test.com"));
  RetryForHistogramUntilCountReached(
      &histogram_tester, "Translate.CLD3.TopLanguageEvaluationDuration", 1);
  histogram_tester.ExpectTotalCount(
      "LanguageDetection.TFLiteModel.WasModelAvailableForDetection", 0);
}

class TranslateModelServiceBrowserTest
    : public TranslateModelServiceDisabledBrowserTest {
 public:
  TranslateModelServiceBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {translate::kTFLiteLanguageDetectionEnabled,
         optimization_guide::features::kOptimizationHints,
         optimization_guide::features::kRemoteOptimizationGuideFetching},
        {});
  }

  void SetUp() override {
    origin_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    origin_server_->ServeFilesFromSourceDirectory("chrome/test/data/translate");
    ASSERT_TRUE(origin_server_->Start());
    english_url_ = origin_server_->GetURL("/english_page.html");
    InProcessBrowserTest::SetUp();
  }

  ~TranslateModelServiceBrowserTest() override = default;

  translate::TranslateModelService* translate_model_service() {
    return TranslateModelServiceFactory::GetOrBuildForKey(
        browser()->profile()->GetProfileKey());
  }

  const GURL& english_url() const { return english_url_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  GURL english_url_;
  std::unique_ptr<net::EmbeddedTestServer> origin_server_;
};

base::FilePath model_file_path() {
  base::FilePath source_root_dir;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
  return source_root_dir.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("translate")
      .AppendASCII("valid_model.tflite");
}

IN_PROC_BROWSER_TEST_F(TranslateModelServiceBrowserTest,
                       TranslateModelServiceEnabled) {
  EXPECT_TRUE(translate_model_service());
}

IN_PROC_BROWSER_TEST_F(TranslateModelServiceBrowserTest,
                       TranslateModelServiceEnabled_OffTheRecord) {
  EXPECT_TRUE(TranslateModelServiceFactory::GetOrBuildForKey(
      browser()->profile()->GetPrimaryOTRProfile()->GetProfileKey()));
}

IN_PROC_BROWSER_TEST_F(TranslateModelServiceBrowserTest,
                       LanguageDetectionModelReadyOnRequest) {
  base::ScopedAllowBlockingForTesting allow_io_for_test_setup;
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(translate_model_service());

  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->OverrideTargetModelFileForTesting(
          optimization_guide::proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION,
          /*model_metadata=*/base::nullopt, model_file_path());

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "TranslateModelService.LanguageDetectionModel.WasLoaded", 1);
  histogram_tester.ExpectUniqueSample(
      "TranslateModelService.LanguageDetectionModel.WasLoaded", true, 1);

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  translate_model_service()->GetLanguageDetectionModelFile(base::BindOnce(
      [](base::RunLoop* run_loop, base::File model_file) {
        EXPECT_TRUE(model_file.IsValid());
        run_loop->Quit();
      },
      run_loop.get()));

  run_loop->Run();
}

IN_PROC_BROWSER_TEST_F(TranslateModelServiceBrowserTest,
                       LanguageDetectionModelLoadedAfterRequest) {
  base::ScopedAllowBlockingForTesting allow_io_for_test_setup;
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(translate_model_service());
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  translate_model_service()->GetLanguageDetectionModelFile(base::BindOnce(
      [](base::RunLoop* run_loop, base::File model_file) {
        EXPECT_TRUE(model_file.IsValid());
        run_loop->Quit();
      },
      run_loop.get()));

  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->OverrideTargetModelFileForTesting(
          optimization_guide::proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION,
          /*model_metadata=*/base::nullopt, model_file_path());

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "TranslateModelService.LanguageDetectionModel.WasLoaded", 1);
  histogram_tester.ExpectUniqueSample(
      "TranslateModelService.LanguageDetectionModel.WasLoaded", true, 1);
  run_loop->Run();
}

IN_PROC_BROWSER_TEST_F(TranslateModelServiceBrowserTest,
                       InvalidModelWhenLoading) {
  base::ScopedAllowBlockingForTesting allow_io_for_test_setup;
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(translate_model_service());
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->OverrideTargetModelFileForTesting(
          optimization_guide::proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION,
          /*model_metadata=*/base::nullopt, base::FilePath());

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "TranslateModelService.LanguageDetectionModel.WasLoaded", 1);
  histogram_tester.ExpectUniqueSample(
      "TranslateModelService.LanguageDetectionModel.WasLoaded", false, 1);
}

IN_PROC_BROWSER_TEST_F(TranslateModelServiceBrowserTest,
                       LanguageDetectionModelCreated) {
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), english_url());
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "LanguageDetection.TFLiteModel.WasModelAvailableForDetection", 1);
  histogram_tester.ExpectUniqueSample(
      "LanguageDetection.TFLiteModel.WasModelAvailableForDetection", false, 1);
}

// Disabled due to flake: https://crbug.com/1177331
#if defined(OS_MAC)
#define MAYBE_LanguageDetectionModelAvailableForDetection \
  DISABLED_LanguageDetectionModelAvailableForDetection
#else
#define MAYBE_LanguageDetectionModelAvailableForDetection \
  LanguageDetectionModelAvailableForDetection
#endif

IN_PROC_BROWSER_TEST_F(TranslateModelServiceBrowserTest,
                       MAYBE_LanguageDetectionModelAvailableForDetection) {
  base::HistogramTester histogram_tester;
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->OverrideTargetModelFileForTesting(
          optimization_guide::proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION,
          /*model_metadata=*/base::nullopt, model_file_path());
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "LanguageDetection.TFLiteModel.LanguageDetectionModelState", 1);
  histogram_tester.ExpectUniqueSample(
      "LanguageDetection.TFLiteModel.LanguageDetectionModelState",
      translate::LanguageDetectionModelState::kModelFileValidAndMemoryMapped,
      1);

  ui_test_utils::NavigateToURL(browser(), english_url());
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "LanguageDetection.TFLiteModel.WasModelAvailableForDetection", 1);
  histogram_tester.ExpectBucketCount(
      "LanguageDetection.TFLiteModel.WasModelAvailableForDetection", true, 1);
}
