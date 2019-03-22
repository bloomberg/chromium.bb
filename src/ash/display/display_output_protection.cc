// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_output_protection.h"

#include <utility>

#include "ui/display/manager/display_configurator.h"

using display::DisplayConfigurator;

namespace ash {

// A BindingContext is created for each bind request. BindingContext contains
// the state unique to each connection.
class DisplayOutputProtection::BindingContext {
 public:
  explicit BindingContext(DisplayConfigurator* display_configurator)
      : display_configurator_(display_configurator),
        client_id_(display_configurator_->RegisterContentProtectionClient()) {}

  ~BindingContext() {
    display_configurator_->UnregisterContentProtectionClient(client_id_);
  }

  uint64_t client_id() const { return client_id_; }

 private:
  display::DisplayConfigurator* const display_configurator_;
  const uint64_t client_id_;

  DISALLOW_COPY_AND_ASSIGN(BindingContext);
};

DisplayOutputProtection::DisplayOutputProtection(
    DisplayConfigurator* display_configurator)
    : display_configurator_(display_configurator) {}

DisplayOutputProtection::~DisplayOutputProtection() = default;

void DisplayOutputProtection::BindRequest(
    mojom::DisplayOutputProtectionRequest request) {
  std::unique_ptr<BindingContext> context =
      std::make_unique<BindingContext>(display_configurator_);
  bindings_.AddBinding(this, std::move(request), std::move(context));
}

void DisplayOutputProtection::QueryContentProtectionStatus(
    int64_t display_id,
    QueryContentProtectionStatusCallback callback) {
  display_configurator_->QueryContentProtectionStatus(
      bindings_.dispatch_context()->client_id(), display_id,
      std::move(callback));
}

void DisplayOutputProtection::SetContentProtection(
    int64_t display_id,
    uint32_t desired_method_mask,
    SetContentProtectionCallback callback) {
  display_configurator_->SetContentProtection(
      bindings_.dispatch_context()->client_id(), display_id,
      desired_method_mask, std::move(callback));
}

}  // namespace ash
