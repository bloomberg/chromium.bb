// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_TASK_ATTRIBUTION_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_TASK_ATTRIBUTION_TIMING_H_

#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class TaskAttributionTiming final : public PerformanceEntry {
  DEFINE_WRAPPERTYPEINFO();

 public:
  AtomicString entryType() const override;
  PerformanceEntryType EntryTypeEnum() const override;

  AtomicString containerType() const;
  String containerSrc() const;
  String containerId() const;
  String containerName() const;

  void Trace(Visitor*) override;

  TaskAttributionTiming(const AtomicString& type,
                        const AtomicString& container_type,
                        const String& container_src,
                        const String& container_id,
                        const String& container_name);
  ~TaskAttributionTiming() override;

 private:
  void BuildJSONValue(V8ObjectBuilder&) const override;

  AtomicString container_type_;
  // TODO(crbug.com/1030396): change the members below to AtomicString.
  String container_src_;
  String container_id_;
  String container_name_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_TASK_ATTRIBUTION_TIMING_H_
