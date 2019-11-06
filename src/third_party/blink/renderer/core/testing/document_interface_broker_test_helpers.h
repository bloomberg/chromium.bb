// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_DOCUMENT_INTERFACE_BROKER_TEST_HELPERS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_DOCUMENT_INTERFACE_BROKER_TEST_HELPERS_H_

#include <utility>

#include "third_party/blink/public/mojom/frame/document_interface_broker.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame_host_test_interface.mojom-blink.h"
#include "third_party/blink/renderer/core/testing/test_document_interface_broker.h"

namespace blink {

constexpr char kGetNameTestResponse[] = "BlinkTestName";

// These classes can be used for verifying that overriding mechanism for
// DocumentInterfaceBroker method calls works as expected with LocalFrameClient
// implementations. See frame_test.cc and local_frame_client_impl.cc for
// examples.
class FrameHostTestInterfaceImpl : public mojom::blink::FrameHostTestInterface {
 public:
  FrameHostTestInterfaceImpl() : binding_(this) {}
  ~FrameHostTestInterfaceImpl() override {}

  void BindAndFlush(mojom::blink::FrameHostTestInterfaceRequest request);

 protected:
  void Ping(const KURL& url, const WTF::String& event) override {}
  void GetName(GetNameCallback callback) override;

 private:
  mojo::Binding<mojom::blink::FrameHostTestInterface> binding_;

  DISALLOW_COPY_AND_ASSIGN(FrameHostTestInterfaceImpl);
};

class FrameHostTestDocumentInterfaceBroker
    : public TestDocumentInterfaceBroker {
 public:
  FrameHostTestDocumentInterfaceBroker(
      mojom::blink::DocumentInterfaceBroker* document_interface_broker,
      mojom::blink::DocumentInterfaceBrokerRequest request)
      : TestDocumentInterfaceBroker(document_interface_broker,
                                    std::move(request)) {}

  void GetFrameHostTestInterface(
      mojom::blink::FrameHostTestInterfaceRequest request) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_DOCUMENT_INTERFACE_BROKER_TEST_HELPERS_H_
