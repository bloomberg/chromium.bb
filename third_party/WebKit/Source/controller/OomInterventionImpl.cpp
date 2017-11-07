// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "controller/OomInterventionImpl.h"

#include "mojo/public/cpp/bindings/strong_binding.h"

namespace blink {

// static
void OomInterventionImpl::Create(mojom::blink::OomInterventionRequest request) {
  mojo::MakeStrongBinding(std::make_unique<OomInterventionImpl>(),
                          std::move(request));
}

// The ScopedPagePauser is destryed when the intervention is declined and mojo
// strong binding is disconnected.
OomInterventionImpl::OomInterventionImpl() = default;

OomInterventionImpl::~OomInterventionImpl() = default;

}  // namespace blink
