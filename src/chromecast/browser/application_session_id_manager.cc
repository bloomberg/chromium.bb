// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/application_session_id_manager.h"

#include <utility>

namespace chromecast {
namespace media {

void CreateApplicationSessionIdManager(
    content::RenderFrameHost* render_frame_host,
    std::string application_session_id,
    ::media::mojom::ApplicationSessionIdManagerRequest request) {
  // The created CastApplicationSessionIdManager will return
  // the |application_session_id| that's passed in here.
  // The object will be deleted on connection error, or when the frame navigates
  // away. See FrameServiceBase for details.
  new CastApplicationSessionIdManager(render_frame_host, std::move(request),
                                      std::move(application_session_id));
}

CastApplicationSessionIdManager::CastApplicationSessionIdManager(
    content::RenderFrameHost* render_frame_host,
    ::media::mojom::ApplicationSessionIdManagerRequest request,
    std::string application_session_id)
    : FrameServiceBase(render_frame_host, std::move(request)),
      application_session_id_(std::move(application_session_id)) {}

void CastApplicationSessionIdManager::GetApplicationSessionId(
    GetApplicationSessionIdCallback callback) {
  std::move(callback).Run(application_session_id_);
}

}  // namespace media
}  // namespace chromecast