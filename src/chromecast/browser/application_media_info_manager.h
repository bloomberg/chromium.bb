// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_APPLICATION_MEDIA_INFO_MANAGER_H_
#define CHROMECAST_BROWSER_APPLICATION_MEDIA_INFO_MANAGER_H_

#include <string>

#include "base/macros.h"
#include "content/public/browser/frame_service_base.h"
#include "media/mojo/interfaces/cast_application_media_info_manager.mojom.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace chromecast {
namespace media {

class ApplicationMediaInfoManager
    : public ::content::FrameServiceBase<
          ::media::mojom::CastApplicationMediaInfoManager> {
 public:
  ApplicationMediaInfoManager(
      content::RenderFrameHost* render_frame_host,
      ::media::mojom::CastApplicationMediaInfoManagerRequest request,
      std::string application_session_id,
      bool mixer_audio_enabled);
  ~ApplicationMediaInfoManager() override;

 private:
  // ::media::mojom::CastApplicationMediaInfoManager implementation:
  void GetCastApplicationMediaInfo(
      GetCastApplicationMediaInfoCallback callback) final;

  const std::string application_session_id_;
  bool mixer_audio_enabled_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationMediaInfoManager);
};

void CreateApplicationMediaInfoManager(
    content::RenderFrameHost* render_frame_host,
    std::string application_session_id,
    bool mixer_audio_enabled,
    ::media::mojom::CastApplicationMediaInfoManagerRequest request);

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_APPLICATION_MEDIA_INFO_MANAGER_H_
