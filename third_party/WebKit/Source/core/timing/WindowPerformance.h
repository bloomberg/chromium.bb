/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012 Intel Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WindowPerformance_h
#define WindowPerformance_h

#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/frame/PerformanceMonitor.h"
#include "core/timing/MemoryInfo.h"
#include "core/timing/Performance.h"
#include "core/timing/PerformanceNavigation.h"
#include "core/timing/PerformanceTiming.h"

namespace blink {

class CORE_EXPORT WindowPerformance final : public Performance,
                                            public PerformanceMonitor::Client,
                                            public DOMWindowClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(WindowPerformance);
  friend class WindowPerformanceTest;

 public:
  static WindowPerformance* Create(LocalDOMWindow* window) {
    return new WindowPerformance(window);
  }
  ~WindowPerformance() override;

  ExecutionContext* GetExecutionContext() const override;

  MemoryInfo* memory();
  PerformanceNavigation* navigation() const;
  PerformanceTiming* timing() const override;

  void UpdateLongTaskInstrumentation() override;

  void Trace(blink::Visitor*) override;
  using Performance::TraceWrappers;

 private:
  explicit WindowPerformance(LocalDOMWindow*);

  PerformanceNavigationTiming* CreateNavigationTimingInstance() override;

  static std::pair<String, DOMWindow*> SanitizedAttribution(
      ExecutionContext*,
      bool has_multiple_contexts,
      LocalFrame* observer_frame);

  // PerformanceMonitor::Client implementation.
  void ReportLongTask(
      double start_time,
      double end_time,
      ExecutionContext* task_context,
      bool has_multiple_contexts,
      const SubTaskAttribution::EntriesVector& sub_task_attributions) override;

  void BuildJSONValue(V8ObjectBuilder&) const override;

  mutable Member<PerformanceNavigation> navigation_;
  mutable Member<PerformanceTiming> timing_;
};

}  // namespace blink

#endif  // WindowPerformance_h
