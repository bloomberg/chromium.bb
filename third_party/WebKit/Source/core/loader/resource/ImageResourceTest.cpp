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

#include "core/loader/resource/ImageResource.h"

#include "core/fetch/FetchInitiatorInfo.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/MockResourceClient.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ResourceLoader.h"
#include "core/fetch/UniqueIdentifier.h"
#include "core/loader/resource/MockImageResourceClient.h"
#include "platform/SharedBuffer.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/graphics/BitmapImage.h"
#include "platform/graphics/Image.h"
#include "platform/scheduler/test/fake_web_task_runner.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/platform/WebURLResponse.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/Base64.h"
#include <memory>

namespace blink {

namespace {

// An image of size 1x1.
const unsigned char kJpegImage[] = {
    0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
    0x01, 0x01, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00, 0xff, 0xfe, 0x00, 0x13,
    0x43, 0x72, 0x65, 0x61, 0x74, 0x65, 0x64, 0x20, 0x77, 0x69, 0x74, 0x68,
    0x20, 0x47, 0x49, 0x4d, 0x50, 0xff, 0xdb, 0x00, 0x43, 0x00, 0x05, 0x03,
    0x04, 0x04, 0x04, 0x03, 0x05, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x06,
    0x07, 0x0c, 0x08, 0x07, 0x07, 0x07, 0x07, 0x0f, 0x0b, 0x0b, 0x09, 0x0c,
    0x11, 0x0f, 0x12, 0x12, 0x11, 0x0f, 0x11, 0x11, 0x13, 0x16, 0x1c, 0x17,
    0x13, 0x14, 0x1a, 0x15, 0x11, 0x11, 0x18, 0x21, 0x18, 0x1a, 0x1d, 0x1d,
    0x1f, 0x1f, 0x1f, 0x13, 0x17, 0x22, 0x24, 0x22, 0x1e, 0x24, 0x1c, 0x1e,
    0x1f, 0x1e, 0xff, 0xdb, 0x00, 0x43, 0x01, 0x05, 0x05, 0x05, 0x07, 0x06,
    0x07, 0x0e, 0x08, 0x08, 0x0e, 0x1e, 0x14, 0x11, 0x14, 0x1e, 0x1e, 0x1e,
    0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
    0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
    0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
    0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0xff,
    0xc0, 0x00, 0x11, 0x08, 0x00, 0x01, 0x00, 0x01, 0x03, 0x01, 0x22, 0x00,
    0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xff, 0xc4, 0x00, 0x15, 0x00, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x08, 0xff, 0xc4, 0x00, 0x14, 0x10, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xff, 0xc4, 0x00, 0x14, 0x01, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xff, 0xc4, 0x00, 0x14, 0x11, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
    0xda, 0x00, 0x0c, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3f,
    0x00, 0xb2, 0xc0, 0x07, 0xff, 0xd9};

const size_t kJpegImageSubrangeWithDimensionsLength = sizeof(kJpegImage) - 1;

// Ensure that the image decoder can determine the dimensions of kJpegImage from
// just the first kJpegImageSubrangeWithDimensionsLength bytes. If this test
// fails, then the test data here probably needs to be updated.
TEST(ImageResourceTest, DimensionsDecodableFromPartialTestImage) {
  RefPtr<Image> image = BitmapImage::create();
  EXPECT_EQ(
      Image::SizeAvailable,
      image->setData(SharedBuffer::create(
                         kJpegImage, kJpegImageSubrangeWithDimensionsLength),
                     true));
  EXPECT_TRUE(image->isBitmapImage());
  EXPECT_EQ(1, image->width());
  EXPECT_EQ(1, image->height());
}

// An image of size 50x50.
const unsigned char kJpegImage2[] = {
    0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
    0x01, 0x01, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43,
    0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xdb, 0x00, 0x43, 0x01, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xc0, 0x00, 0x11, 0x08, 0x00, 0x32, 0x00, 0x32, 0x03,
    0x01, 0x22, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xff, 0xc4, 0x00,
    0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xc4, 0x00, 0x14, 0x10,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xc4, 0x00, 0x15, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x02, 0xff, 0xc4, 0x00, 0x14, 0x11, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xff, 0xda, 0x00, 0x0c, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03,
    0x11, 0x00, 0x3f, 0x00, 0x00, 0x94, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x03, 0xff, 0xd9};

const char kSvgImage[] =
    "<svg width=\"200\" height=\"200\" xmlns=\"http://www.w3.org/2000/svg\" "
    "xmlns:xlink=\"http://www.w3.org/1999/xlink\">"
    "<rect x=\"0\" y=\"0\" width=\"100px\" height=\"100px\" fill=\"red\"/>"
    "</svg>";

const char kSvgImage2[] =
    "<svg width=\"300\" height=\"300\" xmlns=\"http://www.w3.org/2000/svg\" "
    "xmlns:xlink=\"http://www.w3.org/1999/xlink\">"
    "<rect x=\"0\" y=\"0\" width=\"200px\" height=\"200px\" fill=\"green\"/>"
    "</svg>";

void receiveResponse(ImageResource* imageResource,
                     const KURL& url,
                     const AtomicString& mimeType,
                     const char* data,
                     size_t dataSize) {
  ResourceResponse response;
  response.setURL(url);
  response.setHTTPStatusCode(200);
  response.setMimeType(mimeType);
  imageResource->responseReceived(response, nullptr);
  imageResource->appendData(data, dataSize);
  imageResource->finish();
}

class ImageResourceTestMockFetchContext : public FetchContext {
 public:
  static ImageResourceTestMockFetchContext* create() {
    return new ImageResourceTestMockFetchContext;
  }

  virtual ~ImageResourceTestMockFetchContext() {}

  bool allowImage(bool imagesEnabled, const KURL&) const override {
    return true;
  }
  ResourceRequestBlockedReason canRequest(
      Resource::Type,
      const ResourceRequest&,
      const KURL&,
      const ResourceLoaderOptions&,
      bool forPreload,
      FetchRequest::OriginRestriction) const override {
    return ResourceRequestBlockedReason::None;
  }
  bool shouldLoadNewResource(Resource::Type) const override { return true; }
  WebTaskRunner* loadingTaskRunner() const override { return m_runner.get(); }

