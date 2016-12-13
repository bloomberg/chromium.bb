// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/resource/MockFontResourceClient.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

MockFontResourceClient::MockFontResourceClient(Resource* resource)
    : m_resource(resource),
      m_fontLoadShortLimitExceededCalled(false),
      m_fontLoadLongLimitExceededCalled(false) {
  m_resource->addClient(this);
}

MockFontResourceClient::~MockFontResourceClient() {}

void MockFontResourceClient::fontLoadShortLimitExceeded(FontResource*) {
  ASSERT_FALSE(m_fontLoadShortLimitExceededCalled);
  ASSERT_FALSE(m_fontLoadLongLimitExceededCalled);
  m_fontLoadShortLimitExceededCalled = true;
}

void MockFontResourceClient::fontLoadLongLimitExceeded(FontResource*) {
  ASSERT_TRUE(m_fontLoadShortLimitExceededCalled);
  ASSERT_FALSE(m_fontLoadLongLimitExceededCalled);
  m_fontLoadLongLimitExceededCalled = true;
}

void MockFontResourceClient::dispose() {
  if (m_resource) {
    m_resource->removeClient(this);
    m_resource = nullptr;
  }
}

}  // namespace blink
