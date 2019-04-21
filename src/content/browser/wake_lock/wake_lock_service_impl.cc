// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/wake_lock/wake_lock_service_impl.h"

#include "content/public/browser/web_contents.h"
#include "services/device/public/mojom/wake_lock_context.mojom.h"

namespace content {

// static
void WakeLockServiceImpl::Create(RenderFrameHost* render_frame_host,
                                 blink::mojom::WakeLockServiceRequest request) {
  DCHECK(render_frame_host);
  new WakeLockServiceImpl(render_frame_host, std::move(request));
}

void WakeLockServiceImpl::GetWakeLock(device::mojom::WakeLockType type,
                                      device::mojom::WakeLockReason reason,
                                      const std::string& description,
                                      device::mojom::WakeLockRequest request) {
  device::mojom::WakeLockContext* wake_lock_context =
      web_contents()->GetWakeLockContext();

  if (!wake_lock_context)
    return;

  wake_lock_context->GetWakeLock(type, reason, description, std::move(request));
}

WakeLockServiceImpl::WakeLockServiceImpl(
    RenderFrameHost* render_frame_host,
    blink::mojom::WakeLockServiceRequest request)
    : FrameServiceBase(render_frame_host, std::move(request)) {}

}  // namespace content
