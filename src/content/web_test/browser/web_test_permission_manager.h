// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_WEB_TEST_PERMISSION_MANAGER_H_
#define CONTENT_WEB_TEST_BROWSER_WEB_TEST_PERMISSION_MANAGER_H_

#include <stddef.h>

#include "base/callback_forward.h"
#include "base/containers/id_map.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_automation.mojom.h"
#include "url/gurl.h"

namespace content {

class WebTestPermissionManager
    : public PermissionControllerDelegate,
      public blink::test::mojom::PermissionAutomation {
 public:
  WebTestPermissionManager();

  WebTestPermissionManager(const WebTestPermissionManager&) = delete;
  WebTestPermissionManager& operator=(const WebTestPermissionManager&) = delete;

  ~WebTestPermissionManager() override;

  // PermissionManager overrides.
  void RequestPermission(
      PermissionType permission,
      RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      base::OnceCallback<void(blink::mojom::PermissionStatus)> callback)
      override;
  void RequestPermissions(
      const std::vector<PermissionType>& permission,
      RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback)
      override;
  void ResetPermission(PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatus(
      PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;
  blink::mojom::PermissionStatus GetPermissionStatusForFrame(
      content::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin) override;
  SubscriptionId SubscribePermissionStatusChange(
      PermissionType permission,
      RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback)
      override;
  void UnsubscribePermissionStatusChange(
      SubscriptionId subscription_id) override;

  void SetPermission(PermissionType permission,
                     blink::mojom::PermissionStatus status,
                     const GURL& url,
                     const GURL& embedding_url);
  void ResetPermissions();

  // blink::test::mojom::PermissionAutomation
  void SetPermission(
      blink::mojom::PermissionDescriptorPtr descriptor,
      blink::mojom::PermissionStatus status,
      const GURL& url,
      const GURL& embedding_url,
      blink::test::mojom::PermissionAutomation::SetPermissionCallback) override;

  void Bind(
      mojo::PendingReceiver<blink::test::mojom::PermissionAutomation> receiver);

 private:
  // Representation of a permission for the WebTestPermissionManager.
  struct PermissionDescription {
    PermissionDescription() = default;
    PermissionDescription(PermissionType type,
                          const GURL& origin,
                          const GURL& embedding_origin);
    bool operator==(const PermissionDescription& other) const;
    bool operator!=(const PermissionDescription& other) const;

    // Hash operator for hash maps.
    struct Hash {
      size_t operator()(const PermissionDescription& description) const;
    };

    PermissionType type;
    GURL origin;
    GURL embedding_origin;
  };

  struct Subscription;
  using SubscriptionsMap =
      base::IDMap<std::unique_ptr<Subscription>, SubscriptionId>;
  using PermissionsMap = std::unordered_map<PermissionDescription,
                                            blink::mojom::PermissionStatus,
                                            PermissionDescription::Hash>;

  void OnPermissionChanged(const PermissionDescription& permission,
                           blink::mojom::PermissionStatus status);

  // Mutex for permissions access. Unfortunately, the permissions can be
  // accessed from the IO thread because of Notifications' synchronous IPC.
  base::Lock permissions_lock_;

  // List of permissions currently known by the WebTestPermissionManager and
  // their associated |PermissionStatus|.
  PermissionsMap permissions_;

  // List of subscribers currently listening to permission changes.
  SubscriptionsMap subscriptions_;
  SubscriptionId::Generator subscription_id_generator_;

  mojo::ReceiverSet<blink::test::mojom::PermissionAutomation> receivers_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_WEB_TEST_PERMISSION_MANAGER_H_
