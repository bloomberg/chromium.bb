// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ModuleScriptFetcher_h
#define ModuleScriptFetcher_h

#include "core/CoreExport.h"
#include "core/loader/modulescript/ModuleScriptCreationParams.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/wtf/Optional.h"

namespace blink {

class ConsoleMessage;

// ModuleScriptFetcher is an abstract class to fetch module scripts. Derived
// classes are expected to fetch a module script for the given FetchParameters
// and return its client a fetched resource as ModuleScriptCreationParams.
class CORE_EXPORT ModuleScriptFetcher
    : public GarbageCollectedFinalized<ModuleScriptFetcher> {
 public:
  class CORE_EXPORT Client : public GarbageCollectedMixin {
   public:
    virtual void NotifyFetchFinished(
        const WTF::Optional<ModuleScriptCreationParams>&,
        ConsoleMessage* error_message) = 0;
  };

  ModuleScriptFetcher() = default;
  virtual ~ModuleScriptFetcher() = default;

  // Takes a non-const reference to FetchParameters because
  // ScriptResource::Fetch() requires it.
  virtual void Fetch(FetchParameters&, Client*) = 0;

  virtual void Trace(blink::Visitor*);

 protected:
  void NotifyFetchFinished(const WTF::Optional<ModuleScriptCreationParams>&,
                           ConsoleMessage*);

  void SetClient(Client*);

 private:
  Member<Client> client_;
};

}  // namespace blink

#endif  // ModuleScriptFetcher_h
