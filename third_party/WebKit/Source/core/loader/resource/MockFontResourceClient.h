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
  USING_PRE_FINALIZER(MockFontResourceClient, dispose);
  USING_GARBAGE_COLLECTED_MIXIN(MockFontResourceClient);

 public:
  explicit MockFontResourceClient(Resource*);
  ~MockFontResourceClient() override;

  void fontLoadShortLimitExceeded(FontResource*) override;
  void fontLoadLongLimitExceeded(FontResource*) override;

  bool fontLoadShortLimitExceededCalled() const {
    return m_fontLoadShortLimitExceededCalled;
  }

  bool fontLoadLongLimitExceededCalled() const {
    return m_fontLoadLongLimitExceededCalled;
  }

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_resource);
    FontResourceClient::trace(visitor);
  }

  String debugName() const override { return "MockFontResourceClient"; }

 private:
  void dispose();

  Member<Resource> m_resource;
  bool m_fontLoadShortLimitExceededCalled;
  bool m_fontLoadLongLimitExceededCalled;
};

}  // namespace blink

#endif  // MockFontResourceClient_h
