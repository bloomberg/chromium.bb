// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/perfetto_config.h"

#include <cstdint>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_config.h"
#include "build/build_config.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"

namespace tracing {

perfetto::TraceConfig GetDefaultPerfettoConfig(
    const base::trace_event::TraceConfig& chrome_config,
    bool privacy_filtering_enabled) {
  perfetto::TraceConfig perfetto_config;

  size_t size_limit = chrome_config.GetTraceBufferSizeInKb();
  if (size_limit == 0) {
    size_limit = 100 * 1024;
  }
  perfetto_config.add_buffers()->set_size_kb(size_limit);

  // Perfetto uses clock_gettime for its internal snapshotting, which gets
  // blocked by the sandboxed and isn't needed for Chrome regardless.
  auto* builtin_data_sources = perfetto_config.mutable_builtin_data_sources();
  builtin_data_sources->set_disable_clock_snapshotting(true);

  // Capture actual trace events.
  auto* trace_event_data_source = perfetto_config.add_data_sources();
  for (auto& enabled_pid :
       chrome_config.process_filter_config().included_process_ids()) {
    *trace_event_data_source->add_producer_name_filter() = base::StrCat(
        {mojom::kPerfettoProducerNamePrefix,
         base::NumberToString(static_cast<uint32_t>(enabled_pid))});
  }

  // We strip the process filter from the config string we send to Perfetto,
  // so perfetto doesn't reject it from a future
  // TracingService::ChangeTraceConfig call due to being an unsupported
  // update.
  base::trace_event::TraceConfig processfilter_stripped_config(chrome_config);
  processfilter_stripped_config.SetProcessFilterConfig(
      base::trace_event::TraceConfig::ProcessFilterConfig());
  std::string chrome_config_string = processfilter_stripped_config.ToString();

  auto* trace_event_config = trace_event_data_source->mutable_config();
  trace_event_config->set_name(tracing::mojom::kTraceEventDataSourceName);
  trace_event_config->set_target_buffer(0);
  auto* chrome_proto_config = trace_event_config->mutable_chrome_config();
  chrome_proto_config->set_trace_config(chrome_config_string);
  chrome_proto_config->set_privacy_filtering_enabled(privacy_filtering_enabled);

// Capture system trace events if supported and enabled. The datasources will
// only emit events if system tracing is enabled in |chrome_config|.
#if defined(OS_CHROMEOS) || (defined(IS_CHROMECAST) && defined(OS_LINUX))
  auto* system_trace_config =
      perfetto_config.add_data_sources()->mutable_config();
  system_trace_config->set_name(tracing::mojom::kSystemTraceDataSourceName);
  system_trace_config->set_target_buffer(0);
  auto* system_chrome_config = system_trace_config->mutable_chrome_config();
  system_chrome_config->set_trace_config(chrome_config_string);
  system_chrome_config->set_privacy_filtering_enabled(
      privacy_filtering_enabled);
#endif

#if defined(OS_CHROMEOS)
  auto* arc_trace_config = perfetto_config.add_data_sources()->mutable_config();
  arc_trace_config->set_name(tracing::mojom::kArcTraceDataSourceName);
  arc_trace_config->set_target_buffer(0);
  auto* arc_chrome_config = arc_trace_config->mutable_chrome_config();
  arc_chrome_config->set_trace_config(chrome_config_string);
  arc_chrome_config->set_privacy_filtering_enabled(privacy_filtering_enabled);
#endif

  // Also capture global metadata.
  auto* trace_metadata_config =
      perfetto_config.add_data_sources()->mutable_config();
  trace_metadata_config->set_name(tracing::mojom::kMetaDataSourceName);
  trace_metadata_config->set_target_buffer(0);
  auto* metadata_chrome_config = trace_metadata_config->mutable_chrome_config();
  metadata_chrome_config->set_trace_config(chrome_config_string);
  metadata_chrome_config->set_privacy_filtering_enabled(
      privacy_filtering_enabled);

  return perfetto_config;
}

}  // namespace tracing
