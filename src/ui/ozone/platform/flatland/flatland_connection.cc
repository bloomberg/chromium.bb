// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_connection.h"

#include <lib/sys/cpp/component_context.h>

#include "base/bind.h"
#include "base/check.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/logging.h"

namespace ui {

FlatlandConnection::FlatlandConnection(const std::string& debug_name) {
  zx_status_t status =
      base::ComponentContextForProcess()
          ->svc()
          ->Connect<fuchsia::ui::composition::Flatland>(flatland_.NewRequest());
  if (status != ZX_OK) {
    ZX_LOG(FATAL, status) << "Failed to connect to Flatland";
  }
  flatland_->SetDebugName(debug_name);
  flatland_.events().OnError =
      fit::bind_member(this, &FlatlandConnection::OnError);
  flatland_.events().OnFramePresented =
      fit::bind_member(this, &FlatlandConnection::OnFramePresented);
  flatland_.events().OnNextFrameBegin =
      fit::bind_member(this, &FlatlandConnection::OnNextFrameBegin);
}

FlatlandConnection::~FlatlandConnection() = default;

void FlatlandConnection::Present() {
  fuchsia::ui::composition::PresentArgs present_args;
  present_args.set_requested_presentation_time(0);
  present_args.set_acquire_fences({});
  present_args.set_release_fences({});
  present_args.set_unsquashable(false);
  Present(std::move(present_args), base::BindOnce([](zx_time_t) {}));
}

void FlatlandConnection::Present(
    fuchsia::ui::composition::PresentArgs present_args,
    OnFramePresentedCallback callback) {
  // TODO(crbug.com/1230150): Consider making a more advanced present loop where
  // Presents are accumulated until OnNextFrameBegin().
  if (present_credits_ == 0) {
    present_after_receiving_credits_ = true;
    return;
  }
  --present_credits_;

  // In Flatland, release fences apply to the content of the previous present.
  // Keeping track of the previous frame's release fences and swapping ensure we
  // set the correct ones.
  present_args.mutable_release_fences()->swap(previous_present_release_fences_);

  flatland_->Present(std::move(present_args));
  presented_callbacks_.push(std::move(callback));
}

void FlatlandConnection::OnError(
    fuchsia::ui::composition::FlatlandError error) {
  LOG(ERROR) << "Flatland error: " << static_cast<int>(error);
  // TODO(crbug.com/1230150): Send error signal to the owners of this class.
}

void FlatlandConnection::OnNextFrameBegin(
    fuchsia::ui::composition::OnNextFrameBeginValues values) {
  present_credits_ += values.additional_present_credits();
  if (present_credits_ && present_after_receiving_credits_) {
    Present();
    present_after_receiving_credits_ = false;
  }
}

void FlatlandConnection::OnFramePresented(
    fuchsia::scenic::scheduling::FramePresentedInfo info) {
  for (size_t i = 0; i < info.presentation_infos.size(); ++i) {
    std::move(presented_callbacks_.front()).Run(info.actual_presentation_time);
    presented_callbacks_.pop();
  }
}

}  // namespace ui
