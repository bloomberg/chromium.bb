// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_content_holder.h"

#include "third_party/blink/renderer/core/content_capture/content_holder.h"

namespace blink {

WebContentHolder::~WebContentHolder() = default;

WebString WebContentHolder::GetValue() const {
  return private_->GetValue();
}

WebRect WebContentHolder::GetBoundingBox() const {
  return private_->GetBoundingBox();
}

uint64_t WebContentHolder::GetId() const {
  return private_->GetId();
}

WebContentHolder::WebContentHolder(scoped_refptr<ContentHolder> content_holder)
    : private_(std::move(content_holder)) {}

}  // namespace blink
