// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorServiceWorker_h
#define NavigatorServiceWorker_h

#include "core/frame/Navigator.h"
#include "modules/ModulesExport.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Document;
class ExceptionState;
class Navigator;
class ScriptState;
class ServiceWorkerContainer;

class MODULES_EXPORT NavigatorServiceWorker final
    : public GarbageCollected<NavigatorServiceWorker>,
      public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorServiceWorker);

 public:
  static NavigatorServiceWorker* From(Document&);
  static NavigatorServiceWorker& From(Navigator&);
  static NavigatorServiceWorker* ToNavigatorServiceWorker(Navigator&);
  static ServiceWorkerContainer* serviceWorker(ScriptState*,
                                               Navigator&,
                                               ExceptionState&);
  static ServiceWorkerContainer* serviceWorker(ScriptState*,
                                               Navigator&,
                                               String& error_message);
  void ClearServiceWorker();

  virtual void Trace(blink::Visitor*);

 private:
  explicit NavigatorServiceWorker(Navigator&);
  ServiceWorkerContainer* serviceWorker(LocalFrame*, ExceptionState&);
  ServiceWorkerContainer* serviceWorker(LocalFrame*, String& error_message);

  static const char* SupplementName();

  Member<ServiceWorkerContainer> service_worker_;
};

}  // namespace blink

#endif  // NavigatorServiceWorker_h
