// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMWindowStorageController_h
#define DOMWindowStorageController_h

#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "modules/ModulesExport.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Document;

class MODULES_EXPORT DOMWindowStorageController final
    : public GarbageCollected<DOMWindowStorageController>,
      public Supplement<Document>,
      public LocalDOMWindow::EventListenerObserver {
  USING_GARBAGE_COLLECTED_MIXIN(DOMWindowStorageController);

 public:
  virtual void Trace(blink::Visitor*);

  static const char* SupplementName();
  static DOMWindowStorageController& From(Document&);

  // Inherited from LocalDOMWindow::EventListenerObserver
  void DidAddEventListener(LocalDOMWindow*, const AtomicString&) override;
  void DidRemoveEventListener(LocalDOMWindow*, const AtomicString&) override {}
  void DidRemoveAllEventListeners(LocalDOMWindow*) override {}

 private:
  explicit DOMWindowStorageController(Document&);
};

}  // namespace blink

#endif  // DOMWindowStorageController_h
