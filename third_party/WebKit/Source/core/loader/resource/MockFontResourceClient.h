// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MockFontResourceClient_h
#define MockFontResourceClient_h

#include "core/loader/resource/FontResource.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/loader/fetch/ResourceClient.h"

namespace blink {

class MockFontResourceClient final
    : public GarbageCollectedFinalized<MockFontResourceClient>,
      public FontResourceClient {
  USING_PRE_FINALIZER(MockFontResourceClient, Dispose);
  USING_GARBAGE_COLLECTED_MIXIN(MockFontResourceClient);

 public:
  explicit MockFontResourceClient(Resource*);
  ~MockFontResourceClient() override;

  void FontLoadShortLimitExceeded(FontResource*) override;
  void FontLoadLongLimitExceeded(FontResource*) override;

  bool FontLoadShortLimitExceededCalled() const {
    return font_load_short_limit_exceeded_called_;
  }

  bool FontLoadLongLimitExceededCalled() const {
    return font_load_long_limit_exceeded_called_;
  }

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(resource_);
    FontResourceClient::Trace(visitor);
  }

  String DebugName() const override { return "MockFontResourceClient"; }

 private:
  void Dispose();

  Member<Resource> resource_;
  bool font_load_short_limit_exceeded_called_;
  bool font_load_long_limit_exceeded_called_;
};

}  // namespace blink

#endif  // MockFontResourceClient_h