 private:
  ImageResourceTestMockFetchContext()
      : m_runner(WTF::wrapUnique(new scheduler::FakeWebTaskRunner)) {}

  std::unique_ptr<scheduler::FakeWebTaskRunner> m_runner;
};

// Convenience class that registers a mocked URL load on construction, and
// unregisters it on destruction. This allows for a test to use constructs like
// ASSERT_TRUE() without needing to worry about unregistering the mocked URL
// load to avoid putting other tests into inconsistent states in case the
// assertion fails.
class ScopedRegisteredURL {
 public:
  ScopedRegisteredURL(const KURL& url,
                      const String& fileName = "cancelTest.html",
                      const String& mimeType = "text/html")
      : m_url(url) {
    URLTestHelpers::registerMockedURLLoad(m_url, fileName, mimeType);
  }

  ~ScopedRegisteredURL() {
    Platform::current()->getURLLoaderMockFactory()->unregisterURL(m_url);
  }

 private:
  KURL m_url;
};

AtomicString buildContentRange(size_t rangeLength, size_t totalLength) {
  return AtomicString(String("bytes 0-" + String::number(rangeLength) + "/" +
                             String::number(totalLength)));
}

TEST(ImageResourceTest, MultipartImage) {
  ResourceFetcher* fetcher =
      ResourceFetcher::create(ImageResourceTestMockFetchContext::create());
  KURL testURL(ParsedURLString, "http://www.test.com/cancelTest.html");
  ScopedRegisteredURL scopedRegisteredURL(testURL);

  // Emulate starting a real load, but don't expect any "real"
  // WebURLLoaderClient callbacks.
  ImageResource* imageResource =
      ImageResource::create(ResourceRequest(testURL));
  imageResource->setIdentifier(createUniqueIdentifier());
  fetcher->startLoad(imageResource);

  Persistent<MockImageResourceClient> client =
      new MockImageResourceClient(imageResource);
  EXPECT_EQ(Resource::Pending, imageResource->getStatus());

  // Send the multipart response. No image or data buffer is created. Note that
  // the response must be routed through ResourceLoader to ensure the load is
  // flagged as multipart.
  ResourceResponse multipartResponse(KURL(), "multipart/x-mixed-replace", 0,
                                     nullAtom, String());
  multipartResponse.setMultipartBoundary("boundary", strlen("boundary"));
  imageResource->loader()->didReceiveResponse(
      WrappedResourceResponse(multipartResponse), nullptr);
  EXPECT_FALSE(imageResource->resourceBuffer());
  EXPECT_FALSE(imageResource->getContent()->hasImage());
  EXPECT_EQ(0, client->imageChangedCount());
  EXPECT_FALSE(client->notifyFinishedCalled());
  EXPECT_EQ("multipart/x-mixed-replace", imageResource->response().mimeType());

  const char firstPart[] =
      "--boundary\n"
      "Content-Type: image/svg+xml\n\n";
  imageResource->appendData(firstPart, strlen(firstPart));
  // Send the response for the first real part. No image or data buffer is
  // created.
  EXPECT_FALSE(imageResource->resourceBuffer());
  EXPECT_FALSE(imageResource->getContent()->hasImage());
  EXPECT_EQ(0, client->imageChangedCount());
  EXPECT_FALSE(client->notifyFinishedCalled());
  EXPECT_EQ("image/svg+xml", imageResource->response().mimeType());

  const char secondPart[] =
      "<svg xmlns='http://www.w3.org/2000/svg' width='1' height='1'><rect "
      "width='1' height='1' fill='green'/></svg>\n";
  // The first bytes arrive. The data buffer is created, but no image is
  // created.
  imageResource->appendData(secondPart, strlen(secondPart));
  EXPECT_TRUE(imageResource->resourceBuffer());
  EXPECT_FALSE(imageResource->getContent()->hasImage());
  EXPECT_EQ(0, client->imageChangedCount());
  EXPECT_FALSE(client->notifyFinishedCalled());

  // Add a client to check an assertion error doesn't happen
  // (crbug.com/630983).
  Persistent<MockImageResourceClient> client2 =
      new MockImageResourceClient(imageResource);
  EXPECT_EQ(0, client2->imageChangedCount());
  EXPECT_FALSE(client2->notifyFinishedCalled());

  const char thirdPart[] = "--boundary";
  imageResource->appendData(thirdPart, strlen(thirdPart));
  ASSERT_TRUE(imageResource->resourceBuffer());
  EXPECT_EQ(strlen(secondPart) - 1, imageResource->resourceBuffer()->size());

  // This part finishes. The image is created, callbacks are sent, and the data
  // buffer is cleared.
  imageResource->loader()->didFinishLoading(0.0, 0, 0);
  EXPECT_TRUE(imageResource->resourceBuffer());
  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->height());
  EXPECT_EQ(1, client->imageChangedCount());
  EXPECT_TRUE(client->notifyFinishedCalled());
  EXPECT_EQ(1, client2->imageChangedCount());
  EXPECT_TRUE(client2->notifyFinishedCalled());
}

TEST(ImageResourceTest, CancelOnDetach) {
  KURL testURL(ParsedURLString, "http://www.test.com/cancelTest.html");
  ScopedRegisteredURL scopedRegisteredURL(testURL);

  ResourceFetcher* fetcher =
      ResourceFetcher::create(ImageResourceTestMockFetchContext::create());

  // Emulate starting a real load.
  ImageResource* imageResource =
      ImageResource::create(ResourceRequest(testURL));
  imageResource->setIdentifier(createUniqueIdentifier());

  fetcher->startLoad(imageResource);
  memoryCache()->add(imageResource);

  Persistent<MockImageResourceClient> client =
      new MockImageResourceClient(imageResource);
  EXPECT_EQ(Resource::Pending, imageResource->getStatus());

  // The load should still be alive, but a timer should be started to cancel the
  // load inside removeClient().
  client->removeAsClient();
  EXPECT_EQ(Resource::Pending, imageResource->getStatus());
  EXPECT_TRUE(memoryCache()->resourceForURL(testURL));

  // Trigger the cancel timer, ensure the load was cancelled and the resource
  // was evicted from the cache.
  blink::testing::runPendingTasks();
  EXPECT_EQ(Resource::LoadError, imageResource->getStatus());
  EXPECT_FALSE(memoryCache()->resourceForURL(testURL));
}

TEST(ImageResourceTest, DecodedDataRemainsWhileHasClients) {
  ImageResource* imageResource = ImageResource::create(ResourceRequest());
  imageResource->setStatus(Resource::Pending);

  Persistent<MockImageResourceClient> client =
      new MockImageResourceClient(imageResource);

  // Send the image response.
  imageResource->responseReceived(
      ResourceResponse(KURL(), "multipart/x-mixed-replace", 0, nullAtom,
                       String()),
      nullptr);

  imageResource->responseReceived(
      ResourceResponse(KURL(), "image/jpeg", sizeof(kJpegImage), nullAtom,
                       String()),
      nullptr);
  imageResource->appendData(reinterpret_cast<const char*>(kJpegImage),
                            sizeof(kJpegImage));
  EXPECT_NE(0u, imageResource->encodedSizeMemoryUsageForTesting());
  imageResource->finish();
  EXPECT_EQ(0u, imageResource->encodedSizeMemoryUsageForTesting());
  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_TRUE(client->notifyFinishedCalled());

  // The prune comes when the ImageResource still has clients. The image should
  // not be deleted.
  imageResource->prune();
  EXPECT_TRUE(imageResource->isAlive());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());

