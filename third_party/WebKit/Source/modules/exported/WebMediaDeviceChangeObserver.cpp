// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebMediaDeviceChangeObserver.h"

#include "core/dom/Document.h"
#include "modules/mediastream/MediaDevices.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebSecurityOrigin.h"

namespace blink {

WebMediaDeviceChangeObserver::WebMediaDeviceChangeObserver()
    : private_(nullptr) {}

WebMediaDeviceChangeObserver::WebMediaDeviceChangeObserver(bool unused)
    : private_(MediaDevices::Create(Document::CreateForTest())) {}

WebMediaDeviceChangeObserver::WebMediaDeviceChangeObserver(
    const WebMediaDeviceChangeObserver& other) {
  Assign(other);
}

WebMediaDeviceChangeObserver& WebMediaDeviceChangeObserver::operator=(
    const WebMediaDeviceChangeObserver& other) {
  Assign(other);
  return *this;
}

WebMediaDeviceChangeObserver::WebMediaDeviceChangeObserver(
    MediaDevices* observer)
    : private_(observer) {}

WebMediaDeviceChangeObserver::~WebMediaDeviceChangeObserver() {
  Reset();
}

bool WebMediaDeviceChangeObserver::IsNull() const {
  return private_.IsNull();
}

void WebMediaDeviceChangeObserver::DidChangeMediaDevices() {
  if (private_.IsNull())
    return;

  private_->DidChangeMediaDevices();
}

WebSecurityOrigin WebMediaDeviceChangeObserver::GetSecurityOrigin() const {
  if (private_.IsNull())
    return WebSecurityOrigin();

  return WebSecurityOrigin(
      private_->GetExecutionContext()->GetSecurityOrigin());
}

void WebMediaDeviceChangeObserver::Assign(
    const WebMediaDeviceChangeObserver& other) {
  private_ = other.private_;
}

void WebMediaDeviceChangeObserver::Reset() {
  private_.Reset();
}

}  // namespace blink
