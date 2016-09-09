// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/display/platform_screen_stub.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/gfx/geometry/rect.h"

namespace display {
namespace {

const int64_t kDisplayId = 1;

}  // namespace

// static
std::unique_ptr<PlatformScreen> PlatformScreen::Create() {
  return base::MakeUnique<PlatformScreenStub>();
}

PlatformScreenStub::PlatformScreenStub() : weak_ptr_factory_(this) {}

PlatformScreenStub::~PlatformScreenStub() {}

void PlatformScreenStub::FixedSizeScreenConfiguration() {
  delegate_->OnDisplayAdded(kDisplayId, gfx::Rect(1024, 768));
}

void PlatformScreenStub::Init(PlatformScreenDelegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&PlatformScreenStub::FixedSizeScreenConfiguration,
                            weak_ptr_factory_.GetWeakPtr()));
}

int64_t PlatformScreenStub::GetPrimaryDisplayId() const {
  return kDisplayId;
}

}  // namespace display
