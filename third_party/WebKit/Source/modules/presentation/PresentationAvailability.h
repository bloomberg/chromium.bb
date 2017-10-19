// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationAvailability_h
#define PresentationAvailability_h

#include "core/dom/SuspendableObject.h"
#include "core/dom/events/EventTarget.h"
#include "core/page/PageVisibilityObserver.h"
#include "modules/ModulesExport.h"
#include "modules/presentation/PresentationPromiseProperty.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Vector.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/presentation/WebPresentationAvailabilityObserver.h"

namespace blink {

class ExecutionContext;

// Expose whether there is a presentation display available for |url|. The
// object will be initialized with a default value passed via ::take() and will
// then subscribe to receive callbacks if the status for |url| were to
// change. The object will only listen to changes when required.
class MODULES_EXPORT PresentationAvailability final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<PresentationAvailability>,
      public SuspendableObject,
      public PageVisibilityObserver,
      public WebPresentationAvailabilityObserver {
  USING_GARBAGE_COLLECTED_MIXIN(PresentationAvailability);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PresentationAvailability* Take(PresentationAvailabilityProperty*,
                                        const WTF::Vector<KURL>&,
                                        bool);
  ~PresentationAvailability() override;

  // EventTarget implementation.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // WebPresentationAvailabilityObserver implementation.
  void AvailabilityChanged(blink::mojom::ScreenAvailability) override;
  const WebVector<WebURL>& Urls() const override;

  // ScriptWrappable implementation.
  bool HasPendingActivity() const final;

  // SuspendableObject implementation.
  void Suspend() override;
  void Resume() override;
  void ContextDestroyed(ExecutionContext*) override;

  // PageVisibilityObserver implementation.
  void PageVisibilityChanged() override;

  bool value() const;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(change);

  virtual void Trace(blink::Visitor*);

 protected:
  // EventTarget implementation.
  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) override;

 private:
  // Current state of the SuspendableObject. It is Active when created. It
  // becomes Suspended when suspend() is called and moves back to Active if
  // resume() is called. It becomes Inactive when stop() is called or at
  // destruction time.
  enum class State : char {
    kActive,
    kSuspended,
    kInactive,
  };

  PresentationAvailability(ExecutionContext*, const WTF::Vector<KURL>&, bool);

  void SetState(State);
  void UpdateListening();

  WebVector<WebURL> urls_;
  bool value_;
  State state_;
};

}  // namespace blink

#endif  // PresentationAvailability_h
