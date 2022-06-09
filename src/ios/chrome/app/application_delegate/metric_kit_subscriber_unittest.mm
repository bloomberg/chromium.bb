// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/metric_kit_subscriber.h"

#import <Foundation/Foundation.h>
#import <MetricKit/MetricKit.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/ios/ios_util.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/ios/wait_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "ios/chrome/app/application_delegate/mock_metrickit_metric_payload.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
base::FilePath MetricKitReportDirectory() {
  NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                                       NSUserDomainMask, YES);
  NSString* documents_directory = [paths objectAtIndex:0];
  NSString* metric_kit_report_directory = [documents_directory
      stringByAppendingPathComponent:kChromeMetricKitPayloadsDirectory];

  return base::FilePath(base::SysNSStringToUTF8(metric_kit_report_directory));
}

NSString* const kEnableMetricKit = @"EnableMetricKit";
}

class MetricKitSubscriberTest : public PlatformTest {
 public:
  MetricKitSubscriberTest() {
    base::DeletePathRecursively(MetricKitReportDirectory());
    NSUserDefaults* standard_defaults = [NSUserDefaults standardUserDefaults];
    [standard_defaults setBool:YES forKey:kEnableMetricKit];
  }

  ~MetricKitSubscriberTest() override {
    base::DeletePathRecursively(MetricKitReportDirectory());
    NSUserDefaults* standard_defaults = [NSUserDefaults standardUserDefaults];
    [standard_defaults removeObjectForKey:kEnableMetricKit];
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(MetricKitSubscriberTest, Metrics) {
  base::HistogramTester tester;
  MXMetricPayload* mock_report = MockMetricPayload(@{
    @"applicationTimeMetrics" :
        @{@"cumulativeForegroundTime" : @1, @"cumulativeBackgroundTime" : @2},
    @"memoryMetrics" :
        @{@"peakMemoryUsage" : @3, @"averageSuspendedMemory" : @4},
    @"applicationLaunchMetrics" : @{
      @"histogrammedResumeTime" : @{@25 : @5, @35 : @7},
      @"histogrammedTimeToFirstDrawKey" : @{@5 : @2, @15 : @4}
    },
    @"applicationExitMetrics" : @{
      @"backgroundExitData" : @{
        @"cumulativeAppWatchdogExitCount" : @1,
        @"cumulativeMemoryResourceLimitExitCount" : @2,
        // These two entries are present in the simulated payload but not in
        // the SDK.
        // @"cumulativeBackgroundURLSessionCompletionTimeoutExitCount" : @3,
        // @"cumulativeBackgroundFetchCompletionTimeoutExitCount" : @4,
        @"cumulativeAbnormalExitCount" : @5,
        @"cumulativeSuspendedWithLockedFileExitCount" : @6,
        @"cumulativeIllegalInstructionExitCount" : @7,
        @"cumulativeMemoryPressureExitCount" : @8,
        @"cumulativeBadAccessExitCount" : @9,
        @"cumulativeCPUResourceLimitExitCount" : @10,
        @"cumulativeBackgroundTaskAssertionTimeoutExitCount" : @11,
        @"cumulativeNormalAppExitCount" : @12
      },
      @"foregroundExitData" : @{
        @"cumulativeBadAccessExitCount" : @13,
        @"cumulativeAbnormalExitCount" : @14,
        @"cumulativeMemoryResourceLimitExitCount" : @15,
        @"cumulativeNormalAppExitCount" : @16,
        // This entry is present in the simulated payload but not in the SDK.
        // @"cumulativeCPUResourceLimitExitCount" : @17,
        @"cumulativeIllegalInstructionExitCount" : @18,
        @"cumulativeAppWatchdogExitCount" : @19
      }
    },
  });
  NSArray* array = @[ mock_report ];
  [[MetricKitSubscriber sharedInstance] didReceiveMetricPayloads:array];
  tester.ExpectUniqueTimeSample("IOS.MetricKit.ForegroundTimePerDay",
                                base::Seconds(1), 1);
  tester.ExpectUniqueTimeSample("IOS.MetricKit.BackgroundTimePerDay",
                                base::Seconds(2), 1);
  tester.ExpectUniqueSample("IOS.MetricKit.PeakMemoryUsage", 3, 1);
  tester.ExpectUniqueSample("IOS.MetricKit.AverageSuspendedMemory", 4, 1);

  tester.ExpectTotalCount("IOS.MetricKit.ApplicationResumeTime", 12);
  tester.ExpectBucketCount("IOS.MetricKit.ApplicationResumeTime", 25, 5);
  tester.ExpectBucketCount("IOS.MetricKit.ApplicationResumeTime", 35, 7);

  tester.ExpectTotalCount("IOS.MetricKit.TimeToFirstDraw", 6);
  tester.ExpectBucketCount("IOS.MetricKit.TimeToFirstDraw", 5, 2);
  tester.ExpectBucketCount("IOS.MetricKit.TimeToFirstDraw", 15, 4);

  tester.ExpectTotalCount("IOS.MetricKit.BackgroundExitData", 71);
  tester.ExpectBucketCount("IOS.MetricKit.BackgroundExitData", 2, 1);
  tester.ExpectBucketCount("IOS.MetricKit.BackgroundExitData", 4, 2);
  tester.ExpectBucketCount("IOS.MetricKit.BackgroundExitData", 1, 5);
  tester.ExpectBucketCount("IOS.MetricKit.BackgroundExitData", 6, 6);
  tester.ExpectBucketCount("IOS.MetricKit.BackgroundExitData", 8, 7);
  tester.ExpectBucketCount("IOS.MetricKit.BackgroundExitData", 5, 8);
  tester.ExpectBucketCount("IOS.MetricKit.BackgroundExitData", 7, 9);
  tester.ExpectBucketCount("IOS.MetricKit.BackgroundExitData", 3, 10);
  tester.ExpectBucketCount("IOS.MetricKit.BackgroundExitData", 9, 11);
  tester.ExpectBucketCount("IOS.MetricKit.BackgroundExitData", 0, 12);

  tester.ExpectTotalCount("IOS.MetricKit.ForegroundExitData", 95);
  tester.ExpectBucketCount("IOS.MetricKit.ForegroundExitData", 7, 13);
  tester.ExpectBucketCount("IOS.MetricKit.ForegroundExitData", 1, 14);
  tester.ExpectBucketCount("IOS.MetricKit.ForegroundExitData", 4, 15);
  tester.ExpectBucketCount("IOS.MetricKit.ForegroundExitData", 0, 16);
  tester.ExpectBucketCount("IOS.MetricKit.ForegroundExitData", 8, 18);
  tester.ExpectBucketCount("IOS.MetricKit.ForegroundExitData", 2, 19);
}

// Tests that Metrickit reports are correctly saved in the document directory.
TEST_F(MetricKitSubscriberTest, SaveMetricsReport) {
  id mock_report = OCMClassMock([MXMetricPayload class]);
  NSDate* date = [NSDate date];
  std::string file_data("report content");
  NSData* data = [NSData dataWithBytes:file_data.c_str()
                                length:file_data.size()];
  NSDateFormatter* formatter = [[NSDateFormatter alloc] init];
  [formatter setDateFormat:@"yyyyMMdd_HHmmss"];
  [formatter setTimeZone:[NSTimeZone timeZoneWithName:@"UTC"]];
  NSString* file_name = [NSString
      stringWithFormat:@"Metrics-%@.json", [formatter stringFromDate:date]];

  base::FilePath file_path =
      MetricKitReportDirectory().Append(base::SysNSStringToUTF8(file_name));
  OCMStub([mock_report timeStampEnd]).andReturn(date);
  OCMStub([mock_report JSONRepresentation]).andReturn(data);
  NSArray* array = @[ mock_report ];
  [[MetricKitSubscriber sharedInstance] didReceiveMetricPayloads:array];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForFileOperationTimeout, ^bool() {
        base::RunLoop().RunUntilIdle();
        return base::PathExists(file_path);
      }));
  std::string content;
  base::ReadFileToString(file_path, &content);
  EXPECT_EQ(content, file_data);
}