  // The ImageResource no longer has clients. The decoded image data should be
  // deleted by prune.
  client->removeAsClient();
  imageResource->prune();
  EXPECT_FALSE(imageResource->isAlive());
  EXPECT_TRUE(imageResource->getContent()->hasImage());
  // TODO(hajimehoshi): Should check imageResource doesn't have decoded image
  // data.
}

TEST(ImageResourceTest, UpdateBitmapImages) {
  ImageResource* imageResource = ImageResource::create(ResourceRequest());
  imageResource->setStatus(Resource::Pending);

  Persistent<MockImageResourceClient> client =
      new MockImageResourceClient(imageResource);

  // Send the image response.
  imageResource->responseReceived(
      ResourceResponse(KURL(), "image/jpeg", sizeof(kJpegImage), nullAtom,
                       String()),
      nullptr);
  imageResource->appendData(reinterpret_cast<const char*>(kJpegImage),
                            sizeof(kJpegImage));
  imageResource->finish();
  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(2, client->imageChangedCount());
  EXPECT_TRUE(client->notifyFinishedCalled());
  EXPECT_TRUE(imageResource->getContent()->getImage()->isBitmapImage());
}

TEST(ImageResourceTest, ReloadIfLoFiOrPlaceholderAfterFinished) {
  KURL testURL(ParsedURLString, "http://www.test.com/cancelTest.html");
  ScopedRegisteredURL scopedRegisteredURL(testURL);
  ResourceRequest request = ResourceRequest(testURL);
  request.setLoFiState(WebURLRequest::LoFiOn);
  ImageResource* imageResource = ImageResource::create(request);
  imageResource->setStatus(Resource::Pending);

  Persistent<MockImageResourceClient> client =
      new MockImageResourceClient(imageResource);
  ResourceFetcher* fetcher =
      ResourceFetcher::create(ImageResourceTestMockFetchContext::create());

  // Send the image response.
  ResourceResponse resourceResponse(KURL(), "image/jpeg", sizeof(kJpegImage),
                                    nullAtom, String());
  resourceResponse.addHTTPHeaderField("chrome-proxy-content-transform",
                                      "empty-image");

  imageResource->responseReceived(resourceResponse, nullptr);
  imageResource->appendData(reinterpret_cast<const char*>(kJpegImage),
                            sizeof(kJpegImage));
  imageResource->finish();
  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(2, client->imageChangedCount());
  EXPECT_EQ(1, client->imageNotifyFinishedCount());
  EXPECT_EQ(sizeof(kJpegImage), client->encodedSizeOnLastImageChanged());
  // The client should have been notified that the image load completed.
  EXPECT_TRUE(client->notifyFinishedCalled());
  EXPECT_EQ(sizeof(kJpegImage), client->encodedSizeOnNotifyFinished());
  EXPECT_EQ(sizeof(kJpegImage), client->encodedSizeOnImageNotifyFinished());
  EXPECT_TRUE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->height());

  // Call reloadIfLoFiOrPlaceholderImage() after the image has finished loading.
  imageResource->reloadIfLoFiOrPlaceholderImage(fetcher,
                                                Resource::kReloadAlways);
  EXPECT_FALSE(imageResource->errorOccurred());
  EXPECT_FALSE(imageResource->resourceBuffer());
  EXPECT_FALSE(imageResource->getContent()->hasImage());
  EXPECT_EQ(3, client->imageChangedCount());
  EXPECT_EQ(1, client->imageNotifyFinishedCount());

  imageResource->loader()->didReceiveResponse(
      WrappedResourceResponse(resourceResponse), nullptr);
  imageResource->loader()->didReceiveData(
      reinterpret_cast<const char*>(kJpegImage2), sizeof(kJpegImage2));
  imageResource->loader()->didFinishLoading(0.0, sizeof(kJpegImage2),
                                            sizeof(kJpegImage2));
  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(sizeof(kJpegImage2), client->encodedSizeOnLastImageChanged());
  EXPECT_TRUE(client->notifyFinishedCalled());

  // The client should not have been notified of completion again.
  EXPECT_EQ(sizeof(kJpegImage), client->encodedSizeOnNotifyFinished());
  EXPECT_EQ(sizeof(kJpegImage), client->encodedSizeOnImageNotifyFinished());

  EXPECT_TRUE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_EQ(50, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(50, imageResource->getContent()->getImage()->height());
}

