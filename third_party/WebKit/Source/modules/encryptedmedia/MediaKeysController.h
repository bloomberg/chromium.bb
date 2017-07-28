// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaKeysController_h
#define MediaKeysController_h

#include "core/page/Page.h"
#include "modules/ModulesExport.h"

namespace blink {

class ExecutionContext;
class WebEncryptedMediaClient;

class MODULES_EXPORT MediaKeysController final
    : public GarbageCollected<MediaKeysController>,
      public Supplement<Page> {
  USING_GARBAGE_COLLECTED_MIXIN(MediaKeysController);

 public:
  WebEncryptedMediaClient* EncryptedMediaClient(ExecutionContext*);

  static void ProvideMediaKeysTo(Page&);
  static MediaKeysController* From(Page* page) {
    return static_cast<MediaKeysController*>(
        Supplement<Page>::From(page, SupplementName()));
  }

  DEFINE_INLINE_VIRTUAL_TRACE() { Supplement<Page>::Trace(visitor); }

 private:
  MediaKeysController();
  static const char* SupplementName();
};

}  // namespace blink

#endif  // MediaKeysController_h
