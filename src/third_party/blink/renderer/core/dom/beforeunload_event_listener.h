// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_BEFOREUNLOAD_EVENT_LISTENER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_BEFOREUNLOAD_EVENT_LISTENER_H_

#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

class Document;
class Event;
class ExecutionContext;
class Visitor;

// Helper class used to setup beforeunload listener for certain documents which
// include MimeHandlerView (some PluginDocuments and HTMLDocuments).
class BeforeUnloadEventListener : public NativeEventListener {
 public:
  static BeforeUnloadEventListener* Create(Document* document) {
    return MakeGarbageCollected<BeforeUnloadEventListener>(document);
  }

  explicit BeforeUnloadEventListener(Document*);

  void SetShowBeforeUnloadDialog(bool show_dialog) {
    show_dialog_ = show_dialog;
  }

  void Trace(Visitor* visitor) override;

 private:
  void Invoke(ExecutionContext*, Event* event) override;

  Member<Document> doc_;
  bool show_dialog_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_BEFOREUNLOAD_EVENT_LISTENER_H_
