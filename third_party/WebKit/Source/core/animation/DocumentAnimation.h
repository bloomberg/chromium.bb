// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DocumentAnimation_h
#define DocumentAnimation_h

#include "core/animation/Animation.h"
#include "core/animation/DocumentTimeline.h"
#include "core/dom/Document.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class DocumentAnimation {
  STATIC_ONLY(DocumentAnimation);

 public:
  static DocumentTimeline* timeline(Document& document) {
    return &document.Timeline();
  }

  static HeapVector<Member<Animation>> getAnimations(Document& document) {
    return document.Timeline().getAnimations();
  }
};

}  // namespace blink

#endif  // DocumentAnimation_h
