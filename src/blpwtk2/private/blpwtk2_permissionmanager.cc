#include <blpwtk2_permissionmanager.h>

#include "base/callback.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/web_contents.h"

namespace blpwtk2 {

namespace {

bool IsAllowlistedPermissionType(content::PermissionType permission) {
    switch (permission) {
        case content::PermissionType::CLIPBOARD_READ_WRITE:
        case content::PermissionType::CLIPBOARD_SANITIZED_WRITE:
            return true;

        default:
            return false;
    }

    NOTREACHED();
    return false;
}

}  // namespace

PermissionManager::PermissionManager() = default;
PermissionManager::~PermissionManager() {}

void PermissionManager::RequestPermission(
    content::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    base::OnceCallback<void(blink::mojom::PermissionStatus)> callback) {

    std::move(callback).Run(IsAllowlistedPermissionType(permission)
                                ? blink::mojom::PermissionStatus::GRANTED
                                : blink::mojom::PermissionStatus::DENIED);
}

void PermissionManager::RequestPermissions(
    const std::vector<content::PermissionType>& permissions,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {

    std::vector<blink::mojom::PermissionStatus> result;
    for (const auto& permission : permissions) {
        result.push_back(IsAllowlistedPermissionType(permission)
                            ? blink::mojom::PermissionStatus::GRANTED
                            : blink::mojom::PermissionStatus::DENIED);
    }
    std::move(callback).Run(result);
}

void PermissionManager::ResetPermission(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
}

blink::mojom::PermissionStatus PermissionManager::GetPermissionStatus(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {

  return IsAllowlistedPermissionType(permission)
             ? blink::mojom::PermissionStatus::GRANTED
             : blink::mojom::PermissionStatus::DENIED;
}

blink::mojom::PermissionStatus
PermissionManager::GetPermissionStatusForFrame(
    content::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin) {

  return IsAllowlistedPermissionType(permission)
             ? blink::mojom::PermissionStatus::GRANTED
             : blink::mojom::PermissionStatus::DENIED;
}

PermissionManager::SubscriptionId
PermissionManager::SubscribePermissionStatusChange(
    content::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback) {

  return SubscriptionId();
}

void PermissionManager::UnsubscribePermissionStatusChange(
    SubscriptionId subscription_id) {}

}  // namespace blpwtk2