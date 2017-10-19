// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkletModuleResponsesMap_h
#define WorkletModuleResponsesMap_h

#include "core/CoreExport.h"
#include "core/loader/modulescript/ModuleScriptCreationParams.h"
#include "platform/heap/Heap.h"
#include "platform/heap/HeapAllocator.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/KURLHash.h"

namespace blink {

// WorkletModuleResponsesMap implements the module responses map concept and the
// "fetch a worklet script" algorithm:
// https://drafts.css-houdini.org/worklets/#module-responses-map
// https://drafts.css-houdini.org/worklets/#fetch-a-worklet-script
//
// This acts as a cache for creation params (including source code) of module
// scripts, but also performs fetch when needed. The creation params are added
// and retrieved using ReadEntry(). If a module script for a given URL has
// already been fetched, ReadEntry() returns the cached creation params.
// Otherwise, ReadEntry() internally creates DocumentModuleScriptFetcher with
// thie ResourceFetcher that is given to its ctor. Once the module script is
// fetched, its creation params are cached and ReadEntry() returns it.
class CORE_EXPORT WorkletModuleResponsesMap
    : public GarbageCollectedFinalized<WorkletModuleResponsesMap> {
 public:
  // Used for notifying results of ReadEntry(). See comments on the function for
  // details.
  class CORE_EXPORT Client : public GarbageCollectedMixin {
   public:
    virtual ~Client() {}
    virtual void OnRead(const ModuleScriptCreationParams&) = 0;
    virtual void OnFailed() = 0;
  };

  explicit WorkletModuleResponsesMap(ResourceFetcher*);

  // Reads an entry for the given URL. If the entry is already fetched,
  // synchronously calls Client::OnRead(). Otherwise, it's called on the
  // completion of the fetch. See also the class-level comment.
  void ReadEntry(FetchParameters&, Client*);

  // Invalidates the entry and calls OnFailed() for waiting clients.
  void InvalidateEntry(const KURL&);

  // Called when the associated document is destroyed. Aborts all waiting
  // clients and clears the map. Following read and write requests to the map
  // are simply ignored.
  void Dispose();

  void Trace(blink::Visitor*);

 private:
  class Entry;

  bool is_available_ = true;

  Member<ResourceFetcher> fetcher_;

  // TODO(nhiroki): Keep the insertion order of top-level modules to replay
  // addModule() calls for a newly created global scope.
  // See https://drafts.css-houdini.org/worklets/#creating-a-workletglobalscope
  HeapHashMap<KURL, Member<Entry>> entries_;
};

}  // namespace blink

#endif  // WorkletModuleResponsesMap_h
