// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebContentSettingCallbacks.h"

#include <memory>
#include "platform/ContentSettingCallbacks.h"
#include "platform/wtf/RefCounted.h"

namespace blink {

class WebContentSettingCallbacksPrivate
    : public RefCounted<WebContentSettingCallbacksPrivate> {
 public:
  static scoped_refptr<WebContentSettingCallbacksPrivate> Create(
      std::unique_ptr<ContentSettingCallbacks> callbacks) {
    return base::AdoptRef(
        new WebContentSettingCallbacksPrivate(std::move(callbacks)));
  }

  ContentSettingCallbacks* Callbacks() { return callbacks_.get(); }

 private:
  WebContentSettingCallbacksPrivate(
      std::unique_ptr<ContentSettingCallbacks> callbacks)
      : callbacks_(std::move(callbacks)) {}
  std::unique_ptr<ContentSettingCallbacks> callbacks_;
};

WebContentSettingCallbacks::WebContentSettingCallbacks(
    std::unique_ptr<ContentSettingCallbacks>&& callbacks) {
  private_ = WebContentSettingCallbacksPrivate::Create(std::move(callbacks));
}

void WebContentSettingCallbacks::Reset() {
  private_.Reset();
}

void WebContentSettingCallbacks::Assign(
    const WebContentSettingCallbacks& other) {
  private_ = other.private_;
}

void WebContentSettingCallbacks::DoAllow() {
  DCHECK(!private_.IsNull());
  private_->Callbacks()->OnAllowed();
  private_.Reset();
}

void WebContentSettingCallbacks::DoDeny() {
  DCHECK(!private_.IsNull());
  private_->Callbacks()->OnDenied();
  private_.Reset();
}

}  // namespace blink
