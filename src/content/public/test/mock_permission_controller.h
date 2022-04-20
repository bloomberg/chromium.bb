// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_PERMISSION_CONTROLLER_H_
#define CONTENT_PUBLIC_TEST_MOCK_PERMISSION_CONTROLLER_H_

#include "content/public/browser/permission_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

class GURL;

namespace url {
class Origin;
}

namespace content {

enum class PermissionType;

// Mock of the permission controller for unit tests.
class MockPermissionController : public PermissionController {
 public:
  MockPermissionController();

  MockPermissionController(const MockPermissionController&) = delete;
  MockPermissionController& operator=(const MockPermissionController&) = delete;

  ~MockPermissionController() override;

  // PermissionController:
  MOCK_METHOD3(DeprecatedGetPermissionStatus,
               blink::mojom::PermissionStatus(PermissionType permission,
                                              const GURL& requesting_origin,
                                              const GURL& embedding_origin));
  MOCK_METHOD3(
      GetPermissionStatusForWorker,
      blink::mojom::PermissionStatus(PermissionType permission,
                                     RenderProcessHost* render_process_host,
                                     const url::Origin& worker_origin));
  MOCK_METHOD2(
      GetPermissionStatusForCurrentDocument,
      blink::mojom::PermissionStatus(PermissionType permission,
                                     RenderFrameHost* render_frame_host));
  MOCK_METHOD2(
      GetPermissionStatusForOriginWithoutContext,
      blink::mojom::PermissionStatus(PermissionType permission,
                                     const url::Origin& requesting_origin));
  void RequestPermission(
      PermissionType permission,
      RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      base::OnceCallback<void(blink::mojom::PermissionStatus)> callback)
      override;
  void RequestPermissionFromCurrentDocument(
      PermissionType permission,
      RenderFrameHost* render_frame_host,
      bool user_gesture,
      base::OnceCallback<void(blink::mojom::PermissionStatus)> callback)
      override;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_PERMISSION_CONTROLLER_H_
