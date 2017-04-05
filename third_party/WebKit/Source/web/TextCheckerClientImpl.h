// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextCheckerClientImpl_h
#define TextCheckerClientImpl_h

#include "platform/heap/Handle.h"
#include "platform/text/TextCheckerClient.h"

namespace blink {

class WebLocalFrameImpl;
class WebTextCheckClient;

// TODO(xiaochengh): Rename TextCheckerClientImpl to SpellCheckerClientImpl.
class TextCheckerClientImpl final
    : public GarbageCollected<TextCheckerClientImpl>,
      public TextCheckerClient {
 public:
  explicit TextCheckerClientImpl(WebLocalFrameImpl*);

  void checkSpellingOfString(const String&,
                             int* misspellingLocation,
                             int* misspellingLength) final;
  void requestCheckingOfString(TextCheckingRequest*) final;
  void cancelAllPendingRequests() final;

  DECLARE_TRACE();

 private:
  WebTextCheckClient* webTextCheckClient() const;

  Member<WebLocalFrameImpl> m_webLocalFrame;

  DISALLOW_COPY_AND_ASSIGN(TextCheckerClientImpl);
};

}  // namespace blink

#endif
