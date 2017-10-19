// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ModuleMap_h
#define ModuleMap_h

#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/KURLHash.h"
#include "platform/wtf/HashMap.h"

namespace blink {

class Modulator;
class ModuleScript;
class ModuleScriptFetchRequest;
class SingleModuleClient;
enum class ModuleGraphLevel;

// A ModuleMap implements "module map" spec.
// https://html.spec.whatwg.org/#module-map
class CORE_EXPORT ModuleMap final : public GarbageCollected<ModuleMap>,
                                    public TraceWrapperBase {
  WTF_MAKE_NONCOPYABLE(ModuleMap);
  class Entry;

 public:
  static ModuleMap* Create(Modulator* modulator) {
    return new ModuleMap(modulator);
  }
  void Trace(blink::Visitor*);
  void TraceWrappers(const ScriptWrappableVisitor*) const;

  // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-single-module-script
  void FetchSingleModuleScript(const ModuleScriptFetchRequest&,
                               ModuleGraphLevel,
                               SingleModuleClient*);

  // Synchronously get the ModuleScript for a given URL.
  // If the URL wasn't fetched, or is currently being fetched, this returns a
  // nullptr.
  ModuleScript* GetFetchedModuleScript(const KURL&) const;

  Modulator* GetModulator() { return modulator_; }

 private:
  explicit ModuleMap(Modulator*);

  using MapImpl = HeapHashMap<KURL, TraceWrapperMember<Entry>>;

  // A module map is a map of absolute URLs to map entry.
  MapImpl map_;

  Member<Modulator> modulator_;
};

}  // namespace blink

#endif