TEST(ImageResourceTest, ReloadIfLoFiOrPlaceholderDuringFetch) {
  KURL testURL(ParsedURLString, "http://www.test.com/cancelTest.html");
  ScopedRegisteredURL scopedRegisteredURL(testURL);

  ResourceRequest request(testURL);
  request.setLoFiState(WebURLRequest::LoFiOn);
  FetchRequest fetchRequest(request, FetchInitiatorInfo());
  ResourceFetcher* fetcher =
      ResourceFetcher::create(ImageResourceTestMockFetchContext::create());

  ImageResource* imageResource = ImageResource::fetch(fetchRequest, fetcher);
  Persistent<MockImageResourceClient> client =
      new MockImageResourceClient(imageResource);

  // Send the image response.
  ResourceResponse initialResourceResponse(
      testURL, "image/jpeg", sizeof(kJpegImage), nullAtom, String());
  initialResourceResponse.addHTTPHeaderField("chrome-proxy", "q=low");

  imageResource->loader()->didReceiveResponse(
      WrappedResourceResponse(initialResourceResponse));
  imageResource->loader()->didReceiveData(
      reinterpret_cast<const char*>(kJpegImage), sizeof(kJpegImage));

  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(1, client->imageChangedCount());
  EXPECT_EQ(sizeof(kJpegImage), client->encodedSizeOnLastImageChanged());
  EXPECT_FALSE(client->notifyFinishedCalled());
  EXPECT_TRUE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->height());

  // Call reloadIfLoFiOrPlaceholderImage() while the image is still loading.
  imageResource->reloadIfLoFiOrPlaceholderImage(fetcher,
                                                Resource::kReloadAlways);
  EXPECT_FALSE(imageResource->errorOccurred());
  EXPECT_FALSE(imageResource->resourceBuffer());
  EXPECT_FALSE(imageResource->getContent()->hasImage());
  EXPECT_EQ(2, client->imageChangedCount());
  EXPECT_EQ(0U, client->encodedSizeOnLastImageChanged());
  // The client should not have been notified of completion yet, since the image
  // is still loading.
  EXPECT_FALSE(client->notifyFinishedCalled());

  imageResource->loader()->didReceiveResponse(
      WrappedResourceResponse(ResourceResponse(
          testURL, "image/jpeg", sizeof(kJpegImage2), nullAtom, String())),
      nullptr);
  imageResource->loader()->didReceiveData(
      reinterpret_cast<const char*>(kJpegImage2), sizeof(kJpegImage2));
  imageResource->loader()->didFinishLoading(0.0, sizeof(kJpegImage2),
                                            sizeof(kJpegImage2));

  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(sizeof(kJpegImage2), client->encodedSizeOnLastImageChanged());
  // The client should have been notified of completion only after the reload
  // completed.
  EXPECT_TRUE(client->notifyFinishedCalled());
  EXPECT_EQ(sizeof(kJpegImage2), client->encodedSizeOnNotifyFinished());
  EXPECT_EQ(sizeof(kJpegImage2), client->encodedSizeOnImageNotifyFinished());
  EXPECT_TRUE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_EQ(50, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(50, imageResource->getContent()->getImage()->height());
}

TEST(ImageResourceTest, ReloadIfLoFiOrPlaceholderForPlaceholder) {
  KURL testURL(ParsedURLString, "http://www.test.com/cancelTest.html");
  ScopedRegisteredURL scopedRegisteredURL(testURL);

  ResourceFetcher* fetcher =
      ResourceFetcher::create(ImageResourceTestMockFetchContext::create());
  FetchRequest request(testURL, FetchInitiatorInfo());
  request.setAllowImagePlaceholder();
  ImageResource* imageResource = ImageResource::fetch(request, fetcher);
  EXPECT_EQ(FetchRequest::AllowPlaceholder,
            request.placeholderImageRequestType());
  EXPECT_EQ("bytes=0-2047",
            imageResource->resourceRequest().httpHeaderField("range"));
  Persistent<MockImageResourceClient> client =
      new MockImageResourceClient(imageResource);

  ResourceResponse response(testURL, "image/jpeg",
                            kJpegImageSubrangeWithDimensionsLength, nullAtom,
                            String());
  response.setHTTPStatusCode(206);
  response.setHTTPHeaderField(
      "content-range", buildContentRange(kJpegImageSubrangeWithDimensionsLength,
                                         sizeof(kJpegImage)));
  imageResource->loader()->didReceiveResponse(
      WrappedResourceResponse(response));
  imageResource->loader()->didReceiveData(
      reinterpret_cast<const char*>(kJpegImage),
      kJpegImageSubrangeWithDimensionsLength);
  imageResource->loader()->didFinishLoading(
      0.0, kJpegImageSubrangeWithDimensionsLength,
      kJpegImageSubrangeWithDimensionsLength);

  EXPECT_EQ(Resource::Cached, imageResource->getStatus());
  EXPECT_TRUE(imageResource->isPlaceholder());

  imageResource->reloadIfLoFiOrPlaceholderImage(fetcher,
                                                Resource::kReloadAlways);

  EXPECT_EQ(Resource::Pending, imageResource->getStatus());
  EXPECT_FALSE(imageResource->isPlaceholder());
  EXPECT_EQ(nullAtom,
            imageResource->resourceRequest().httpHeaderField("range"));
  EXPECT_EQ(
      static_cast<int>(WebCachePolicy::BypassingCache),
      static_cast<int>(imageResource->resourceRequest().getCachePolicy()));

  imageResource->loader()->cancel();
}

TEST(ImageResourceTest, SVGImage) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo");
  ImageResource* imageResource = ImageResource::create(ResourceRequest(url));
  Persistent<MockImageResourceClient> client =
      new MockImageResourceClient(imageResource);

  receiveResponse(imageResource, url, "image/svg+xml", kSvgImage,
                  strlen(kSvgImage));

  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(1, client->imageChangedCount());
  EXPECT_TRUE(client->notifyFinishedCalled());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isBitmapImage());
}

TEST(ImageResourceTest, SuccessfulRevalidationJpeg) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo");
  ImageResource* imageResource = ImageResource::create(ResourceRequest(url));
  Persistent<MockImageResourceClient> client =
      new MockImageResourceClient(imageResource);

  receiveResponse(imageResource, url, "image/jpeg",
                  reinterpret_cast<const char*>(kJpegImage),
                  sizeof(kJpegImage));

  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(2, client->imageChangedCount());
  EXPECT_EQ(1, client->imageNotifyFinishedCount());
  EXPECT_TRUE(client->notifyFinishedCalled());
  EXPECT_TRUE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->height());

  imageResource->setRevalidatingRequest(ResourceRequest(url));
  ResourceResponse response;
  response.setURL(url);
  response.setHTTPStatusCode(304);

  imageResource->responseReceived(response, nullptr);

  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(2, client->imageChangedCount());
  EXPECT_EQ(1, client->imageNotifyFinishedCount());
  EXPECT_TRUE(client->notifyFinishedCalled());
  EXPECT_TRUE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->height());
}

