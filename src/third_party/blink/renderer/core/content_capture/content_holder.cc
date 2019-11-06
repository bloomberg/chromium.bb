// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/content_capture/content_holder.h"

#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

ContentHolder::ContentHolder(Node& node) : node_(&node) {}

ContentHolder::~ContentHolder() {}

String ContentHolder::GetValue() const {
  DCHECK(IsValid());
  if (node_)
    return node_->nodeValue();
  return String();
}

IntRect ContentHolder::GetBoundingBox() const {
  DCHECK(IsValid());
  if (node_ && node_->GetLayoutObject())
    return EnclosingIntRect(node_->GetLayoutObject()->VisualRectInDocument());
  return IntRect();
}

uint64_t ContentHolder::GetId() const {
  DCHECK(IsValid());
  if (node_)
    return reinterpret_cast<uint64_t>(node_.Get());
  return 0;
}

}  // namespace blink
