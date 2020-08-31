// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/frame_usb_services.h"

#include "build/build_config.h"
#include "chrome/browser/usb/usb_tab_helper.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/usb/web_usb_chooser_android.h"
#else
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/usb/web_usb_chooser_desktop.h"
#endif  // defined(OS_ANDROID)

using content::RenderFrameHost;
using content::WebContents;

namespace {

// The renderer performs its own feature policy checks so a request that gets
// to the browser process indicates malicious code.
const char kFeaturePolicyViolation[] =
    "Feature policy blocks access to WebUSB.";

}  // namespace

FrameUsbServices::FrameUsbServices(RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {
  // Create UsbTabHelper on creating FrameUsbServices.
  UsbTabHelper::CreateForWebContents(
      WebContents::FromRenderFrameHost(render_frame_host_));
}

FrameUsbServices::~FrameUsbServices() = default;

void FrameUsbServices::InitializeWebUsbChooser() {
  if (!usb_chooser_) {
    usb_chooser_.reset(
#if defined(OS_ANDROID)
        new WebUsbChooserAndroid(render_frame_host_));
#else
        new WebUsbChooserDesktop(render_frame_host_));
#endif  // defined(OS_ANDROID)
  }
}

void FrameUsbServices::InitializeWebUsbService(
    mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) {
  if (!AllowedByFeaturePolicy()) {
    mojo::ReportBadMessage(kFeaturePolicyViolation);
    return;
  }

  InitializeWebUsbChooser();
  if (!web_usb_service_) {
    web_usb_service_.reset(
        new WebUsbServiceImpl(render_frame_host_, usb_chooser_->GetWeakPtr()));
  }
  web_usb_service_->BindReceiver(std::move(receiver));
}

bool FrameUsbServices::AllowedByFeaturePolicy() const {
  return render_frame_host_->IsFeatureEnabled(
      blink::mojom::FeaturePolicyFeature::kUsb);
}

// static
void FrameUsbServices::CreateFrameUsbServices(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) {
  FrameUsbServices::CreateForCurrentDocument(render_frame_host);
  FrameUsbServices::GetForCurrentDocument(render_frame_host)
      ->InitializeWebUsbService(std::move(receiver));
}

RENDER_DOCUMENT_HOST_USER_DATA_KEY_IMPL(FrameUsbServices)
