/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/fetch/RawResource.h"

#include "core/fetch/ImageResourceClient.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/MockImageResourceClient.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ResourcePtr.h"
#include "platform/SharedBuffer.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/WebUnitTestSupport.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(RawResourceTest, DontIgnoreAcceptForCacheReuse)
{
    ResourceRequest jpegRequest;
    jpegRequest.setHTTPAccept("image/jpeg");

    ResourcePtr<RawResource> jpegResource(new RawResource(jpegRequest, Resource::Raw));

    ResourceRequest pngRequest;
    pngRequest.setHTTPAccept("image/png");

    ASSERT_FALSE(jpegResource->canReuse(pngRequest));
}

class DummyClient final : public RawResourceClient {
public:
    DummyClient() : m_called(false) {}
    ~DummyClient() override {}

    // ResourceClient implementation.
    void notifyFinished(Resource* resource) override
    {
        m_called = true;
    }
    String debugName() const override { return "DummyClient"; }

    void dataReceived(Resource*, const char* data, size_t length) override
    {
        m_data.append(data, length);
    }

    bool called() { return m_called; }
    const Vector<char>& data() { return m_data; }
private:
    bool m_called;
    Vector<char> m_data;
};

// This client adds another client when notified.
class AddingClient : public RawResourceClient {
public:
    AddingClient(DummyClient* client, Resource* resource)
        : m_dummyClient(client)
        , m_resource(resource)
        , m_removeClientTimer(this, &AddingClient::removeClient) {}

    ~AddingClient() override {}

    // ResourceClient implementation.
    void notifyFinished(Resource* resource) override
    {
        // First schedule an asynchronous task to remove the client.
        // We do not expect the client to be called.
        m_removeClientTimer.startOneShot(0, BLINK_FROM_HERE);
        resource->addClient(m_dummyClient);
    }
    String debugName() const override { return "AddingClient"; }

    void removeClient(Timer<AddingClient>* timer)
    {
        m_resource->removeClient(m_dummyClient);
    }
private:
    DummyClient* m_dummyClient;
    ResourcePtr<Resource> m_resource;
    Timer<AddingClient> m_removeClientTimer;
};

TEST(RawResourceTest, RevalidationSucceeded)
{
    ResourcePtr<Resource> resource = new RawResource(ResourceRequest("data:text/html,"), Resource::Raw);
    ResourceResponse response;
    response.setHTTPStatusCode(200);
    resource->responseReceived(response, nullptr);
    const char data[5] = "abcd";
    resource->appendData(data, 4);
    resource->finish();
    memoryCache()->add(resource.get());

    // Simulate a successful revalidation.
    resource->setRevalidatingRequest(ResourceRequest("data:text/html,"));

    OwnPtr<DummyClient> client = adoptPtr(new DummyClient);
    resource->addClient(client.get());

    ResourceResponse revalidatingResponse;
    revalidatingResponse.setHTTPStatusCode(304);
    resource->responseReceived(revalidatingResponse, nullptr);
    EXPECT_FALSE(resource->isCacheValidator());
    EXPECT_EQ(200, resource->response().httpStatusCode());
    EXPECT_EQ(4u, resource->resourceBuffer()->size());
    EXPECT_EQ(memoryCache()->resourceForURL(KURL(ParsedURLString, "data:text/html,")), resource.get());
    memoryCache()->remove(resource.get());

    resource->removeClient(client.get());
    EXPECT_FALSE(resource->hasClients());
    EXPECT_FALSE(client->called());
    EXPECT_EQ("abcd", String(client->data().data(), client->data().size()));
}

TEST(RawResourceTest, RevalidationSucceededForResourceWithoutBody)
{
    ResourcePtr<Resource> resource = new RawResource(ResourceRequest("data:text/html,"), Resource::Raw);
    ResourceResponse response;
    response.setHTTPStatusCode(200);
    resource->responseReceived(response, nullptr);
    resource->finish();
    memoryCache()->add(resource.get());

    // Simulate a successful revalidation.
    resource->setRevalidatingRequest(ResourceRequest("data:text/html,"));

    OwnPtr<DummyClient> client = adoptPtr(new DummyClient);
    resource->addClient(client.get());

    ResourceResponse revalidatingResponse;
    revalidatingResponse.setHTTPStatusCode(304);
    resource->responseReceived(revalidatingResponse, nullptr);
    EXPECT_FALSE(resource->isCacheValidator());
    EXPECT_EQ(200, resource->response().httpStatusCode());
    EXPECT_EQ(nullptr, resource->resourceBuffer());
    EXPECT_EQ(memoryCache()->resourceForURL(KURL(ParsedURLString, "data:text/html,")), resource.get());
    memoryCache()->remove(resource.get());

    resource->removeClient(client.get());
    EXPECT_FALSE(resource->hasClients());
    EXPECT_FALSE(client->called());
    EXPECT_EQ(0u, client->data().size());
}

TEST(RawResourceTest, AddClientDuringCallback)
{
    ResourcePtr<Resource> raw = new RawResource(ResourceRequest("data:text/html,"), Resource::Raw);
    raw->setLoading(false);

    // Create a non-null response.
    ResourceResponse response = raw->response();
    response.setURL(KURL(ParsedURLString, "http://600.613/"));
    raw->setResponse(response);
    EXPECT_FALSE(raw->response().isNull());

    OwnPtr<DummyClient> dummyClient = adoptPtr(new DummyClient());
    OwnPtr<AddingClient> addingClient = adoptPtr(new AddingClient(dummyClient.get(), raw.get()));
    raw->addClient(addingClient.get());
    testing::runPendingTasks();
    raw->removeClient(addingClient.get());
    EXPECT_FALSE(dummyClient->called());
    EXPECT_FALSE(raw->hasClients());
}

// This client removes another client when notified.
class RemovingClient : public RawResourceClient {
public:
    RemovingClient(DummyClient* client)
        : m_dummyClient(client) {}

    ~RemovingClient() override {}

    // ResourceClient implementation.
    void notifyFinished(Resource* resource) override
    {
        resource->removeClient(m_dummyClient);
        resource->removeClient(this);
    }
    String debugName() const override { return "RemovingClient"; }
private:
    DummyClient* m_dummyClient;
};

TEST(RawResourceTest, RemoveClientDuringCallback)
{
    ResourcePtr<Resource> raw = new RawResource(ResourceRequest("data:text/html,"), Resource::Raw);
    raw->setLoading(false);

    // Create a non-null response.
    ResourceResponse response = raw->response();
    response.setURL(KURL(ParsedURLString, "http://600.613/"));
    raw->setResponse(response);
    EXPECT_FALSE(raw->response().isNull());

    OwnPtr<DummyClient> dummyClient = adoptPtr(new DummyClient());
    OwnPtr<RemovingClient> removingClient = adoptPtr(new RemovingClient(dummyClient.get()));
    raw->addClient(dummyClient.get());
    raw->addClient(removingClient.get());
    testing::runPendingTasks();
    EXPECT_FALSE(raw->hasClients());
}

} // namespace blink
