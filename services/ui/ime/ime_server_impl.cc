// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ime/ime_server_impl.h"

#include "services/ui/ime/ime_registrar_impl.h"

namespace ui {

IMEServerImpl::IMEServerImpl() : current_id_(0) {}

IMEServerImpl::~IMEServerImpl() {}

void IMEServerImpl::AddBinding(mojom::IMEServerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void IMEServerImpl::OnDriverChanged(mojom::IMEDriverPtr driver) {
  driver_ = std::move(driver);

  while (!pending_requests_.empty()) {
    driver_->StartSession(current_id_++,
                          std::move(pending_requests_.front().first),
                          std::move(pending_requests_.front().second));
    pending_requests_.pop();
  }
}

void IMEServerImpl::StartSession(
    mojom::TextInputClientPtr client,
    mojom::InputMethodRequest input_method_request) {
  if (driver_.get()) {
    // TODO(moshayedi): crbug.com/634431. This will forward all calls from
    // clients to the driver as they are. We may need to check |caret_bounds|
    // parameter of InputMethod::OnCaretBoundsChanged() here and limit them to
    // client's focused window.
    driver_->StartSession(current_id_++, std::move(client),
                          std::move(input_method_request));
  } else {
    pending_requests_.push(
        std::make_pair(std::move(client), std::move(input_method_request)));
  }
}

}  // namespace ui
