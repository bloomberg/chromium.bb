// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fetch/MockResourceClients.h"

#include "core/fetch/ImageResource.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

MockResourceClient::MockResourceClient(PassRefPtrWillBeRawPtr<Resource> resource)
    : m_resource(resource.get())
    , m_notifyFinishedCalled(false)
{
    m_resource->addClient(this);
}

MockResourceClient::~MockResourceClient()
{
    if (m_resource)
        m_resource->removeClient(this);
}
void MockResourceClient::notifyFinished(Resource*)
{
    ASSERT_FALSE(m_notifyFinishedCalled);
    m_notifyFinishedCalled = true;
}

void MockResourceClient::removeAsClient()
{
    m_resource->removeClient(this);
    m_resource = nullptr;
}

MockImageResourceClient::MockImageResourceClient(PassRefPtrWillBeRawPtr<ImageResource> resource)
    : MockResourceClient(resource)
    , m_imageChangedCount(0)
{
    toImageResource(m_resource.get())->addObserver(this);
}

MockImageResourceClient::~MockImageResourceClient()
{
    if (m_resource)
        toImageResource(m_resource.get())->removeObserver(this);
}

void MockImageResourceClient::removeAsClient()
{
    toImageResource(m_resource.get())->removeObserver(this);
    MockResourceClient::removeAsClient();
}

void MockImageResourceClient::imageChanged(ImageResource*, const IntRect*)
{
    m_imageChangedCount++;
}

} // namespace blink
