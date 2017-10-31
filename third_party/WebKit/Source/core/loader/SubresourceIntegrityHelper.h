// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SubresourceIntegrityHelper_h
#define SubresourceIntegrityHelper_h

#include "core/CoreExport.h"
#include "core/inspector/ConsoleMessage.h"
#include "platform/loader/SubresourceIntegrity.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class ExecutionContext;

class CORE_EXPORT SubresourceIntegrityHelper final {
  STATIC_ONLY(SubresourceIntegrityHelper);

 public:
  static void DoReport(ExecutionContext&,
                       const SubresourceIntegrity::ReportInfo&);

  static void GetConsoleMessages(const SubresourceIntegrity::ReportInfo&,
                                 HeapVector<Member<ConsoleMessage>>*);
};

}  // namespace blink

#endif
