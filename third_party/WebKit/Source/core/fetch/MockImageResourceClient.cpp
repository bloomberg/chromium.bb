// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fetch/MockImageResourceClient.h"

#include "core/fetch/ImageResource.h"
#include "core/fetch/ResourcePtr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

MockImageResourceClient::MockImageResourceClient(const ResourcePtr<Resource>& resource)
    : m_resource(resource.get())
    , m_imageChangedCount(0)
    , m_notifyFinishedCalled(false)
{
    m_resource->addClient(this);
}

MockImageResourceClient::~MockImageResourceClient()
{
    if (m_resource)
        m_resource->removeClient(this);
}

void MockImageResourceClient::notifyFinished(Resource*)
{
    ASSERT_FALSE(m_notifyFinishedCalled);
    m_notifyFinishedCalled = true;
}

void MockImageResourceClient::removeAsClient()
{
    m_resource->removeClient(this);
    m_resource = nullptr;
}

} // namespace blink
