// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/ime/ime_driver_bridge.h"

#include "services/service_manager/public/cpp/connector.h"
#include "services/ws/ime/ime_registrar_impl.h"

namespace ws {

struct IMEDriverBridge::Request {
  Request() = default;
  Request(Request&& other) = default;
  Request(mojom::InputMethodRequest input_method_request,
          mojom::TextInputClientPtr client,
          mojom::SessionDetailsPtr details)
      : input_method_request(std::move(input_method_request)),
        client(std::move(client)),
        details(std::move(details)) {}
  ~Request() = default;

  mojom::InputMethodRequest input_method_request;
  mojom::TextInputClientPtr client;
  mojom::SessionDetailsPtr details;
};

IMEDriverBridge::IMEDriverBridge() = default;
IMEDriverBridge::~IMEDriverBridge() = default;

void IMEDriverBridge::AddBinding(mojom::IMEDriverRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void IMEDriverBridge::SetDriver(mojom::IMEDriverPtr driver) {
  // TODO(moshayedi): crbug.com/669681. Handle switching drivers properly. For
  // now we only register the first driver to avoid clients of the previous
  // driver from hanging.
  if (driver_)
    return;

  driver_ = std::move(driver);

  while (!pending_requests_.empty()) {
    auto& request = pending_requests_.front();
    driver_->StartSession(std::move(request.input_method_request),
                          std::move(request.client),
                          std::move(request.details));
    pending_requests_.pop();
  }
}

void IMEDriverBridge::StartSession(
    mojom::InputMethodRequest input_method_request,
    mojom::TextInputClientPtr client,
    mojom::SessionDetailsPtr details) {
  if (driver_.get()) {
    // TODO(moshayedi): crbug.com/634431. This will forward all calls from
    // clients to the driver as they are. We may need to check |caret_bounds|
    // parameter of InputMethod::OnCaretBoundsChanged() here and limit them to
    // client's focused window.
    driver_->StartSession(std::move(input_method_request), std::move(client),
                          std::move(details));
  } else {
    pending_requests_.push(Request(std::move(input_method_request),
                                   std::move(client), std::move(details)));
  }
}

}  // namespace ws
