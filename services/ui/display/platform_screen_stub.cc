// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/display/platform_screen_stub.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/shell/public/cpp/interface_registry.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace display {
namespace {

const int64_t kDisplayId = 1;
constexpr gfx::Size kDisplaySize(1024, 768);

}  // namespace

// static
std::unique_ptr<PlatformScreen> PlatformScreen::Create() {
  return base::MakeUnique<PlatformScreenStub>();
}

PlatformScreenStub::PlatformScreenStub() : weak_ptr_factory_(this) {}

PlatformScreenStub::~PlatformScreenStub() {}

void PlatformScreenStub::FixedSizeScreenConfiguration() {
  float device_scale_factor = 1.0f;
  if (Display::HasForceDeviceScaleFactor())
    device_scale_factor = Display::GetForcedDeviceScaleFactor();

  gfx::Size scaled_size =
      gfx::ScaleToRoundedSize(kDisplaySize, 1.0f / device_scale_factor);
  delegate_->OnDisplayAdded(kDisplayId, gfx::Rect(scaled_size), kDisplaySize,
                            device_scale_factor);
}

void PlatformScreenStub::AddInterfaces(shell::InterfaceRegistry* registry) {}

void PlatformScreenStub::Init(PlatformScreenDelegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&PlatformScreenStub::FixedSizeScreenConfiguration,
                            weak_ptr_factory_.GetWeakPtr()));
}

void PlatformScreenStub::RequestCloseDisplay(int64_t display_id) {
  if (display_id == kDisplayId) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&PlatformScreenDelegate::OnDisplayRemoved,
                              base::Unretained(delegate_), display_id));
  }
}

int64_t PlatformScreenStub::GetPrimaryDisplayId() const {
  return kDisplayId;
}

}  // namespace display
