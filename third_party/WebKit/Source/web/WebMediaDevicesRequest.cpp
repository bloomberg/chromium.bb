/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "public/web/WebMediaDevicesRequest.h"

#include "core/dom/Document.h"
#include "modules/mediastream/MediaDevicesRequest.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Vector.h"
#include "public/platform/WebMediaDeviceInfo.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"
#include "public/web/WebDocument.h"

namespace blink {

WebMediaDevicesRequest::WebMediaDevicesRequest(MediaDevicesRequest* request)
    : private_(request) {}

void WebMediaDevicesRequest::Reset() {
  private_.Reset();
}

WebSecurityOrigin WebMediaDevicesRequest::GetSecurityOrigin() const {
  DCHECK(!IsNull());
  DCHECK(private_->GetExecutionContext());
  return WebSecurityOrigin(
      private_->GetExecutionContext()->GetSecurityOrigin());
}

WebDocument WebMediaDevicesRequest::OwnerDocument() const {
  DCHECK(!IsNull());
  return WebDocument(private_->OwnerDocument());
}

void WebMediaDevicesRequest::RequestSucceeded(
    WebVector<WebMediaDeviceInfo> web_devices) {
  DCHECK(!IsNull());

  MediaDeviceInfoVector devices(web_devices.size());
  for (size_t i = 0; i < web_devices.size(); ++i)
    devices[i] = MediaDeviceInfo::Create(web_devices[i]);

  private_->Succeed(devices);
}

bool WebMediaDevicesRequest::Equals(const WebMediaDevicesRequest& other) const {
  if (IsNull() || other.IsNull())
    return false;
  return private_.Get() == other.private_.Get();
}

void WebMediaDevicesRequest::Assign(const WebMediaDevicesRequest& other) {
  private_ = other.private_;
}

WebMediaDevicesRequest::operator MediaDevicesRequest*() const {
  return private_.Get();
}

}  // namespace blink
