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

#include "config.h"
#include "core/fetch/ImageResource.h"

#include "core/fetch/ImageResourceClient.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/MockImageResourceClient.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ResourcePtr.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/EmptyClients.h"
#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "platform/SharedBuffer.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/WebUnitTestSupport.h"

using namespace WebCore;

namespace {

class QuitTask : public WebKit::WebThread::Task {
public:
    virtual void run()
    {
        WebKit::Platform::current()->currentThread()->exitRunLoop();
    }
};

void runPendingTasks()
{
    WebKit::Platform::current()->currentThread()->postTask(new QuitTask);
    WebKit::Platform::current()->currentThread()->enterRunLoop();
}

TEST(ImageResourceTest, MultipartImage)
{
    ResourcePtr<ImageResource> cachedImage = new ImageResource(ResourceRequest());
    cachedImage->setLoading(true);

    MockImageResourceClient client;
    cachedImage->addClient(&client);

    // Send the multipart response. No image or data buffer is created.
    cachedImage->responseReceived(ResourceResponse(KURL(), "multipart/x-mixed-replace", 0, String(), String()));
    ASSERT_FALSE(cachedImage->resourceBuffer());
    ASSERT_FALSE(cachedImage->hasImage());
    ASSERT_EQ(client.imageChangedCount(), 0);
    ASSERT_FALSE(client.notifyFinishedCalled());

    // Send the response for the first real part. No image or data buffer is created.
    const char* svgData = "<svg xmlns='http://www.w3.org/2000/svg' width='1' height='1'><rect width='1' height='1' fill='green'/></svg>";
    unsigned svgDataLength = strlen(svgData);
    cachedImage->responseReceived(ResourceResponse(KURL(), "image/svg+xml", svgDataLength, String(), String()));
    ASSERT_FALSE(cachedImage->resourceBuffer());
    ASSERT_FALSE(cachedImage->hasImage());
    ASSERT_EQ(client.imageChangedCount(), 0);
    ASSERT_FALSE(client.notifyFinishedCalled());

    // The first bytes arrive. The data buffer is created, but no image is created.
    cachedImage->appendData(svgData, svgDataLength);
    ASSERT_TRUE(cachedImage->resourceBuffer());
    ASSERT_EQ(cachedImage->resourceBuffer()->size(), svgDataLength);
    ASSERT_FALSE(cachedImage->hasImage());
    ASSERT_EQ(client.imageChangedCount(), 0);
    ASSERT_FALSE(client.notifyFinishedCalled());

    // This part finishes. The image is created, callbacks are sent, and the data buffer is cleared.
    cachedImage->finish();
    ASSERT_FALSE(cachedImage->resourceBuffer());
    ASSERT_FALSE(cachedImage->errorOccurred());
    ASSERT_TRUE(cachedImage->hasImage());
    ASSERT_FALSE(cachedImage->image()->isNull());
    ASSERT_EQ(cachedImage->image()->width(), 1);
    ASSERT_EQ(cachedImage->image()->height(), 1);
    ASSERT_EQ(client.imageChangedCount(), 2);
    ASSERT_TRUE(client.notifyFinishedCalled());
}

TEST(ImageResourceTest, CancelOnDetach)
{
    KURL testURL(ParsedURLString, "http://www.test.com/cancelTest.html");

    WebKit::WebURLResponse response;
    response.initialize();
    response.setMIMEType("text/html");
    WTF::String localPath = WebKit::Platform::current()->unitTestSupport()->webKitRootDir();
    localPath.append("/Source/web/tests/data/cancelTest.html");
    WebKit::Platform::current()->unitTestSupport()->registerMockedURL(testURL, response, localPath);

    // Create enough of a mocked world to get a functioning ResourceLoader.
    Page::PageClients pageClients;
    fillWithEmptyClients(pageClients);
    EmptyFrameLoaderClient frameLoaderClient;
    Page page(pageClients);
    RefPtr<Frame> frame = Frame::create(&page, 0, &frameLoaderClient);
    frame->setView(FrameView::create(frame.get()));
    frame->init();
    RefPtr<DocumentLoader> documentLoader = DocumentLoader::create(ResourceRequest(testURL), SubstituteData());
    documentLoader->setFrame(frame.get());

    // Emulate starting a real load.
    ResourcePtr<ImageResource> cachedImage = new ImageResource(ResourceRequest(testURL));
    cachedImage->load(documentLoader->fetcher(), ResourceLoaderOptions());
    memoryCache()->add(cachedImage.get());

    MockImageResourceClient client;
    cachedImage->addClient(&client);
    EXPECT_EQ(Resource::Pending, cachedImage->status());

    // The load should still be alive, but a timer should be started to cancel the load inside removeClient().
    cachedImage->removeClient(&client);
    EXPECT_EQ(Resource::Pending, cachedImage->status());
    EXPECT_NE(reinterpret_cast<Resource*>(0), memoryCache()->resourceForURL(testURL));

    // Trigger the cancel timer, ensure the load was cancelled and the resource was evicted from the cache.
    runPendingTasks();
    EXPECT_EQ(Resource::LoadError, cachedImage->status());
    EXPECT_EQ(reinterpret_cast<Resource*>(0), memoryCache()->resourceForURL(testURL));

    WebKit::Platform::current()->unitTestSupport()->unregisterMockedURL(testURL);
}

} // namespace
