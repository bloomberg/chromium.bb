// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BeforeInstallPromptEvent_h
#define BeforeInstallPromptEvent_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseProperty.h"
#include "core/frame/LocalFrame.h"
#include "modules/EventModules.h"
#include "modules/app_banner/AppBannerPromptResult.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "public/platform/modules/app_banner/app_banner.mojom-blink.h"
#include <utility>

namespace blink {

class BeforeInstallPromptEvent;
class BeforeInstallPromptEventInit;

using UserChoiceProperty =
    ScriptPromiseProperty<Member<BeforeInstallPromptEvent>,
                          Member<AppBannerPromptResult>,
                          ToV8UndefinedGenerator>;

class BeforeInstallPromptEvent final : public Event,
                                       public mojom::blink::AppBannerEvent {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(BeforeInstallPromptEvent, Dispose);

 public:
  ~BeforeInstallPromptEvent() override;

  static BeforeInstallPromptEvent* Create(
      const AtomicString& name,
      LocalFrame& frame,
      mojom::blink::AppBannerServicePtr service_ptr,
      mojom::blink::AppBannerEventRequest event_request,
      const Vector<String>& platforms) {
    return new BeforeInstallPromptEvent(name, frame, std::move(service_ptr),
                                        std::move(event_request), platforms);
  }

  static BeforeInstallPromptEvent* Create(
      const AtomicString& name,
      const BeforeInstallPromptEventInit& init) {
    return new BeforeInstallPromptEvent(name, init);
  }

  void Dispose();

  Vector<String> platforms() const;
  ScriptPromise userChoice(ScriptState*);
  ScriptPromise prompt(ScriptState*);

  const AtomicString& InterfaceName() const override;
  void preventDefault() override;

  DECLARE_VIRTUAL_TRACE();

 private:
  BeforeInstallPromptEvent(const AtomicString& name,
                           LocalFrame&,
                           mojom::blink::AppBannerServicePtr,
                           mojom::blink::AppBannerEventRequest,
                           const Vector<String>& platforms);
  BeforeInstallPromptEvent(const AtomicString& name,
                           const BeforeInstallPromptEventInit&);

  // mojom::blink::AppBannerEvent methods:
  void BannerAccepted(const String& platform) override;
  void BannerDismissed() override;

  mojom::blink::AppBannerServicePtr banner_service_;
  mojo::Binding<mojom::blink::AppBannerEvent> binding_;
  Vector<String> platforms_;
  Member<UserChoiceProperty> user_choice_;
  bool prompt_called_;
};

DEFINE_TYPE_CASTS(BeforeInstallPromptEvent,
                  Event,
                  event,
                  event->InterfaceName() ==
                      EventNames::BeforeInstallPromptEvent,
                  event.InterfaceName() ==
                      EventNames::BeforeInstallPromptEvent);

}  // namespace blink

#endif  // BeforeInstallPromptEvent_h
