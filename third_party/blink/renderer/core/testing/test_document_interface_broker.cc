// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/test_document_interface_broker.h"

#include <utility>

namespace blink {

TestDocumentInterfaceBroker::TestDocumentInterfaceBroker(
    mojom::blink::DocumentInterfaceBroker* document_interface_broker,
    mojom::blink::DocumentInterfaceBrokerRequest request)
    : real_broker_(document_interface_broker),
      binding_(this, std::move(request)) {}

TestDocumentInterfaceBroker::~TestDocumentInterfaceBroker() {}

mojom::blink::DocumentInterfaceBroker*
TestDocumentInterfaceBroker::GetForwardingInterface() {
  return real_broker_;
}

void TestDocumentInterfaceBroker::Flush() {
  binding_.FlushForTesting();
}

}  // namespace blink
