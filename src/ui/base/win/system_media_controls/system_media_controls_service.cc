// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/system_media_controls/system_media_controls_service.h"

#include "ui/base/win/system_media_controls/system_media_controls_service_impl.h"

namespace system_media_controls {

// static
SystemMediaControlsService* SystemMediaControlsService::GetInstance() {
  internal::SystemMediaControlsServiceImpl* impl =
      internal::SystemMediaControlsServiceImpl::GetInstance();
  if (impl->Initialize())
    return impl;
  return nullptr;
}

SystemMediaControlsService::~SystemMediaControlsService() = default;

}  // namespace system_media_controls
