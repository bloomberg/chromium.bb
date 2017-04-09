// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaKeysController_h
#define MediaKeysController_h

#include "core/page/Page.h"
#include "modules/ModulesExport.h"

namespace blink {

class ExecutionContext;
class MediaKeysClient;
class WebEncryptedMediaClient;

class MODULES_EXPORT MediaKeysController final
    : public GarbageCollected<MediaKeysController>,
      public Supplement<Page> {
  USING_GARBAGE_COLLECTED_MIXIN(MediaKeysController);

 public:
  WebEncryptedMediaClient* EncryptedMediaClient(ExecutionContext*);

  static void ProvideMediaKeysTo(Page&, MediaKeysClient*);
  static MediaKeysController* From(Page* page) {
    return static_cast<MediaKeysController*>(
        Supplement<Page>::From(page, SupplementName()));
  }

  DEFINE_INLINE_VIRTUAL_TRACE() { Supplement<Page>::Trace(visitor); }

 private:
  explicit MediaKeysController(MediaKeysClient*);
  static const char* SupplementName();

  // Raw reference to the client implementation, which is currently owned
  // by the WebView. Its lifetime extends past any m_client accesses.
  // It is not on the Oilpan heap.
  MediaKeysClient* client_;
};

}  // namespace blink

#endif  // MediaKeysController_h
