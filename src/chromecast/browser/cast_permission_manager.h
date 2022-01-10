// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_PERMISSION_MANAGER_H_
#define CHROMECAST_BROWSER_CAST_PERMISSION_MANAGER_H_

#include "base/callback_forward.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "url/gurl.h"

namespace chromecast {
namespace shell {

class CastPermissionManager : public content::PermissionControllerDelegate {
 public:
  CastPermissionManager();

  CastPermissionManager(const CastPermissionManager&) = delete;
  CastPermissionManager& operator=(const CastPermissionManager&) = delete;

  ~CastPermissionManager() override;

  // content::PermissionControllerDelegate implementation:
  void RequestPermission(
      content::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      base::OnceCallback<void(blink::mojom::PermissionStatus)> callback)
      override;
  void RequestPermissions(
      const std::vector<content::PermissionType>& permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback)
      override;
  void ResetPermission(content::PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatus(
      content::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatusForFrame(
      content::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin) override;
  SubscriptionId SubscribePermissionStatusChange(
      content::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback)
      override;
  void UnsubscribePermissionStatusChange(
      SubscriptionId subscription_id) override;
};

}  // namespace shell
}  // namespace chromecast

#endif // CHROMECAST_BROWSER_CAST_PERMISSION_MANAGER_H_
