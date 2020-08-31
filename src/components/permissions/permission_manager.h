// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_MANAGER_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_MANAGER_H_

#include <unordered_map>

#include "base/callback_forward.h"
#include "base/containers/id_map.h"
#include "base/macros.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/permission_type.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
}

namespace permissions {
class PermissionContextBase;
struct PermissionResult;

class PermissionManager : public KeyedService,
                          public content::PermissionControllerDelegate,
                          public content_settings::Observer {
 public:
  using PermissionContextMap =
      std::unordered_map<ContentSettingsType,
                         std::unique_ptr<PermissionContextBase>,
                         ContentSettingsTypeHash>;
  PermissionManager(content::BrowserContext* browser_context,
                    PermissionContextMap permission_contexts);
  ~PermissionManager() override;

  // Converts from |url|'s actual origin to the "canonical origin" that should
  // be used for the purpose of requesting/storing permissions. For example, the
  // origin of the local NTP gets mapped to the Google base URL instead. With
  // Permission Delegation it will transform the requesting origin into
  // the embedding origin because all permission checks happen on the top level
  // origin.
  //
  // All the public methods below, such as RequestPermission or
  // GetPermissionStatus, take the actual origin and do the canonicalization
  // internally. You only need to call this directly if you do something else
  // with the origin, such as display it in the UI.
  GURL GetCanonicalOrigin(ContentSettingsType permission,
                          const GURL& requesting_origin,
                          const GURL& embedding_origin) const;

  // Callers from within chrome/ should use the methods which take the
  // ContentSettingsType enum. The methods which take PermissionType values
  // are for the content::PermissionControllerDelegate overrides and shouldn't
  // be used from chrome/.

  int RequestPermission(ContentSettingsType permission,
                        content::RenderFrameHost* render_frame_host,
                        const GURL& requesting_origin,
                        bool user_gesture,
                        base::OnceCallback<void(ContentSetting)> callback);
  int RequestPermissions(
      const std::vector<ContentSettingsType>& permissions,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      base::OnceCallback<void(const std::vector<ContentSetting>&)> callback);

  PermissionResult GetPermissionStatus(ContentSettingsType permission,
                                       const GURL& requesting_origin,
                                       const GURL& embedding_origin);

  // Returns the permission status for a given frame. This should be preferred
  // over GetPermissionStatus as additional checks can be performed when we know
  // the exact context the request is coming from.
  // TODO(raymes): Currently we still pass the |requesting_origin| as a separate
  // parameter because we can't yet guarantee that it matches the last committed
  // origin of the RenderFrameHost. See crbug.com/698985.
  PermissionResult GetPermissionStatusForFrame(
      ContentSettingsType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin);

  // content::PermissionControllerDelegate implementation.
  int RequestPermission(content::PermissionType permission,
                        content::RenderFrameHost* render_frame_host,
                        const GURL& requesting_origin,
                        bool user_gesture,
                        base::OnceCallback<void(blink::mojom::PermissionStatus)>
                            callback) override;
  int RequestPermissions(
      const std::vector<content::PermissionType>& permissions,
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
  bool IsPermissionOverridableByDevTools(
      content::PermissionType permission,
      const base::Optional<url::Origin>& origin) override;
  int SubscribePermissionStatusChange(
      content::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback)
      override;
  void UnsubscribePermissionStatusChange(int subscription_id) override;

  // TODO(raymes): Rather than exposing this, use the denial reason from
  // GetPermissionStatus in callers to determine whether a permission is
  // denied due to the kill switch.
  bool IsPermissionKillSwitchOn(ContentSettingsType);

  // For the given |origin|, overrides permissions that belong to |overrides|.
  // These permissions are in-sync with the PermissionController.
  void SetPermissionOverridesForDevTools(
      const base::Optional<url::Origin>& origin,
      const PermissionOverrides& overrides) override;
  void ResetPermissionOverridesForDevTools() override;

  // KeyedService implementation
  void Shutdown() override;

  // Helper method to convert PermissionType to ContentSettingType.
  static ContentSettingsType PermissionTypeToContentSetting(
      content::PermissionType permission);

  PermissionContextBase* GetPermissionContextForTesting(
      ContentSettingsType type);

 private:
  friend class PermissionManagerTest;

  class PendingRequest;
  using PendingRequestsMap = base::IDMap<std::unique_ptr<PendingRequest>>;

  class PermissionResponseCallback;

  struct Subscription;
  using SubscriptionsMap = base::IDMap<std::unique_ptr<Subscription>>;

  PermissionContextBase* GetPermissionContext(ContentSettingsType type);

  // Called when a permission was decided for a given PendingRequest. The
  // PendingRequest is identified by its |request_id| and the permission is
  // identified by its |permission_id|. If the PendingRequest contains more than
  // one permission, it will wait for the remaining permissions to be resolved.
  // When all the permissions have been resolved, the PendingRequest's callback
  // is run.
  void OnPermissionsRequestResponseStatus(int request_id,
                                          int permission_id,
                                          ContentSetting status);

  // content_settings::Observer implementation.
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               const std::string& resource_identifier) override;

  PermissionResult GetPermissionStatusHelper(
      ContentSettingsType permission,
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin);

  ContentSetting GetPermissionOverrideForDevTools(
      const url::Origin& origin,
      ContentSettingsType permission);

  content::BrowserContext* browser_context_;
  PendingRequestsMap pending_requests_;
  SubscriptionsMap subscriptions_;

  PermissionContextMap permission_contexts_;
  using ContentSettingsTypeOverrides =
      base::flat_map<ContentSettingsType, ContentSetting>;
  std::map<url::Origin, ContentSettingsTypeOverrides>
      devtools_permission_overrides_;
  url::Origin devtools_global_overrides_origin_;

  bool is_shutting_down_ = false;

  DISALLOW_COPY_AND_ASSIGN(PermissionManager);
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_MANAGER_H_
