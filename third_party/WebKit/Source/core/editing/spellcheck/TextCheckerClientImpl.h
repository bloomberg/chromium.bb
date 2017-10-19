// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextCheckerClientImpl_h
#define TextCheckerClientImpl_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/text/TextCheckerClient.h"

namespace blink {

class WebLocalFrameImpl;
class WebTextCheckClient;

// TODO(xiaochengh): Rename TextCheckerClientImpl to SpellCheckerClientImpl.
class CORE_EXPORT TextCheckerClientImpl final
    : public GarbageCollected<TextCheckerClientImpl>,
      public TextCheckerClient {
 public:
  explicit TextCheckerClientImpl(WebLocalFrameImpl*);

  void CheckSpellingOfString(const String&,
                             int* misspelling_location,
                             int* misspelling_length) final;
  void RequestCheckingOfString(TextCheckingRequest*) final;
  void CancelAllPendingRequests() final;

  void Trace(blink::Visitor*);

 private:
  WebTextCheckClient* GetWebTextCheckClient() const;

  Member<WebLocalFrameImpl> web_local_frame_;

  DISALLOW_COPY_AND_ASSIGN(TextCheckerClientImpl);
};

}  // namespace blink

#endif
