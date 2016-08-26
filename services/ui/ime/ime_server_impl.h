// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_IME_IME_SERVER_IMPL_H_
#define SERVICES_UI_IME_IME_SERVER_IMPL_H_

#include <utility>

#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/ui/public/interfaces/ime.mojom.h"

namespace shell {
class Connector;
}

namespace ui {

class IMEServerImpl : public mojom::IMEServer {
 public:
  IMEServerImpl();
  ~IMEServerImpl() override;

  void Init(shell::Connector* connector);
  void AddBinding(mojom::IMEServerRequest request);
  void OnDriverChanged(mojom::IMEDriverPtr driver);

 private:
  // mojom::IMEServer:
  void StartSession(mojom::TextInputClientPtr client,
                    mojom::InputMethodRequest input_method) override;

  mojo::BindingSet<mojom::IMEServer> bindings_;
  mojom::IMEDriverPtr driver_;
  int current_id_;

  using PendingRequest =
      std::pair<mojom::TextInputClientPtr, mojom::InputMethodRequest>;
  std::queue<PendingRequest> pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(IMEServerImpl);
};

}  // namespace ui

#endif  // SERVICES_UI_IME_IME_SERVER_IMPL_H_
