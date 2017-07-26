// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ReportingObserver_h
#define ReportingObserver_h

#include "bindings/core/v8/ReportingObserverCallback.h"
#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/Vector.h"

namespace blink {

class ExecutionContext;
class Report;
class ReportingObserverCallback;

class CORE_EXPORT ReportingObserver final
    : public GarbageCollected<ReportingObserver>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ReportingObserver* Create(ExecutionContext*,
                                   ReportingObserverCallback*);

  // Call the callback with reports.
  void ReportToCallback(const HeapVector<Member<Report>>& reports);

  void observe();
  void disconnect();

  DECLARE_TRACE();

 private:
  explicit ReportingObserver(ExecutionContext*, ReportingObserverCallback*);

  Member<ExecutionContext> execution_context_;
  Member<ReportingObserverCallback> callback_;
};

}  // namespace blink

#endif  // ReportingObserver_h
