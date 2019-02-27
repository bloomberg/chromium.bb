// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/document_interface_broker_test_helpers.h"

#include <utility>

namespace blink {

void FrameHostTestInterfaceImpl::BindAndFlush(
    mojom::blink::FrameHostTestInterfaceRequest request) {
  binding_.Bind(std::move(request));
  binding_.WaitForIncomingMethodCall();
}

void FrameHostTestInterfaceImpl::GetName(GetNameCallback callback) {
  std::move(callback).Run(kGetNameTestResponse);
}

void FrameHostTestDocumentInterfaceBroker::GetFrameHostTestInterface(
    mojom::blink::FrameHostTestInterfaceRequest request) {
  FrameHostTestInterfaceImpl impl;
  impl.BindAndFlush(std::move(request));
}

}  // namespace blink
