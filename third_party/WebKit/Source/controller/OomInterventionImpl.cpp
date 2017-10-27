// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "controller/OomInterventionImpl.h"

#include "core/page/ScopedPagePauser.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace blink {

// static
void OomInterventionImpl::Create(mojom::blink::OomInterventionRequest request) {
  mojo::MakeStrongBinding(std::make_unique<OomInterventionImpl>(),
                          std::move(request));
}

OomInterventionImpl::OomInterventionImpl() = default;

OomInterventionImpl::~OomInterventionImpl() = default;

void OomInterventionImpl::OnNearOomDetected(
    OnNearOomDetectedCallback callback) {
  if (!pauser_) {
    pauser_.reset(new ScopedPagePauser());
  }
  std::move(callback).Run();
}

void OomInterventionImpl::OnInterventionDeclined(
    OnInterventionDeclinedCallback callback) {
  pauser_.reset();
  std::move(callback).Run();
}

}  // namespace blink
