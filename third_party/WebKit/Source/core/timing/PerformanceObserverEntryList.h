// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PerformanceObserverEntryList_h
#define PerformanceObserverEntryList_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"

namespace blink {

class PerformanceEntry;
using PerformanceEntryVector = HeapVector<Member<PerformanceEntry>>;

class PerformanceObserverEntryList
    : public GarbageCollectedFinalized<PerformanceObserverEntryList>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  PerformanceObserverEntryList(const PerformanceEntryVector&);

  virtual ~PerformanceObserverEntryList();

  PerformanceEntryVector getEntries() const;
  PerformanceEntryVector getEntriesByType(const String& entry_type);
  PerformanceEntryVector getEntriesByName(const String& name,
                                          const String& entry_type);

  void Trace(blink::Visitor*);

 protected:
  PerformanceEntryVector performance_entries_;
};

}  // namespace blink

#endif  // PerformanceObserverEntryList_h
