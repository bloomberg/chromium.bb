// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/trace_startup.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/trace_event/trace_log.h"
#include "components/tracing/common/trace_startup_config.h"
#include "components/tracing/common/trace_to_console.h"
#include "components/tracing/common/tracing_switches.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "services/tracing/public/cpp/tracing_features.h"

namespace tracing {

void EnableStartupTracingIfNeeded() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // TODO(oysteine): Support startup tracing to a perfetto protobuf trace. This
  // should also enable TraceLog and call
  // TraceEventDataSource::SetupStartupTracing().
  if (command_line.HasSwitch(switches::kPerfettoOutputFile))
    return;

  // Ensure TraceLog is initialized first.
  // https://crbug.com/764357
  base::trace_event::TraceLog::GetInstance();

  if (TraceStartupConfig::GetInstance()->IsEnabled()) {
    const base::trace_event::TraceConfig& trace_config =
        TraceStartupConfig::GetInstance()->GetTraceConfig();
    uint8_t modes = base::trace_event::TraceLog::RECORDING_MODE;
    if (!trace_config.event_filters().empty())
      modes |= base::trace_event::TraceLog::FILTERING_MODE;
    if (TracingUsesPerfettoBackend())
      TraceEventDataSource::GetInstance()->SetupStartupTracing();
    base::trace_event::TraceLog::GetInstance()->SetEnabled(
        TraceStartupConfig::GetInstance()->GetTraceConfig(), modes);
  } else if (command_line.HasSwitch(switches::kTraceToConsole)) {
    // TODO(eseckler): Remove ability to trace to the console, perfetto doesn't
    // support this and noone seems to use it.
    base::trace_event::TraceConfig trace_config = GetConfigForTraceToConsole();
    LOG(ERROR) << "Start " << switches::kTraceToConsole
               << " with CategoryFilter '"
               << trace_config.ToCategoryFilterString() << "'.";
    if (TracingUsesPerfettoBackend())
      TraceEventDataSource::GetInstance()->SetupStartupTracing();
    base::trace_event::TraceLog::GetInstance()->SetEnabled(
        trace_config, base::trace_event::TraceLog::RECORDING_MODE);
  }
}

}  // namespace tracing
