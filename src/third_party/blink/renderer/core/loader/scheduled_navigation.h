// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_SCHEDULED_NAVIGATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_SCHEDULED_NAVIGATION_H_

#include "base/macros.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/renderer/core/loader/frame_loader_types.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class Document;
class LocalFrame;
class UserGestureIndicator;
class UserGestureToken;

class ScheduledNavigation
    : public GarbageCollectedFinalized<ScheduledNavigation> {
 public:
  ScheduledNavigation(ClientNavigationReason,
                      double delay,
                      Document* origin_document,
                      const KURL&,
                      WebFrameLoadType,
                      base::TimeTicks input_timestamp);
  virtual ~ScheduledNavigation();

  virtual void Fire(LocalFrame*) = 0;

  virtual bool ShouldStartTimer(LocalFrame*) { return true; }

  ClientNavigationReason GetReason() const { return reason_; }
  double Delay() const { return delay_; }
  Document* OriginDocument() const { return origin_document_.Get(); }
  const KURL& Url() const { return url_; }
  std::unique_ptr<UserGestureIndicator> CreateUserGestureIndicator();
  base::TimeTicks InputTimestamp() const { return input_timestamp_; }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(origin_document_);
  }

 protected:
  void ClearUserGesture() { user_gesture_token_ = nullptr; }
  WebFrameLoadType LoadType() const { return frame_load_type_; }

 private:
  ClientNavigationReason reason_;
  double delay_;
  Member<Document> origin_document_;
  KURL url_;
  WebFrameLoadType frame_load_type_;
  scoped_refptr<UserGestureToken> user_gesture_token_;
  base::TimeTicks input_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(ScheduledNavigation);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_SCHEDULED_NAVIGATION_H_
