// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/nfc_host.h"

#include <utility>

#include "base/atomic_sequence_num.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/public/android/content_jni_headers/NfcHost_jni.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/device/public/mojom/nfc.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace content {

namespace {
base::AtomicSequenceNumber g_unique_id;
}  // namespace

NFCHost::NFCHost(WebContents* web_contents)
    : WebContentsObserver(web_contents) {
  DCHECK(web_contents);

  permission_controller_ = PermissionControllerImpl::FromBrowserContext(
      web_contents->GetBrowserContext());
}

NFCHost::~NFCHost() {
  Close();
}

void NFCHost::GetNFC(RenderFrameHost* render_frame_host,
                     mojo::PendingReceiver<device::mojom::NFC> receiver) {
  // https://w3c.github.io/web-nfc/#security-policies
  // WebNFC API must be only accessible from top level browsing context.
  if (render_frame_host->GetParent()) {
    mojo::ReportBadMessage(
        "WebNFC is only allowed in a top-level browsing context.");
    return;
  }

  GURL origin_url = render_frame_host->GetLastCommittedOrigin().GetURL();
  if (permission_controller_->GetPermissionStatus(PermissionType::NFC,
                                                  origin_url, origin_url) !=
      blink::mojom::PermissionStatus::GRANTED) {
    return;
  }

  // base::Unretained() is safe here because the subscription is canceled when
  // this object is destroyed.
  subscription_id_ = permission_controller_->SubscribePermissionStatusChange(
      PermissionType::NFC, render_frame_host, origin_url,
      base::BindRepeating(&NFCHost::OnPermissionStatusChange,
                          base::Unretained(this)));

  if (!nfc_provider_) {
    content::GetDeviceService().BindNFCProvider(
        nfc_provider_.BindNewPipeAndPassReceiver());
  }

  JNIEnv* env = base::android::AttachCurrentThread();

  // The created instance's reference is kept inside a map in Java world.
  int id = g_unique_id.GetNext();
  Java_NfcHost_create(env, web_contents()->GetJavaWebContents(), id);

  // Connect to an NFC object, associating it with |id_|.
  nfc_provider_->GetNFCForHost(id, std::move(receiver));
}

void NFCHost::RenderFrameHostChanged(RenderFrameHost* old_host,
                                     RenderFrameHost* new_host) {
  // If the main frame has been replaced then close an old NFC connection.
  if (!new_host->GetParent())
    Close();
}

void NFCHost::OnVisibilityChanged(Visibility visibility) {
  // For cases NFC not initialized, such as the permission has been revoked.
  if (!nfc_provider_)
    return;

  // NFC operations should be suspended.
  // https://w3c.github.io/web-nfc/#nfc-suspended
  if (visibility == Visibility::VISIBLE)
    nfc_provider_->ResumeNFCOperations();
  else
    nfc_provider_->SuspendNFCOperations();
}

void NFCHost::OnPermissionStatusChange(blink::mojom::PermissionStatus status) {
  if (status != blink::mojom::PermissionStatus::GRANTED)
    Close();
}

void NFCHost::Close() {
  nfc_provider_.reset();
  if (subscription_id_ != 0)
    permission_controller_->UnsubscribePermissionStatusChange(subscription_id_);
}

}  // namespace content