TEST(ImageResourceTest, SuccessfulRevalidationSvg) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo");
  ImageResource* imageResource = ImageResource::create(ResourceRequest(url));
  Persistent<MockImageResourceClient> client =
      new MockImageResourceClient(imageResource);

  receiveResponse(imageResource, url, "image/svg+xml", kSvgImage,
                  strlen(kSvgImage));

  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(1, client->imageChangedCount());
  EXPECT_EQ(1, client->imageNotifyFinishedCount());
  EXPECT_TRUE(client->notifyFinishedCalled());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_EQ(200, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(200, imageResource->getContent()->getImage()->height());

  imageResource->setRevalidatingRequest(ResourceRequest(url));
  ResourceResponse response;
  response.setURL(url);
  response.setHTTPStatusCode(304);
  imageResource->responseReceived(response, nullptr);

  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(1, client->imageChangedCount());
  EXPECT_EQ(1, client->imageNotifyFinishedCount());
  EXPECT_TRUE(client->notifyFinishedCalled());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_EQ(200, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(200, imageResource->getContent()->getImage()->height());
}

TEST(ImageResourceTest, FailedRevalidationJpegToJpeg) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo");
  ImageResource* imageResource = ImageResource::create(ResourceRequest(url));
  Persistent<MockImageResourceClient> client =
      new MockImageResourceClient(imageResource);

  receiveResponse(imageResource, url, "image/jpeg",
                  reinterpret_cast<const char*>(kJpegImage),
                  sizeof(kJpegImage));

  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(2, client->imageChangedCount());
  EXPECT_EQ(1, client->imageNotifyFinishedCount());
  EXPECT_TRUE(client->notifyFinishedCalled());
  EXPECT_TRUE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->height());

  imageResource->setRevalidatingRequest(ResourceRequest(url));
  receiveResponse(imageResource, url, "image/jpeg",
                  reinterpret_cast<const char*>(kJpegImage2),
                  sizeof(kJpegImage2));

  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(4, client->imageChangedCount());
  EXPECT_EQ(1, client->imageNotifyFinishedCount());
  EXPECT_TRUE(client->notifyFinishedCalled());
  EXPECT_TRUE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_EQ(50, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(50, imageResource->getContent()->getImage()->height());
}

TEST(ImageResourceTest, FailedRevalidationJpegToSvg) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo");
  ImageResource* imageResource = ImageResource::create(ResourceRequest(url));
  Persistent<MockImageResourceClient> client =
      new MockImageResourceClient(imageResource);

  receiveResponse(imageResource, url, "image/jpeg",
                  reinterpret_cast<const char*>(kJpegImage),
                  sizeof(kJpegImage));

  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(2, client->imageChangedCount());
  EXPECT_EQ(1, client->imageNotifyFinishedCount());
  EXPECT_TRUE(client->notifyFinishedCalled());
  EXPECT_TRUE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->height());

  imageResource->setRevalidatingRequest(ResourceRequest(url));
  receiveResponse(imageResource, url, "image/svg+xml", kSvgImage,
                  strlen(kSvgImage));

  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(3, client->imageChangedCount());
  EXPECT_EQ(1, client->imageNotifyFinishedCount());
  EXPECT_TRUE(client->notifyFinishedCalled());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_EQ(200, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(200, imageResource->getContent()->getImage()->height());
}

TEST(ImageResourceTest, FailedRevalidationSvgToJpeg) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo");
  ImageResource* imageResource = ImageResource::create(ResourceRequest(url));
  Persistent<MockImageResourceClient> client =
      new MockImageResourceClient(imageResource);

  receiveResponse(imageResource, url, "image/svg+xml", kSvgImage,
                  strlen(kSvgImage));

  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(1, client->imageChangedCount());
  EXPECT_EQ(1, client->imageNotifyFinishedCount());
  EXPECT_TRUE(client->notifyFinishedCalled());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_EQ(200, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(200, imageResource->getContent()->getImage()->height());

  imageResource->setRevalidatingRequest(ResourceRequest(url));
  receiveResponse(imageResource, url, "image/jpeg",
                  reinterpret_cast<const char*>(kJpegImage),
                  sizeof(kJpegImage));

  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(3, client->imageChangedCount());
  EXPECT_EQ(1, client->imageNotifyFinishedCount());
  EXPECT_TRUE(client->notifyFinishedCalled());
  EXPECT_TRUE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->height());
}

TEST(ImageResourceTest, FailedRevalidationSvgToSvg) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo");
  ImageResource* imageResource = ImageResource::create(ResourceRequest(url));
  Persistent<MockImageResourceClient> client =
      new MockImageResourceClient(imageResource);

  receiveResponse(imageResource, url, "image/svg+xml", kSvgImage,
                  strlen(kSvgImage));

  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(1, client->imageChangedCount());
  EXPECT_EQ(1, client->imageNotifyFinishedCount());
  EXPECT_TRUE(client->notifyFinishedCalled());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_EQ(200, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(200, imageResource->getContent()->getImage()->height());

  imageResource->setRevalidatingRequest(ResourceRequest(url));
  receiveResponse(imageResource, url, "image/svg+xml", kSvgImage2,
                  strlen(kSvgImage2));

  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(2, client->imageChangedCount());
  EXPECT_EQ(1, client->imageNotifyFinishedCount());
  EXPECT_TRUE(client->notifyFinishedCalled());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_EQ(300, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(300, imageResource->getContent()->getImage()->height());
}

// Tests for pruning.

TEST(ImageResourceTest, AddClientAfterPrune) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo");
  ImageResource* imageResource = ImageResource::create(ResourceRequest(url));

  // Adds a ResourceClient but not ImageResourceObserver.
  Persistent<MockResourceClient> client1 =
      new MockResourceClient(imageResource);

  receiveResponse(imageResource, url, "image/jpeg",
                  reinterpret_cast<const char*>(kJpegImage),
                  sizeof(kJpegImage));

  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->height());
  EXPECT_TRUE(client1->notifyFinishedCalled());

  client1->removeAsClient();

  EXPECT_FALSE(imageResource->isAlive());

  imageResource->prune();

  EXPECT_TRUE(imageResource->getContent()->hasImage());

  // Re-adds a ResourceClient but not ImageResourceObserver.
  Persistent<MockResourceClient> client2 =
      new MockResourceClient(imageResource);

  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->height());
  EXPECT_TRUE(client2->notifyFinishedCalled());
}

