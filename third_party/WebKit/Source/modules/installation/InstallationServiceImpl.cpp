// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/installation/InstallationServiceImpl.h"

#include <utility>
#include "core/dom/events/Event.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

InstallationServiceImpl::InstallationServiceImpl(LocalFrame& frame)
    : frame_(frame) {}

// static
void InstallationServiceImpl::Create(
    LocalFrame* frame,
    mojom::blink::InstallationServiceRequest request) {
  mojo::MakeStrongBinding(WTF::MakeUnique<InstallationServiceImpl>(*frame),
                          std::move(request));
}

void InstallationServiceImpl::OnInstall() {
  if (!frame_)
    return;

  LocalDOMWindow* dom_window = frame_->DomWindow();
  if (!dom_window)
    return;

  dom_window->DispatchEvent(Event::Create(EventTypeNames::appinstalled));
}

}  // namespace blink
