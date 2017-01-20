// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ime/ime_server_impl.h"

#include "base/memory/ptr_util.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/ime/ime_registrar_impl.h"

namespace ui {

IMEServerImpl::IMEServerImpl() : current_id_(0) {}

IMEServerImpl::~IMEServerImpl() {}

void IMEServerImpl::Init(service_manager::Connector* connector,
                         bool is_test_config) {
  if (is_test_config)
    connector->Connect("test_ime_driver");
  // For non test configs we assume a client registers with us.
}

void IMEServerImpl::AddBinding(mojom::IMEServerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void IMEServerImpl::OnDriverChanged(mojom::IMEDriverPtr driver) {
  // TODO(moshayedi): crbug.com/669681. Handle switching drivers properly. For
  // now we only register the first driver to avoid clients of the previous
  // driver from hanging.
  if (driver_)
    return;

  driver_ = std::move(driver);

  while (!pending_requests_.empty()) {
    driver_->StartSession(current_id_++, std::move(pending_requests_.front()));
    pending_requests_.pop();
  }
}

void IMEServerImpl::StartSession(mojom::StartSessionDetailsPtr details) {
  if (driver_.get()) {
    // TODO(moshayedi): crbug.com/634431. This will forward all calls from
    // clients to the driver as they are. We may need to check |caret_bounds|
    // parameter of InputMethod::OnCaretBoundsChanged() here and limit them to
    // client's focused window.
    driver_->StartSession(current_id_++, std::move(details));
  } else {
    pending_requests_.push(std::move(details));
  }
}

}  // namespace ui