TEST(ImageResourceTest, CancelOnDecodeError) {
  KURL testURL(ParsedURLString, "http://www.test.com/cancelTest.html");
  ScopedRegisteredURL scopedRegisteredURL(testURL);

  ResourceFetcher* fetcher =
      ResourceFetcher::create(ImageResourceTestMockFetchContext::create());
  FetchRequest request(testURL, FetchInitiatorInfo());
  ImageResource* imageResource = ImageResource::fetch(request, fetcher);

  imageResource->loader()->didReceiveResponse(
      WrappedResourceResponse(
          ResourceResponse(testURL, "image/jpeg", 18, nullAtom, String())),
      nullptr);
  imageResource->loader()->didReceiveData("notactuallyanimage", 18);
  EXPECT_EQ(Resource::DecodeError, imageResource->getStatus());
  EXPECT_FALSE(imageResource->isLoading());
}

TEST(ImageResourceTest, FetchDisallowPlaceholder) {
  KURL testURL(ParsedURLString, "http://www.test.com/cancelTest.html");
  ScopedRegisteredURL scopedRegisteredURL(testURL);

  FetchRequest request(testURL, FetchInitiatorInfo());
  ImageResource* imageResource = ImageResource::fetch(
      request,
      ResourceFetcher::create(ImageResourceTestMockFetchContext::create()));
  EXPECT_EQ(FetchRequest::DisallowPlaceholder,
            request.placeholderImageRequestType());
  EXPECT_EQ(nullAtom,
            imageResource->resourceRequest().httpHeaderField("range"));
  EXPECT_FALSE(imageResource->isPlaceholder());
  Persistent<MockImageResourceClient> client =
      new MockImageResourceClient(imageResource);

  imageResource->loader()->didReceiveResponse(
      WrappedResourceResponse(ResourceResponse(
          testURL, "image/jpeg", sizeof(kJpegImage), nullAtom, String())));
  imageResource->loader()->didReceiveData(
      reinterpret_cast<const char*>(kJpegImage), sizeof(kJpegImage));
  imageResource->loader()->didFinishLoading(0.0, sizeof(kJpegImage),
                                            sizeof(kJpegImage));

  EXPECT_EQ(Resource::Cached, imageResource->getStatus());
  EXPECT_EQ(sizeof(kJpegImage), imageResource->encodedSize());
  EXPECT_FALSE(imageResource->isPlaceholder());
  EXPECT_LT(0, client->imageChangedCount());
  EXPECT_EQ(sizeof(kJpegImage), client->encodedSizeOnLastImageChanged());
  EXPECT_TRUE(client->notifyFinishedCalled());
  EXPECT_EQ(sizeof(kJpegImage), client->encodedSizeOnNotifyFinished());
  EXPECT_EQ(sizeof(kJpegImage), client->encodedSizeOnImageNotifyFinished());

  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->height());
  EXPECT_TRUE(imageResource->getContent()->getImage()->isBitmapImage());
}

TEST(ImageResourceTest, FetchAllowPlaceholderDataURL) {
  KURL testURL(ParsedURLString,
               "data:image/jpeg;base64," +
                   base64Encode(reinterpret_cast<const char*>(kJpegImage),
                                sizeof(kJpegImage)));
  FetchRequest request(testURL, FetchInitiatorInfo());
  request.setAllowImagePlaceholder();
  ImageResource* imageResource = ImageResource::fetch(
      request,
      ResourceFetcher::create(ImageResourceTestMockFetchContext::create()));
  EXPECT_EQ(FetchRequest::DisallowPlaceholder,
            request.placeholderImageRequestType());
  EXPECT_EQ(nullAtom,
            imageResource->resourceRequest().httpHeaderField("range"));
  EXPECT_FALSE(imageResource->isPlaceholder());
}

TEST(ImageResourceTest, FetchAllowPlaceholderPostRequest) {
  KURL testURL(ParsedURLString, "http://www.test.com/cancelTest.html");
  ScopedRegisteredURL scopedRegisteredURL(testURL);
  ResourceRequest resourceRequest(testURL);
  resourceRequest.setHTTPMethod("POST");
  FetchRequest request(resourceRequest, FetchInitiatorInfo());
  request.setAllowImagePlaceholder();
  ImageResource* imageResource = ImageResource::fetch(
      request,
      ResourceFetcher::create(ImageResourceTestMockFetchContext::create()));
  EXPECT_EQ(FetchRequest::DisallowPlaceholder,
            request.placeholderImageRequestType());
  EXPECT_EQ(nullAtom,
            imageResource->resourceRequest().httpHeaderField("range"));
  EXPECT_FALSE(imageResource->isPlaceholder());

  imageResource->loader()->cancel();
}

TEST(ImageResourceTest, FetchAllowPlaceholderExistingRangeHeader) {
  KURL testURL(ParsedURLString, "http://www.test.com/cancelTest.html");
  ScopedRegisteredURL scopedRegisteredURL(testURL);
  ResourceRequest resourceRequest(testURL);
  resourceRequest.setHTTPHeaderField("range", "bytes=128-255");
  FetchRequest request(resourceRequest, FetchInitiatorInfo());
  request.setAllowImagePlaceholder();
  ImageResource* imageResource = ImageResource::fetch(
      request,
      ResourceFetcher::create(ImageResourceTestMockFetchContext::create()));
  EXPECT_EQ(FetchRequest::DisallowPlaceholder,
            request.placeholderImageRequestType());
  EXPECT_EQ("bytes=128-255",
            imageResource->resourceRequest().httpHeaderField("range"));
  EXPECT_FALSE(imageResource->isPlaceholder());

  imageResource->loader()->cancel();
}

