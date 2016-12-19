// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/installation/InstallationServiceImpl.h"

#include "core/events/Event.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "wtf/PtrUtil.h"
#include <utility>

namespace blink {

InstallationServiceImpl::InstallationServiceImpl(LocalFrame& frame)
    : m_frame(frame) {}

// static
void InstallationServiceImpl::create(
    LocalFrame* frame,
    mojom::blink::InstallationServiceRequest request) {
  mojo::MakeStrongBinding(WTF::makeUnique<InstallationServiceImpl>(*frame),
                          std::move(request));
}

void InstallationServiceImpl::OnInstall() {
  if (!m_frame)
    return;

  LocalDOMWindow* domWindow = m_frame->domWindow();
  if (!domWindow)
    return;

  domWindow->dispatchEvent(Event::create(EventTypeNames::appinstalled));
}

}  // namespace blink
