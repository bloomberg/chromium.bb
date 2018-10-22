// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/test/noop_dual_media_sink_service.h"

#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service.h"

namespace media_router {

NoopDualMediaSinkService::NoopDualMediaSinkService()
    : DualMediaSinkService(std::unique_ptr<CastMediaSinkService>(nullptr),
                           std::unique_ptr<DialMediaSinkService>(nullptr)) {}
NoopDualMediaSinkService::~NoopDualMediaSinkService() = default;

}  // namespace media_router
