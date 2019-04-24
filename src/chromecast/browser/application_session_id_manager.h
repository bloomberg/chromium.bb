// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_APPLICATION_SESSION_ID_MANAGER_H_
#define CHROMECAST_BROWSER_APPLICATION_SESSION_ID_MANAGER_H_

#include <string>

#include "base/macros.h"
#include "content/public/browser/frame_service_base.h"
#include "media/mojo/interfaces/application_session_id_manager.mojom.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace chromecast {
namespace media {

class CastApplicationSessionIdManager
    : public ::content::FrameServiceBase<
          ::media::mojom::ApplicationSessionIdManager> {
 public:
  CastApplicationSessionIdManager(
      content::RenderFrameHost* render_frame_host,
      ::media::mojom::ApplicationSessionIdManagerRequest request,
      std::string application_session_id);

 private:
  // ::media::mojom::ApplicationSessionIdManager implementation.
  void GetApplicationSessionId(GetApplicationSessionIdCallback callback) final;

  const std::string application_session_id_;

  DISALLOW_COPY_AND_ASSIGN(CastApplicationSessionIdManager);
};

void CreateApplicationSessionIdManager(
    content::RenderFrameHost* render_frame_host,
    std::string application_session_id,
    ::media::mojom::ApplicationSessionIdManagerRequest request);

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_APPLICATION_SESSION_ID_MANAGER_H_
