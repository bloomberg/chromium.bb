// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/loader/testing/WebURLLoaderFactoryWithMock.h"

#include "public/platform/WebURLLoader.h"
#include "public/platform/WebURLLoaderMockFactory.h"

namespace blink {

WebURLLoaderFactoryWithMock::WebURLLoaderFactoryWithMock(
    WebURLLoaderMockFactory* mock_factory)
    : mock_factory_(mock_factory) {}

WebURLLoaderFactoryWithMock::~WebURLLoaderFactoryWithMock() = default;

std::unique_ptr<WebURLLoader> WebURLLoaderFactoryWithMock::CreateURLLoader(
    const WebURLRequest& request,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return mock_factory_->CreateURLLoader(nullptr);
}

}  // namespace blink
