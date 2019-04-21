// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/perf/performance_test.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/trace_event/trace_event.h"
#include "chrome/test/base/test_switches.h"
#include "content/public/browser/tracing_controller.h"
#include "services/tracing/public/cpp/trace_event_agent.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gl/gl_switches.h"

static const char kTraceDir[] = "trace-dir";

////////////////////////////////////////////////////////////////////////////////
// PerformanceTest

PerformanceTest::PerformanceTest()
    : should_start_trace_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(kTraceDir)) {
  if (should_start_trace_) {
    auto* cmd = base::CommandLine::ForCurrentProcess();
    cmd->AppendSwitch(switches::kUseGpuInTests);
    cmd->AppendSwitch(switches::kEnablePixelOutputInTests);
  }
}

PerformanceTest::~PerformanceTest() = default;

std::vector<std::string> PerformanceTest::GetUMAHistogramNames() const {
  return {};
}

const std::string PerformanceTest::GetTracingCategories() const {
  return std::string();
}

void PerformanceTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();
  if (!should_start_trace_)
    return;

  auto* controller = content::TracingController::GetInstance();
  base::trace_event::TraceConfig config(GetTracingCategories(),
                                        base::trace_event::RECORD_CONTINUOUSLY);
  for (const auto& histogram : GetUMAHistogramNames())
    config.EnableHistogram(histogram);

  base::RunLoop runloop;
  bool result = controller->StartTracing(config, runloop.QuitClosure());
  runloop.Run();
  CHECK(result);
}

void PerformanceTest::TearDownOnMainThread() {
  if (should_start_trace_) {
    auto* controller = content::TracingController::GetInstance();
    ASSERT_TRUE(controller->IsTracing())
        << "Did you forget to call PerformanceTest::SetUpOnMainThread?";

    base::RunLoop runloop;
    base::FilePath dir =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(kTraceDir);
    base::FilePath trace_file;
    CHECK(base::CreateTemporaryFileInDir(dir, &trace_file));

    auto trace_data_endpoint = content::TracingController::CreateFileEndpoint(
        trace_file, runloop.QuitClosure());
    bool result = controller->StopTracing(trace_data_endpoint);
    runloop.Run();
    CHECK(result);
  }
  bool print = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kPerfTestPrintUmaMeans);
  LOG_IF(INFO, print) << "=== Histogram Means ===";
  for (auto name : GetUMAHistogramNames()) {
    EXPECT_TRUE(HasHistogram(name)) << "missing histogram:" << name;
    LOG_IF(INFO, print) << name << ": " << GetHistogramMean(name);
  }
  LOG_IF(INFO, print) << "=== End Histogram Means ===";

  InProcessBrowserTest::TearDownOnMainThread();
}

float PerformanceTest::GetHistogramMean(const std::string& name) {
  auto* histogram = base::StatisticsRecorder::FindHistogram(name);
  if (!histogram)
    return 0;
  auto samples = histogram->SnapshotSamples();
  DCHECK_NE(0, samples->TotalCount());
  return static_cast<float>(samples->sum()) / samples->TotalCount();
}

bool PerformanceTest::HasHistogram(const std::string& name) {
  return !!base::StatisticsRecorder::FindHistogram(name);
}

////////////////////////////////////////////////////////////////////////////////
// UIPerformanceTest

const std::string UIPerformanceTest::GetTracingCategories() const {
  return "benchmark,cc,viz,input,latency,gpu,rail,toplevel,ui,views,viz";
}
