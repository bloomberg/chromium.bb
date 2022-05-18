// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_permission_manager.h"

#include "base/callback.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace headless {

HeadlessPermissionManager::HeadlessPermissionManager(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

HeadlessPermissionManager::~HeadlessPermissionManager() = default;

void HeadlessPermissionManager::RequestPermission(
    blink::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    base::OnceCallback<void(blink::mojom::PermissionStatus)> callback) {
  // In headless mode we just pretent the user "closes" any permission prompt,
  // without accepting or denying. Notifications are the exception to this,
  // which are explicitly disabled in Incognito mode.
  if (browser_context_->IsOffTheRecord() &&
      permission == blink::PermissionType::NOTIFICATIONS) {
    std::move(callback).Run(blink::mojom::PermissionStatus::DENIED);
    return;
  }

  std::move(callback).Run(blink::mojom::PermissionStatus::ASK);
}

void HeadlessPermissionManager::RequestPermissions(
    const std::vector<blink::PermissionType>& permissions,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {
  // In headless mode we just pretent the user "closes" any permission prompt,
  // without accepting or denying.
  std::vector<blink::mojom::PermissionStatus> result(
      permissions.size(), blink::mojom::PermissionStatus::ASK);
  std::move(callback).Run(result);
}

void HeadlessPermissionManager::ResetPermission(
    blink::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {}

void HeadlessPermissionManager::RequestPermissionsFromCurrentDocument(
    const std::vector<blink::PermissionType>& permissions,
    content::RenderFrameHost* render_frame_host,
    bool user_gesture,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {
  // In headless mode we just pretent the user "closes" any permission prompt,
  // without accepting or denying.
  std::vector<blink::mojom::PermissionStatus> result(
      permissions.size(), blink::mojom::PermissionStatus::ASK);
  std::move(callback).Run(result);
}

blink::mojom::PermissionStatus HeadlessPermissionManager::GetPermissionStatus(
    blink::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  return blink::mojom::PermissionStatus::ASK;
}

blink::mojom::PermissionStatus
HeadlessPermissionManager::GetPermissionStatusForCurrentDocument(
    blink::PermissionType permission,
    content::RenderFrameHost* render_frame_host) {
  return blink::mojom::PermissionStatus::ASK;
}

blink::mojom::PermissionStatus
HeadlessPermissionManager::GetPermissionStatusForWorker(
    blink::PermissionType permission,
    content::RenderProcessHost* render_process_host,
    const GURL& worker_origin) {
  return blink::mojom::PermissionStatus::ASK;
}

HeadlessPermissionManager::SubscriptionId
HeadlessPermissionManager::SubscribePermissionStatusChange(
    blink::PermissionType permission,
    content::RenderProcessHost* render_process_host,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback) {
  return SubscriptionId();
}

void HeadlessPermissionManager::UnsubscribePermissionStatusChange(
    SubscriptionId subscription_id) {}

}  // namespace headless
