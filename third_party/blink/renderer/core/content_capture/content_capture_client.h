// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_CONTENT_CAPTURE_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_CONTENT_CAPTURE_CLIENT_H_

#include <vector>

#include "third_party/blink/renderer/core/content_capture/content_holder.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class ContentHolder;

// The interface ContentCapture client should be implemented.
class CORE_EXPORT ContentCaptureClient
    : public GarbageCollectedFinalized<ContentCaptureClient> {
 public:
  virtual ~ContentCaptureClient() = default;

  // Returns if ContentCapture should be enable.
  virtual bool IsContentCaptureEnabled() = 0;

  // Returns the NodeHolder::Type should be used in ContentCapture.
  virtual NodeHolder::Type GetNodeHolderType() const = 0;

  // Invoked when a list of |content| is captured, |first_content| indicates if
  // this is first captured content.
  virtual void DidCaptureContent(
      const std::vector<scoped_refptr<ContentHolder>>& content,
      bool first_content) = 0;

  // Invoked when the content is removed, |content_ids| is a list of id of
  // removed content.
  virtual void DidRemoveContent(const std::vector<int64_t>& content_ids) = 0;
  virtual void Trace(blink::Visitor* visitor) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_CONTENT_CAPTURE_CLIENT_H_
