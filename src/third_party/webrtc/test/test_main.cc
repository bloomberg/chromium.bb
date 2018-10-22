/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/flags.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial_default.h"
#include "system_wrappers/include/metrics_default.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"
#include "test/testsupport/perf_test.h"

#if defined(WEBRTC_IOS)
#include "test/ios/test_support.h"

DEFINE_string(NSTreatUnknownArgumentsAsOpen,
              "",
              "Intentionally ignored flag intended for iOS simulator.");
DEFINE_string(ApplePersistenceIgnoreState,
              "",
              "Intentionally ignored flag intended for iOS simulator.");
DEFINE_bool(
    save_chartjson_result,
    false,
    "Store the perf results in Documents/perf_result.json in the format "
    "described by "
    "https://github.com/catapult-project/catapult/blob/master/dashboard/docs/"
    "data-format.md.");

#else

DEFINE_string(isolated_script_test_output,
              "",
              "Intentionally ignored flag intended for Chromium.");

DEFINE_string(
    isolated_script_test_perf_output,
    "",
    "Path where the perf results should be stored in the JSON format described "
    "by "
    "https://github.com/catapult-project/catapult/blob/master/dashboard/docs/"
    "data-format.md.");

#endif

DEFINE_bool(logs, false, "print logs to stderr");

DEFINE_string(
    force_fieldtrials,
    "",
    "Field trials control experimental feature code which can be forced. "
    "E.g. running with --force_fieldtrials=WebRTC-FooFeature/Enable/"
    " will assign the group Enable to field trial WebRTC-FooFeature.");

DEFINE_bool(help, false, "Print this message.");

int main(int argc, char* argv[]) {
  ::testing::InitGoogleMock(&argc, argv);

  // Default to LS_INFO, even for release builds to provide better test logging.
  // TODO(pbos): Consider adding a command-line override.
  if (rtc::LogMessage::GetLogToDebug() > rtc::LS_INFO)
    rtc::LogMessage::LogToDebug(rtc::LS_INFO);

  if (rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, false)) {
    return 1;
  }
  if (FLAG_help) {
    rtc::FlagList::Print(nullptr, false);
    return 0;
  }

  webrtc::test::SetExecutablePath(argv[0]);
  webrtc::test::ValidateFieldTrialsStringOrDie(FLAG_force_fieldtrials);
  // InitFieldTrialsFromString stores the char*, so the char array must outlive
  // the application.
  webrtc::field_trial::InitFieldTrialsFromString(FLAG_force_fieldtrials);
  webrtc::metrics::Enable();

  rtc::LogMessage::SetLogToStderr(FLAG_logs);

#if defined(WEBRTC_IOS)

  rtc::test::InitTestSuite(RUN_ALL_TESTS, argc, argv,
                           FLAG_save_chartjson_result);
  rtc::test::RunTestsFromIOSApp();

#else

  int exit_code = RUN_ALL_TESTS();

  std::string chartjson_result_file = FLAG_isolated_script_test_perf_output;
  if (!chartjson_result_file.empty()) {
    webrtc::test::WritePerfResults(chartjson_result_file);
  }

  return exit_code;

#endif
}
