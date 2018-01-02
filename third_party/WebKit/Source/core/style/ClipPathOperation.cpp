// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/style/ClipPathOperation.h"

namespace blink {

void ReferenceClipPathOperation::AddClient(SVGResourceClient* client,
                                           WebTaskRunner* task_runner) {
  element_proxy_->AddClient(client, task_runner);
}

void ReferenceClipPathOperation::RemoveClient(SVGResourceClient* client) {
  element_proxy_->RemoveClient(client);
}

SVGElement* ReferenceClipPathOperation::FindElement(
    TreeScope& tree_scope) const {
  return element_proxy_->FindElement(tree_scope);
}

bool ReferenceClipPathOperation::operator==(const ClipPathOperation& o) const {
  if (!IsSameType(o))
    return false;
  const ReferenceClipPathOperation& other = ToReferenceClipPathOperation(o);
  return element_proxy_ == other.element_proxy_ && url_ == other.url_;
}

}  // namespace blink
