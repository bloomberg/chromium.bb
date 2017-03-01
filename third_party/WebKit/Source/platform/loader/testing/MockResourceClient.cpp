// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/loader/testing/MockResourceClient.h"

namespace blink {

MockResourceClient::MockResourceClient(Resource* resource)
    : m_resource(resource),
      m_notifyFinishedCalled(false),
      m_encodedSizeOnNotifyFinished(0) {
  m_resource->addClient(this);
}

MockResourceClient::~MockResourceClient() {}

void MockResourceClient::notifyFinished(Resource* resource) {
  CHECK(!m_notifyFinishedCalled);
  m_notifyFinishedCalled = true;
  m_encodedSizeOnNotifyFinished = resource->encodedSize();
}

void MockResourceClient::removeAsClient() {
  m_resource->removeClient(this);
  m_resource = nullptr;
}

void MockResourceClient::dispose() {
  if (m_resource) {
    m_resource->removeClient(this);
    m_resource = nullptr;
  }
}

DEFINE_TRACE(MockResourceClient) {
  visitor->trace(m_resource);
  ResourceClient::trace(visitor);
}

}  // namespace blink
