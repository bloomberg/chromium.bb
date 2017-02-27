// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ModuleMap_h
#define ModuleMap_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/KURLHash.h"
#include "wtf/HashMap.h"

namespace blink {

class Modulator;
class ModuleScript;
class ModuleScriptFetchRequest;
class SingleModuleClient;
enum class ModuleGraphLevel;

// A ModuleMap implements "module map" spec.
// https://html.spec.whatwg.org/#module-map
class CORE_EXPORT ModuleMap final : public GarbageCollected<ModuleMap> {
  WTF_MAKE_NONCOPYABLE(ModuleMap);
  class Entry;
  class LoaderHost;

 public:
  static ModuleMap* create(Modulator* modulator) {
    return new ModuleMap(modulator);
  }
  DECLARE_TRACE();

  // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-single-module-script
  void fetchSingleModuleScript(const ModuleScriptFetchRequest&,
                               ModuleGraphLevel,
                               SingleModuleClient*);

  // Synchronously get the ModuleScript for a given URL.
  // Note: fetchSingleModuleScript of the ModuleScript must be complete before
  // calling this.
  ModuleScript* getFetchedModuleScript(const KURL&) const;

  Modulator* modulator() { return m_modulator; }

 private:
  explicit ModuleMap(Modulator*);

  using MapImpl = HeapHashMap<KURL, Member<Entry>>;

  // A module map is a map of absolute URLs to map entry.
  MapImpl m_map;

  Member<Modulator> m_modulator;
};

}  // namespace blink

#endif
