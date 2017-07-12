// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkletModuleResponsesMap_h
#define WorkletModuleResponsesMap_h

#include "core/CoreExport.h"
#include "core/loader/modulescript/ModuleScriptCreationParams.h"
#include "platform/heap/Heap.h"
#include "platform/heap/HeapAllocator.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/KURLHash.h"

namespace blink {

// WorkletModuleResponsesMap implements the module responses map concept and the
// "fetch a worklet script" algorithm:
// https://drafts.css-houdini.org/worklets/#module-responses-map
// https://drafts.css-houdini.org/worklets/#fetch-a-worklet-script
class CORE_EXPORT WorkletModuleResponsesMap
    : public GarbageCollectedFinalized<WorkletModuleResponsesMap> {
 public:
  // Used for notifying results of ReadOrCreateEntry(). See comments on the
  // function for details.
  class Client : public GarbageCollectedMixin {
   public:
    virtual ~Client() {}
    virtual void OnRead(const ModuleScriptCreationParams&) = 0;
    virtual void OnFetchNeeded() = 0;
    virtual void OnFailed() = 0;
  };

  WorkletModuleResponsesMap() = default;

  // Reads an entry for a given URL, or creates a placeholder entry:
  // 1) If an entry is already fetched, synchronously calls Client::OnRead().
  // 2) If an entry is now being fetched, pushes a given client into the entry's
  //    waiting queue and asynchronously calls Client::OnRead() on the
  //    completion of the fetch.
  // 3) If an entry doesn't exist, creates a placeholder entry and synchronously
  //    calls Client::OnFetchNeeded. A caller is required to fetch a module
  //    script and update the entry via UpdateEntry().
  void ReadOrCreateEntry(const KURL&, Client*);

  // Updates an entry in 'fetching' state to 'fetched'.
  void UpdateEntry(const KURL&, const ModuleScriptCreationParams&);

  // Marks an entry as "failed" state and calls OnFailed() for waiting clients.
  void InvalidateEntry(const KURL&);

  DECLARE_TRACE();

 private:
  class Entry;

  // TODO(nhiroki): Keep the insertion order of top-level modules to replay
  // addModule() calls for a newly created global scope.
  // See https://drafts.css-houdini.org/worklets/#creating-a-workletglobalscope
  HeapHashMap<KURL, Member<Entry>> entries_;
};

}  // namespace blink

#endif  // WorkletModuleResponsesMap_h
