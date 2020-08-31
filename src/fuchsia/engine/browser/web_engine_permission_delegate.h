// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_PERMISSION_DELEGATE_H_
#define FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_PERMISSION_DELEGATE_H_

#include "content/public/browser/permission_controller_delegate.h"

// PermissionControllerDelegate implementation for WebEngine. It redirects
// permission redirects all calls to the appropriate FramePermissionController
// instance.
class WebEnginePermissionDelegate
    : public content::PermissionControllerDelegate {
 public:
  WebEnginePermissionDelegate();
  ~WebEnginePermissionDelegate() override;

  WebEnginePermissionDelegate(WebEnginePermissionDelegate&) = delete;
  WebEnginePermissionDelegate& operator=(WebEnginePermissionDelegate&) = delete;

  // content::PermissionControllerDelegate implementation:
  int RequestPermission(content::PermissionType permission,
                        content::RenderFrameHost* render_frame_host,
                        const GURL& requesting_origin,
                        bool user_gesture,
                        base::OnceCallback<void(blink::mojom::PermissionStatus)>
                            callback) override;
  int RequestPermissions(
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
  int SubscribePermissionStatusChange(
      content::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback)
      override;
  void UnsubscribePermissionStatusChange(int subscription_id) override;
};

#endif  // FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_PERMISSION_DELEGATE_H_
