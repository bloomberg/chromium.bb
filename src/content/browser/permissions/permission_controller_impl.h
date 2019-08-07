// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PERMISSIONS_PERMISSION_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_PERMISSIONS_PERMISSION_CONTROLLER_IMPL_H_

#include "base/containers/id_map.h"
#include "content/common/content_export.h"
#include "content/public/browser/permission_controller.h"
#include "url/gurl.h"

namespace content {

class BrowserContext;

// Implementation of the PermissionController interface. This
// is used by content/ layer to manage permissions.
// There is one instance of this class per BrowserContext.
class CONTENT_EXPORT PermissionControllerImpl : public PermissionController {
 public:
  explicit PermissionControllerImpl(BrowserContext* browser_context);
  ~PermissionControllerImpl() override;

  static PermissionControllerImpl* FromBrowserContext(
      BrowserContext* browser_context);

  using PermissionOverrides = std::set<PermissionType>;
  // For the given |origin|, grant permissions that belong to |overrides|
  // and reject all others.
  void SetPermissionOverridesForDevTools(const GURL& origin,
                                         const PermissionOverrides& overrides);
  void ResetPermissionOverridesForDevTools();

  // PermissionController implementation.
  blink::mojom::PermissionStatus GetPermissionStatus(
      PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;

  blink::mojom::PermissionStatus GetPermissionStatusForFrame(
      PermissionType permission,
      RenderFrameHost* render_frame_host,
      const GURL& requesting_origin) override;

  int RequestPermission(
      PermissionType permission,
      RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      const base::Callback<void(blink::mojom::PermissionStatus)>& callback);

  int RequestPermissions(
      const std::vector<PermissionType>& permission,
      RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      const base::Callback<
          void(const std::vector<blink::mojom::PermissionStatus>&)>& callback);

  void ResetPermission(PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin);

  int SubscribePermissionStatusChange(
      PermissionType permission,
      RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const base::Callback<void(blink::mojom::PermissionStatus)>& callback);

  void UnsubscribePermissionStatusChange(int subscription_id);

 private:
  struct Subscription;
  using SubscriptionsMap = base::IDMap<std::unique_ptr<Subscription>>;

  blink::mojom::PermissionStatus GetSubscriptionCurrentValue(
      const Subscription& subscription);
  void OnDelegatePermissionStatusChange(Subscription* subscription,
                                        blink::mojom::PermissionStatus status);

  std::map<GURL, PermissionOverrides> devtools_permission_overrides_;
  SubscriptionsMap subscriptions_;
  BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(PermissionControllerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PERMISSIONS_PERMISSION_CONTROLLER_IMPL_H_
