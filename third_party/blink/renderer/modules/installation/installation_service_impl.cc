// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/installation/installation_service_impl.h"

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

InstallationServiceImpl::InstallationServiceImpl(LocalFrame& frame)
    : frame_(frame) {}

// static
void InstallationServiceImpl::Create(
    LocalFrame* frame,
    mojom::blink::InstallationServiceRequest request) {
  // See https://bit.ly/2S0zRAS for task types.
  mojo::MakeStrongBinding(std::make_unique<InstallationServiceImpl>(*frame),
                          std::move(request),
                          frame->GetTaskRunner(TaskType::kMiscPlatformAPI));
}

void InstallationServiceImpl::OnInstall() {
  if (!frame_)
    return;

  LocalDOMWindow* dom_window = frame_->DomWindow();
  if (!dom_window)
    return;

  dom_window->DispatchEvent(*Event::Create(event_type_names::kAppinstalled));
}

}  // namespace blink
