// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/mojom/event_mojom_traits.h"

#include <map>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/mojom/event.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo {

// static
metrics::structured::mojom::MetricValueDataView::Tag
UnionTraits<metrics::structured::mojom::MetricValueDataView,
            metrics::structured::Event::MetricValue>::
    GetTag(const metrics::structured::Event::MetricValue& metric_value) {
  switch (metric_value.type) {
    case metrics::structured::Event::MetricType::kHmac:
      return metrics::structured::mojom::MetricValueDataView::Tag::HMAC_VALUE;
    case metrics::structured::Event::MetricType::kLong:
      return metrics::structured::mojom::MetricValueDataView::Tag::LONG_VALUE;
    case metrics::structured::Event::MetricType::kInt:
      return metrics::structured::mojom::MetricValueDataView::Tag::INT_VALUE;
    case metrics::structured::Event::MetricType::kDouble:
      return metrics::structured::mojom::MetricValueDataView::Tag::DOUBLE_VALUE;
    case metrics::structured::Event::MetricType::kRawString:
      return metrics::structured::mojom::MetricValueDataView::Tag::
          RAW_STR_VALUE;
    case metrics::structured::Event::MetricType::kBoolean:
      return metrics::structured::mojom::MetricValueDataView::Tag::BOOL_VALUE;
  }
}

// static
bool UnionTraits<metrics::structured::mojom::MetricValueDataView,
                 metrics::structured::Event::MetricValue>::
    Read(metrics::structured::mojom::MetricValueDataView metric,
         metrics::structured::Event::MetricValue* out) {
  switch (metric.tag()) {
    case metrics::structured::mojom::MetricValueDataView::Tag::HMAC_VALUE: {
      std::string hmac_value;
      if (!metric.ReadHmacValue(&hmac_value))
        return false;
      out->type = metrics::structured::Event::MetricType::kHmac;
      out->value = base::Value(std::move(hmac_value));
      break;
    }
    case metrics::structured::mojom::MetricValueDataView::Tag::LONG_VALUE:
      out->type = metrics::structured::Event::MetricType::kLong;
      out->value = base::Value(base::NumberToString(metric.long_value()));
      break;
    case metrics::structured::mojom::MetricValueDataView::Tag::INT_VALUE:
      out->type = metrics::structured::Event::MetricType::kInt;
      out->value = base::Value(metric.int_value());
      break;
    case metrics::structured::mojom::MetricValueDataView::Tag::DOUBLE_VALUE:
      out->type = metrics::structured::Event::MetricType::kDouble;
      out->value = base::Value(metric.double_value());
      break;
    case metrics::structured::mojom::MetricValueDataView::Tag::RAW_STR_VALUE: {
      std::string raw_str_value;
      if (!metric.ReadRawStrValue(&raw_str_value))
        return false;
      out->type = metrics::structured::Event::MetricType::kRawString;
      out->value = base::Value(std::move(raw_str_value));
      break;
    }
    case metrics::structured::mojom::MetricValueDataView::Tag::BOOL_VALUE:
      out->type = metrics::structured::Event::MetricType::kBoolean;
      out->value = base::Value(metric.bool_value());
      break;
  }
  return true;
}

// static
bool StructTraits<metrics::structured::mojom::EventDataView,
                  metrics::structured::Event>::
    Read(metrics::structured::mojom::EventDataView event,
         metrics::structured::Event* out) {
  std::string project_name, event_name;
  std::map<std::string, metrics::structured::Event::MetricValue> metrics;
  if (!event.ReadProjectName(&project_name) ||
      !event.ReadEventName(&event_name) || !event.ReadMetrics(&metrics))
    return false;

  *out = metrics::structured::Event(project_name, event_name);

  for (auto&& metric : metrics) {
    if (!out->AddMetric(metric.first, metric.second.type,
                        std::move(metric.second.value)))
      return false;
  }
  return true;
}

}  // namespace mojo
