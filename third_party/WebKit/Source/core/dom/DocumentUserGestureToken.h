// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DocumentUserGestureToken_h
#define DocumentUserGestureToken_h

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "platform/UserGestureIndicator.h"

namespace blink {

// Associates a UserGestureToken with a Document, if a non-null Document*
// is provided in the constructor.
class DocumentUserGestureToken final : public UserGestureToken {
  WTF_MAKE_NONCOPYABLE(DocumentUserGestureToken);

 public:
  static PassRefPtr<UserGestureToken> Create(
      Document* document,
      Status status = kPossiblyExistingGesture) {
    SetHasReceivedUserGesture(document);
    return AdoptRef(new DocumentUserGestureToken(status));
  }

  static PassRefPtr<UserGestureToken> Adopt(Document* document,
                                            UserGestureToken* token) {
    if (!token || !token->HasGestures())
      return nullptr;
    SetHasReceivedUserGesture(document);
    return token;
  }

 private:
  DocumentUserGestureToken(Status status) : UserGestureToken(status) {}

  static void SetHasReceivedUserGesture(Document* document) {
    if (document && document->GetFrame()) {
      bool had_gesture = document->GetFrame()->HasReceivedUserGesture();
      if (!had_gesture)
        document->GetFrame()->SetDocumentHasReceivedUserGesture();
      document->GetFrame()->Loader().Client()->SetHasReceivedUserGesture(
          had_gesture);
    }
  }
};

}  // namespace blink

#endif  // DocumentUserGestureToken_h
