// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_TRACING_MODEL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_TRACING_MODEL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/values.h"

namespace arc {

class ArcTracingEvent;

// This is a base model that is built from the output of Chrome tracing
// (chrome://tracing). It contains native Chrome test events and system kernel
// events converted to common Chrome test events. Events are kept by thread or
// by group in case of asynchronous events. Events are hierarchical and each
// thread or group is represented by one top-level event.
// There are methods to query the model for particular events.
// |ArcTracingModel| is usually used as a source for more specialized models.
class ArcTracingModel {
 public:
  using TracingEvents = std::vector<std::unique_ptr<ArcTracingEvent>>;
  using TracingEventPtrs = std::vector<const ArcTracingEvent*>;

  ArcTracingModel();
  ~ArcTracingModel();

  // Builds model from string data in Json format. Returns false if model
  // cannot be built.
  bool Build(const std::string& data);

  // Selects list of events according to |query|. |query| consists from segments
  // separated by '/' where segment is in format
  // category:name(arg_name=arg_value;..). See ArcTracingEventMatcher for more
  // details. Processing starts from the each root node for thread or group.
  TracingEventPtrs Select(const std::string query) const;
  // Similar to case above but starts from provided event |event|.
  TracingEventPtrs Select(const ArcTracingEvent* event,
                          const std::string query) const;

  // Dumps this model to |stream|.
  void Dump(std::ostream& stream) const;

 private:
  // Processes list of events. Returns true in case all events were processed
  // successfully.
  bool ProcessEvent(base::ListValue* events);

  // Converts sys traces events to the |base::Dictionary| based format used in
  // Chrome.
  bool ConvertSysTraces(const std::string& sys_traces);

  // Adds tracing event to the thread model hierarchy.
  bool AddToThread(std::unique_ptr<ArcTracingEvent> event);

  // Contains events separated by threads. Key is a composition of pid and tid.
  std::map<uint64_t, TracingEvents> per_thread_events_;
  // Contains events, separated by id of the event. Used for asynchronous
  // tracing events.
  std::map<std::string, TracingEvents> group_events_;

  // Metadata events.
  TracingEvents metadata_events_;

  DISALLOW_COPY_AND_ASSIGN(ArcTracingModel);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_TRACING_MODEL_H_
