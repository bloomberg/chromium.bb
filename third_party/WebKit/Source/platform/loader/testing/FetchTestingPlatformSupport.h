// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FetchTestingPlatformSupport_h
#define FetchTestingPlatformSupport_h

#include <memory>
#include "platform/heap/Persistent.h"
#include "platform/testing/TestingPlatformSupport.h"

namespace blink {

class MockFetchContext;

class FetchTestingPlatformSupport
    : public TestingPlatformSupportWithMockScheduler {
 public:
  FetchTestingPlatformSupport();
  ~FetchTestingPlatformSupport() override;

  MockFetchContext* context();

  // Platform:
  WebURLError cancelledError(const WebURL&) const override;
  WebURLLoaderMockFactory* getURLLoaderMockFactory() override;
  WebURLLoader* createURLLoader() override;

 private:
  class FetchTestingWebURLLoaderMockFactory;

  Persistent<MockFetchContext> m_context;
  std::unique_ptr<WebURLLoaderMockFactory> m_urlLoaderMockFactory;

  DISALLOW_COPY_AND_ASSIGN(FetchTestingPlatformSupport);
};

}  // namespace blink

#endif
