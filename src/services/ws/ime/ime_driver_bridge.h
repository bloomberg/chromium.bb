// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_IME_IME_DRIVER_BRIDGE_H_
#define SERVICES_WS_IME_IME_DRIVER_BRIDGE_H_

#include <utility>

#include "base/containers/queue.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/ws/public/mojom/ime/ime.mojom.h"

namespace ws {

class IMEDriverBridge : public mojom::IMEDriver {
 public:
  IMEDriverBridge();
  ~IMEDriverBridge() override;

  void AddBinding(mojom::IMEDriverRequest request);
  void SetDriver(mojom::IMEDriverPtr driver);

 private:
  // Holds the params to start an IME session.
  struct Request;

  // mojom::IMEDriver:
  void StartSession(mojom::InputMethodRequest input_method_request,
                    mojom::TextInputClientPtr client,
                    mojom::SessionDetailsPtr details) override;

  mojo::BindingSet<mojom::IMEDriver> bindings_;
  mojom::IMEDriverPtr driver_;

  base::queue<Request> pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(IMEDriverBridge);
};

}  // namespace ws

#endif  // SERVICES_WS_IME_IME_DRIVER_BRIDGE_H_
