// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_CONTENT_HOLDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_CONTENT_HOLDER_H_

#include "cc/paint/node_holder.h"
#include "cc/paint/text_holder.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using NodeHolder = cc::NodeHolder;

class Node;

class CORE_EXPORT ContentHolder : public cc::TextHolder {
 public:
  ContentHolder(Node& node);
  ~ContentHolder() override;

  bool IsValid() const { return node_; }

  String GetValue() const;
  IntRect GetBoundingBox() const;
  uint64_t GetId() const;

  void SetHasSent() { has_sent_ = true; }
  bool HasSent() const { return has_sent_; }

  const Node* GetNode() { return node_; }

  void OnNodeDetachedFromLayoutTree() { node_ = nullptr; }

 private:
  bool has_sent_ = false;

  UntracedMember<const Node> node_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_CONTENT_HOLDER_H_
