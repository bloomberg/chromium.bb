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

  MockFetchContext* Context();

  // Platform:
  WebURLLoaderMockFactory* GetURLLoaderMockFactory() override;
  std::unique_ptr<WebURLLoader> CreateURLLoader(
      const blink::WebURLRequest&,
      base::SingleThreadTaskRunner*) override;

 private:
  class FetchTestingWebURLLoaderMockFactory;

  Persistent<MockFetchContext> context_;
  std::unique_ptr<WebURLLoaderMockFactory> url_loader_mock_factory_;

  DISALLOW_COPY_AND_ASSIGN(FetchTestingPlatformSupport);
};

}  // namespace blink

#endif