TEST(ImageResourceTest, FetchAllowPlaceholderSuccessful) {
  KURL testURL(ParsedURLString, "http://www.test.com/cancelTest.html");
  ScopedRegisteredURL scopedRegisteredURL(testURL);

  FetchRequest request(testURL, FetchInitiatorInfo());
  request.setAllowImagePlaceholder();
  ImageResource* imageResource = ImageResource::fetch(
      request,
      ResourceFetcher::create(ImageResourceTestMockFetchContext::create()));
  EXPECT_EQ(FetchRequest::AllowPlaceholder,
            request.placeholderImageRequestType());
  EXPECT_EQ("bytes=0-2047",
            imageResource->resourceRequest().httpHeaderField("range"));
  EXPECT_TRUE(imageResource->isPlaceholder());
  Persistent<MockImageResourceClient> client =
      new MockImageResourceClient(imageResource);

  ResourceResponse response(testURL, "image/jpeg",
                            kJpegImageSubrangeWithDimensionsLength, nullAtom,
                            String());
  response.setHTTPStatusCode(206);
  response.setHTTPHeaderField(
      "content-range", buildContentRange(kJpegImageSubrangeWithDimensionsLength,
                                         sizeof(kJpegImage)));
  imageResource->loader()->didReceiveResponse(
      WrappedResourceResponse(response));
  imageResource->loader()->didReceiveData(
      reinterpret_cast<const char*>(kJpegImage),
      kJpegImageSubrangeWithDimensionsLength);
  imageResource->loader()->didFinishLoading(
      0.0, kJpegImageSubrangeWithDimensionsLength,
      kJpegImageSubrangeWithDimensionsLength);

  EXPECT_EQ(Resource::Cached, imageResource->getStatus());
  EXPECT_EQ(kJpegImageSubrangeWithDimensionsLength,
            imageResource->encodedSize());
  EXPECT_TRUE(imageResource->isPlaceholder());
  EXPECT_LT(0, client->imageChangedCount());
  EXPECT_EQ(kJpegImageSubrangeWithDimensionsLength,
            client->encodedSizeOnLastImageChanged());
  EXPECT_TRUE(client->notifyFinishedCalled());
  EXPECT_EQ(kJpegImageSubrangeWithDimensionsLength,
            client->encodedSizeOnNotifyFinished());
  EXPECT_EQ(kJpegImageSubrangeWithDimensionsLength,
            client->encodedSizeOnImageNotifyFinished());

  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->height());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isSVGImage());
}

TEST(ImageResourceTest, FetchAllowPlaceholderUnsuccessful) {
  KURL testURL(ParsedURLString, "http://www.test.com/cancelTest.html");
  ScopedRegisteredURL scopedRegisteredURL(testURL);

  FetchRequest request(testURL, FetchInitiatorInfo());
  request.setAllowImagePlaceholder();
  ImageResource* imageResource = ImageResource::fetch(
      request,
      ResourceFetcher::create(ImageResourceTestMockFetchContext::create()));
  EXPECT_EQ(FetchRequest::AllowPlaceholder,
            request.placeholderImageRequestType());
  EXPECT_EQ("bytes=0-2047",
            imageResource->resourceRequest().httpHeaderField("range"));
  EXPECT_TRUE(imageResource->isPlaceholder());
  Persistent<MockImageResourceClient> client =
      new MockImageResourceClient(imageResource);

  const char kBadData[] = "notanimageresponse";

  imageResource->loader()->didReceiveResponse(
      WrappedResourceResponse(ResourceResponse(
          testURL, "image/jpeg", sizeof(kBadData), nullAtom, String())));
  imageResource->loader()->didReceiveData(kBadData, sizeof(kBadData));

  // The dimensions could not be extracted, so the full original image should be
  // loading.
  EXPECT_EQ(Resource::Pending, imageResource->getStatus());
  EXPECT_FALSE(imageResource->isPlaceholder());
  EXPECT_EQ(nullAtom,
            imageResource->resourceRequest().httpHeaderField("range"));
  EXPECT_EQ(
      static_cast<int>(WebCachePolicy::BypassingCache),
      static_cast<int>(imageResource->resourceRequest().getCachePolicy()));
  EXPECT_FALSE(client->notifyFinishedCalled());
  EXPECT_EQ(0, client->imageNotifyFinishedCount());

  imageResource->loader()->didReceiveResponse(
      WrappedResourceResponse(ResourceResponse(
          testURL, "image/jpeg", sizeof(kJpegImage), nullAtom, String())));
  imageResource->loader()->didReceiveData(
      reinterpret_cast<const char*>(kJpegImage), sizeof(kJpegImage));
  imageResource->loader()->didFinishLoading(0.0, sizeof(kJpegImage),
                                            sizeof(kJpegImage));

  EXPECT_EQ(Resource::Cached, imageResource->getStatus());
  EXPECT_EQ(sizeof(kJpegImage), imageResource->encodedSize());
  EXPECT_FALSE(imageResource->isPlaceholder());
  EXPECT_LT(0, client->imageChangedCount());
  EXPECT_EQ(sizeof(kJpegImage), client->encodedSizeOnLastImageChanged());
  EXPECT_TRUE(client->notifyFinishedCalled());
  EXPECT_EQ(1, client->imageNotifyFinishedCount());
  EXPECT_EQ(sizeof(kJpegImage), client->encodedSizeOnNotifyFinished());
  EXPECT_EQ(sizeof(kJpegImage), client->encodedSizeOnImageNotifyFinished());

  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->height());
  EXPECT_TRUE(imageResource->getContent()->getImage()->isBitmapImage());
}

TEST(ImageResourceTest, FetchAllowPlaceholderThenDisallowPlaceholder) {
  KURL testURL(ParsedURLString, "http://www.test.com/cancelTest.html");
  ScopedRegisteredURL scopedRegisteredURL(testURL);

  ResourceFetcher* fetcher =
      ResourceFetcher::create(ImageResourceTestMockFetchContext::create());
  FetchRequest placeholderRequest(testURL, FetchInitiatorInfo());
  placeholderRequest.setAllowImagePlaceholder();
  ImageResource* imageResource =
      ImageResource::fetch(placeholderRequest, fetcher);
  Persistent<MockImageResourceClient> client =
      new MockImageResourceClient(imageResource);

  FetchRequest nonPlaceholderRequest(testURL, FetchInitiatorInfo());
  ImageResource* secondImageResource =
      ImageResource::fetch(nonPlaceholderRequest, fetcher);
  EXPECT_EQ(imageResource, secondImageResource);
  EXPECT_EQ(Resource::Pending, imageResource->getStatus());
  EXPECT_FALSE(imageResource->isPlaceholder());
  EXPECT_EQ(nullAtom,
            imageResource->resourceRequest().httpHeaderField("range"));
  EXPECT_EQ(
      static_cast<int>(WebCachePolicy::UseProtocolCachePolicy),
      static_cast<int>(imageResource->resourceRequest().getCachePolicy()));
  EXPECT_FALSE(client->notifyFinishedCalled());

  imageResource->loader()->cancel();
}

