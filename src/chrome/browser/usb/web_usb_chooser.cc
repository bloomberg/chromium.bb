// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/web_usb_chooser.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/usb/usb_chooser_controller.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"

WebUsbChooser::WebUsbChooser(content::RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(render_frame_host);
}

WebUsbChooser::~WebUsbChooser() {}

void WebUsbChooser::GetPermission(
    std::vector<device::mojom::UsbDeviceFilterPtr> device_filters,
    blink::mojom::WebUsbService::GetPermissionCallback callback) {
  DCHECK(!render_frame_host_->IsNestedWithinFencedFrame());
  url::Origin origin =
      render_frame_host_->GetMainFrame()->GetLastCommittedOrigin();
  auto* profile =
      Profile::FromBrowserContext(render_frame_host_->GetBrowserContext());
  auto* context = UsbChooserContextFactory::GetForProfile(profile);
  if (!context->CanRequestObjectPermission(origin)) {
    std::move(callback).Run(nullptr);
    return;
  }

  auto controller = std::make_unique<UsbChooserController>(
      render_frame_host_, std::move(device_filters), std::move(callback));
  ShowChooser(std::move(controller));
}
