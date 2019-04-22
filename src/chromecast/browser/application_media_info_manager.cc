// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/application_media_info_manager.h"

#include <utility>

namespace chromecast {
namespace media {

void CreateApplicationMediaInfoManager(
    content::RenderFrameHost* render_frame_host,
    std::string application_session_id,
    bool mixer_audio_enabled,
    ::media::mojom::CastApplicationMediaInfoManagerRequest request) {
  // The created ApplicationMediaInfoManager will be deleted on connection
  // error, or when the frame navigates away. See FrameServiceBase for details.
  new ApplicationMediaInfoManager(render_frame_host, std::move(request),
                                  std::move(application_session_id),
                                  mixer_audio_enabled);
}

ApplicationMediaInfoManager::ApplicationMediaInfoManager(
    content::RenderFrameHost* render_frame_host,
    ::media::mojom::CastApplicationMediaInfoManagerRequest request,
    std::string application_session_id,
    bool mixer_audio_enabled)
    : FrameServiceBase(render_frame_host, std::move(request)),
      application_session_id_(std::move(application_session_id)),
      mixer_audio_enabled_(mixer_audio_enabled) {}

ApplicationMediaInfoManager::~ApplicationMediaInfoManager() = default;

void ApplicationMediaInfoManager::GetCastApplicationMediaInfo(
    GetCastApplicationMediaInfoCallback callback) {
  std::move(callback).Run(::media::mojom::CastApplicationMediaInfo::New(
      application_session_id_, mixer_audio_enabled_));
}

}  // namespace media
}  // namespace chromecast
