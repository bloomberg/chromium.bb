// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_EVENT_H_
#define COMPONENTS_METRICS_STRUCTURED_EVENT_H_

#include <map>
#include <memory>
#include <string>

#include "base/values.h"

// Builder classes for sending events are generated in the similarily-named
// //components/metrics/structured/structured_events.h.
//
// Builder classes will generate subclasses of |Event| rather than |EventBase|
// once the mojo implementation of structured metrics is complete. |EventBase|
// will only be used by the central service to record events.

namespace metrics {
namespace structured {

// Event to be built and sent by StructuredMetrics clients. Builder
// classes will be codegen'd from metrics definitions in structured.xml, but
// clients may choose to use this class directly to build Events.
class Event {
 public:
  // There should be a 1-1 mapping between MetricType and the mojom enums.
  //
  // TODO(jongahn): Move this into common enum file.
  enum class MetricType {
    kHmac = 0,
    kLong = 1,
    kInt = 2,
    kDouble = 3,
    kRawString = 4,
    kBoolean = 5,
  };

  // Holds the value and the type of the metric encoded.
  struct MetricValue {
    MetricValue() = default;
    MetricValue(MetricType type, base::Value value);

    MetricValue(MetricValue&& other);
    MetricValue& operator=(MetricValue&& other);

    bool operator==(const MetricValue& rhs) const;
    ~MetricValue();

    MetricType type;
    base::Value value;
  };

  Event();
  Event(const std::string& project_name, const std::string& event_name);

  Event(Event&& other);
  Event& operator=(Event&& other);

  virtual ~Event();

  // Records |this|. Once this method is called, |this| will be unsafe to
  // access.
  void Record();

  // Returns true if the value was added successfully. |type| and type of
  // |value| must be consistent and will be enforced. If the data in |value| and
  // |type| do match, then |value| will be moved into |this| when called.
  bool AddMetric(const std::string& metric_name,
                 MetricType type,
                 base::Value&& value);

  const std::string& project_name() const;
  const std::string& event_name() const;
  const std::map<std::string, MetricValue>& metric_values() const;

 private:
  std::string project_name_;
  std::string event_name_;
  std::map<std::string, MetricValue> metric_values_;
};

}  // namespace structured
}  // namespace metrics

#endif  // COMPONENTS_METRICS_STRUCTURED_EVENT_H_
