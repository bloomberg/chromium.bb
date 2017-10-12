// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/app_banner/AppBannerController.h"

#include <memory>
#include <utility>
#include "core/dom/Document.h"
#include "core/event_type_names.h"
#include "core/frame/DOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "modules/app_banner/BeforeInstallPromptEvent.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/Referrer.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

AppBannerController::AppBannerController(LocalFrame& frame) : frame_(frame) {}

void AppBannerController::BindMojoRequest(
    LocalFrame* frame,
    mojom::blink::AppBannerControllerRequest request) {
  DCHECK(frame);

  mojo::MakeStrongBinding(WTF::WrapUnique(new AppBannerController(*frame)),
                          std::move(request));
}

void AppBannerController::BannerPromptRequest(
    mojom::blink::AppBannerServicePtr service_ptr,
    mojom::blink::AppBannerEventRequest event_request,
    const Vector<String>& platforms,
    BannerPromptRequestCallback callback) {
  if (!frame_ || !frame_->GetDocument()) {
    std::move(callback).Run(mojom::blink::AppBannerPromptReply::NONE, "");
    return;
  }

  mojom::AppBannerPromptReply reply =
      frame_->DomWindow()->DispatchEvent(BeforeInstallPromptEvent::Create(
          EventTypeNames::beforeinstallprompt, *frame_, std::move(service_ptr),
          std::move(event_request), platforms)) ==
              DispatchEventResult::kNotCanceled
          ? mojom::AppBannerPromptReply::NONE
          : mojom::AppBannerPromptReply::CANCEL;

  AtomicString referrer = SecurityPolicy::GenerateReferrer(
                              frame_->GetDocument()->GetReferrerPolicy(),
                              KURL(), frame_->GetDocument()->OutgoingReferrer())
                              .referrer;

  std::move(callback).Run(reply, referrer.IsNull() ? g_empty_string : referrer);
}

}  // namespace blink