TEST(ImageResourceTest,
     FetchAllowPlaceholderThenDisallowPlaceholderAfterLoaded) {
  KURL testURL(ParsedURLString, "http://www.test.com/cancelTest.html");
  ScopedRegisteredURL scopedRegisteredURL(testURL);

  ResourceFetcher* fetcher =
      ResourceFetcher::create(ImageResourceTestMockFetchContext::create());
  FetchRequest placeholderRequest(testURL, FetchInitiatorInfo());
  placeholderRequest.setAllowImagePlaceholder();
  ImageResource* imageResource =
      ImageResource::fetch(placeholderRequest, fetcher);
  Persistent<MockImageResourceClient> client =
      new MockImageResourceClient(imageResource);

  ResourceResponse response(testURL, "image/jpeg",
                            kJpegImageSubrangeWithDimensionsLength, nullAtom,
                            String());
  response.setHTTPStatusCode(206);
  response.setHTTPHeaderField(
      "content-range", buildContentRange(kJpegImageSubrangeWithDimensionsLength,
                                         sizeof(kJpegImage)));
  imageResource->loader()->didReceiveResponse(
      WrappedResourceResponse(response));
  imageResource->loader()->didReceiveData(
      reinterpret_cast<const char*>(kJpegImage),
      kJpegImageSubrangeWithDimensionsLength);
  imageResource->loader()->didFinishLoading(
      0.0, kJpegImageSubrangeWithDimensionsLength,
      kJpegImageSubrangeWithDimensionsLength);

  EXPECT_EQ(Resource::Cached, imageResource->getStatus());
  EXPECT_EQ(kJpegImageSubrangeWithDimensionsLength,
            imageResource->encodedSize());
  EXPECT_TRUE(imageResource->isPlaceholder());
  EXPECT_LT(0, client->imageChangedCount());
  EXPECT_TRUE(client->notifyFinishedCalled());

  FetchRequest nonPlaceholderRequest(testURL, FetchInitiatorInfo());
  ImageResource* secondImageResource =
      ImageResource::fetch(nonPlaceholderRequest, fetcher);
  EXPECT_EQ(imageResource, secondImageResource);
  EXPECT_EQ(Resource::Pending, imageResource->getStatus());
  EXPECT_FALSE(imageResource->isPlaceholder());
  EXPECT_EQ(nullAtom,
            imageResource->resourceRequest().httpHeaderField("range"));
  EXPECT_EQ(
      static_cast<int>(WebCachePolicy::UseProtocolCachePolicy),
      static_cast<int>(imageResource->resourceRequest().getCachePolicy()));

  imageResource->loader()->cancel();
}

TEST(ImageResourceTest, PeriodicFlushTest) {
  TestingPlatformSupportWithMockScheduler platform;
  KURL testURL(ParsedURLString, "http://www.test.com/cancelTest.html");
  URLTestHelpers::registerMockedURLLoad(testURL, "cancelTest.html",
                                        "text/html");
  ResourceRequest request = ResourceRequest(testURL);
  ImageResource* imageResource = ImageResource::create(request);
  imageResource->setStatus(Resource::Pending);

  Persistent<MockImageResourceClient> client =
      new MockImageResourceClient(imageResource);

  // Send the image response.
  ResourceResponse resourceResponse(KURL(), "image/jpeg", sizeof(kJpegImage2),
                                    nullAtom, String());
  resourceResponse.addHTTPHeaderField("chrome-proxy", "q=low");

  imageResource->responseReceived(resourceResponse, nullptr);

  // This is number is sufficiently large amount of bytes necessary for the
  // image to be created (since the size is known). This was determined by
  // appending one byte at a time (with flushes) until the image was decoded.
  size_t meaningfulImageSize = 280;
  imageResource->appendData(reinterpret_cast<const char*>(kJpegImage2),
                            meaningfulImageSize);
  size_t bytesSent = meaningfulImageSize;

  EXPECT_FALSE(imageResource->errorOccurred());
  EXPECT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_EQ(1, client->imageChangedCount());

  platform.runForPeriodSeconds(1.);
  platform.advanceClockSeconds(1.);

  // Sanity check that we created an image after appending |meaningfulImageSize|
  // bytes just once.
  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_EQ(1, client->imageChangedCount());

  for (int flushCount = 1; flushCount <= 3; ++flushCount) {
    // For each of the iteration that appends data, we don't expect
    // |imageChangeCount()| to change, since the time is adjusted by 0.2001
    // seconds (it's greater than 0.2 to avoid double precision problems).
    // After 5 appends, we breach the flush interval and the flush count
    // increases.
    for (int i = 0; i < 5; ++i) {
      SCOPED_TRACE(i);
      imageResource->appendData(
          reinterpret_cast<const char*>(kJpegImage2) + bytesSent, 1);

      EXPECT_FALSE(imageResource->errorOccurred());
      ASSERT_TRUE(imageResource->getContent()->hasImage());
      EXPECT_EQ(flushCount, client->imageChangedCount());

      ++bytesSent;
      platform.runForPeriodSeconds(0.2001);
    }
  }

  // Increasing time by a large number only causes one extra flush.
  platform.runForPeriodSeconds(10.);
  platform.advanceClockSeconds(10.);
  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(4, client->imageChangedCount());

  // Append the rest of the data and finish (which causes another flush).
  imageResource->appendData(
      reinterpret_cast<const char*>(kJpegImage2) + bytesSent,
      sizeof(kJpegImage2) - bytesSent);
  imageResource->finish();

  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(5, client->imageChangedCount());
  EXPECT_TRUE(client->notifyFinishedCalled());
  EXPECT_TRUE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_EQ(50, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(50, imageResource->getContent()->getImage()->height());

  WTF::setTimeFunctionsForTesting(nullptr);
}

}  // namespace
}  // namespace blink
