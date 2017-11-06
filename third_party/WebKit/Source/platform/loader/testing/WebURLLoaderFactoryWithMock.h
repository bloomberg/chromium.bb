// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebURLLoaderFactoryWithMock_h
#define WebURLLoaderFactoryWithMock_h

#include <memory>

#include "public/platform/WebURLLoaderFactory.h"

namespace blink {

class WebURLLoaderMockFactory;

class WebURLLoaderFactoryWithMock : public WebURLLoaderFactory {
 public:
  explicit WebURLLoaderFactoryWithMock(WebURLLoaderMockFactory*);
  ~WebURLLoaderFactoryWithMock() override;

  std::unique_ptr<WebURLLoader> CreateURLLoader(
      const WebURLRequest&,
      scoped_refptr<base::SingleThreadTaskRunner>) override;

 private:
  // Not owned. The mock factory should outlive |this|.
  WebURLLoaderMockFactory* mock_factory_;
};

}  // namespace blink

#endif  // WebURLLoaderFactoryWithMock_h
