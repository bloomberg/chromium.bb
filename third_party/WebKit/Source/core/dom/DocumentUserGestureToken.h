// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DocumentUserGestureToken_h
#define DocumentUserGestureToken_h

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "platform/UserGestureIndicator.h"

namespace blink {

// Associates a UserGestureToken with a Document, if a non-null Document*
// is provided in the constructor.
class DocumentUserGestureToken final : public UserGestureToken {
  WTF_MAKE_NONCOPYABLE(DocumentUserGestureToken);

 public:
  static PassRefPtr<UserGestureToken> create(
      Document* document,
      Status status = PossiblyExistingGesture) {
    return adoptRef(new DocumentUserGestureToken(document, status));
  }

  ~DocumentUserGestureToken() final {}

 private:
  DocumentUserGestureToken(Document* document, Status status)
      : UserGestureToken(status) {
    if (!document || document->hasReceivedUserGesture())
      return;
    document->setHasReceivedUserGesture();
    for (Frame* frame = document->frame()->tree().parent(); frame;
         frame = frame->tree().parent()) {
      frame->setDocumentHasReceivedUserGesture();
    }
  }
};

}  // namespace blink

#endif  // DocumentUserGestureToken_h
