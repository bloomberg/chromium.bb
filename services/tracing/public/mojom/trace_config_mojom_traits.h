// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This defines mappings from mojom IPC representations to their native perfetto
// equivalents.

#ifndef SERVICES_TRACING_PUBLIC_MOJOM_TRACE_CONFIG_MOJOM_TRAITS_H_
#define SERVICES_TRACING_PUBLIC_MOJOM_TRACE_CONFIG_MOJOM_TRAITS_H_

#include <string>
#include <vector>

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"

namespace mojo {

// perfetto::TraceConfig::BufferConfig
template <>
class StructTraits<tracing::mojom::BufferConfigDataView,
                   perfetto::TraceConfig::BufferConfig> {
 public:
  static uint32_t size_kb(const perfetto::TraceConfig::BufferConfig& src) {
    return src.size_kb();
  }

  static bool Read(tracing::mojom::BufferConfigDataView data,
                   perfetto::TraceConfig::BufferConfig* out);
};

// perfetto::TraceConfig::DataSource
template <>
class StructTraits<tracing::mojom::DataSourceDataView,
                   perfetto::TraceConfig::DataSource> {
 public:
  static const perfetto::DataSourceConfig& config(
      const perfetto::TraceConfig::DataSource& src) {
    return src.config();
  }

  static bool Read(tracing::mojom::DataSourceDataView data,
                   perfetto::TraceConfig::DataSource* out);
};

// perfetto::TraceConfig
template <>
class StructTraits<tracing::mojom::TraceConfigDataView, perfetto::TraceConfig> {
 public:
  static const std::vector<perfetto::TraceConfig::DataSource>& data_sources(
      const perfetto::TraceConfig& src) {
    return src.data_sources();
  }

  static const std::vector<perfetto::TraceConfig::BufferConfig>& buffers(
      const perfetto::TraceConfig& src) {
    return src.buffers();
  }

  static uint32_t duration_ms(const perfetto::TraceConfig& src) {
    return src.duration_ms();
  }

  static bool Read(tracing::mojom::TraceConfigDataView data,
                   perfetto::TraceConfig* out);
};

}  // namespace mojo
#endif  // SERVICES_TRACING_PUBLIC_MOJOM_TRACE_CONFIG_MOJOM_TRAITS_H_