TEST_F(MetricKitSubscriberTest, SaveDiagnosticReport) {
  id mock_report = OCMClassMock([MXDiagnosticPayload class]);
  NSDate* date = [NSDate date];
  std::string file_data("report content");
  NSData* data = [NSData dataWithBytes:file_data.c_str()
                                length:file_data.size()];
  NSDateFormatter* formatter = [[NSDateFormatter alloc] init];
  [formatter setDateFormat:@"yyyyMMdd_HHmmss"];
  [formatter setTimeZone:[NSTimeZone timeZoneWithName:@"UTC"]];
  NSString* file_name = [NSString
      stringWithFormat:@"Diagnostic-%@.json", [formatter stringFromDate:date]];

  base::FilePath file_path =
      MetricKitReportDirectory().Append(base::SysNSStringToUTF8(file_name));
  OCMStub([mock_report timeStampEnd]).andReturn(date);
  OCMStub([mock_report JSONRepresentation]).andReturn(data);
  NSArray* array = @[ mock_report ];
  [[MetricKitSubscriber sharedInstance] didReceiveDiagnosticPayloads:array];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForFileOperationTimeout, ^bool() {
        base::RunLoop().RunUntilIdle();
        return base::PathExists(file_path);
      }));
  std::string content;
  base::ReadFileToString(file_path, &content);
  EXPECT_EQ(content, file_data);
}
