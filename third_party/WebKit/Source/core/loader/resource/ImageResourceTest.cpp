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

#include <memory>
#include "core/loader/resource/MockImageResourceObserver.h"
#include "platform/SharedBuffer.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/graphics/BitmapImage.h"
#include "platform/graphics/Image.h"
#include "platform/loader/fetch/FetchInitiatorInfo.h"
#include "platform/loader/fetch/FetchRequest.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoader.h"
#include "platform/loader/fetch/UniqueIdentifier.h"
#include "platform/loader/testing/MockFetchContext.h"
#include "platform/loader/testing/MockResourceClient.h"
#include "platform/scheduler/test/fake_web_task_runner.h"
#include "platform/testing/ScopedMockedURL.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/platform/WebURLResponse.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/Base64.h"

namespace blink {

using testing::ScopedMockedURLLoad;

namespace {

// An image of size 1x1.
constexpr unsigned char kJpegImage[] = {
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

constexpr int kJpegImageWidth = 1;

constexpr size_t kJpegImageSubrangeWithDimensionsLength =
    sizeof(kJpegImage) - 1;
constexpr size_t kJpegImageSubrangeWithoutDimensionsLength = 3;

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
constexpr unsigned char kJpegImage2[] = {
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

constexpr char kSvgImage[] =
    "<svg width=\"200\" height=\"200\" xmlns=\"http://www.w3.org/2000/svg\" "
    "xmlns:xlink=\"http://www.w3.org/1999/xlink\">"
    "<rect x=\"0\" y=\"0\" width=\"100px\" height=\"100px\" fill=\"red\"/>"
    "</svg>";

constexpr char kSvgImage2[] =
    "<svg width=\"300\" height=\"300\" xmlns=\"http://www.w3.org/2000/svg\" "
    "xmlns:xlink=\"http://www.w3.org/1999/xlink\">"
    "<rect x=\"0\" y=\"0\" width=\"200px\" height=\"200px\" fill=\"green\"/>"
    "</svg>";

constexpr char kTestURL[] = "http://www.test.com/cancelTest.html";

String GetTestFilePath() {
  return testing::webTestDataPath("cancelTest.html");
}

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

void testThatReloadIsStartedThenServeReload(const KURL& testURL,
                                            ImageResource* imageResource,
                                            ImageResourceContent* content,
                                            MockImageResourceObserver* observer,
                                            WebCachePolicy policyForReload) {
  const char* data = reinterpret_cast<const char*>(kJpegImage2);
  constexpr size_t dataLength = sizeof(kJpegImage2);
  constexpr int imageWidth = 50;
  constexpr int imageHeight = 50;

  // Checks that |imageResource| and |content| are ready for non-placeholder
  // reloading.
  EXPECT_EQ(ResourceStatus::Pending, imageResource->getStatus());
  EXPECT_FALSE(imageResource->resourceBuffer());
  EXPECT_FALSE(imageResource->isPlaceholder());
  EXPECT_EQ(nullAtom,
            imageResource->resourceRequest().httpHeaderField("range"));
  EXPECT_EQ(policyForReload, imageResource->resourceRequest().getCachePolicy());
  EXPECT_EQ(content, imageResource->getContent());
  EXPECT_FALSE(content->hasImage());

  // Checks |observer| before reloading.
  const int originalImageChangedCount = observer->imageChangedCount();
  const bool alreadyNotifiedFinish = observer->imageNotifyFinishedCalled();
  const int imageWidthOnImageNotifyFinished =
      observer->imageWidthOnImageNotifyFinished();
  ASSERT_NE(imageWidth, imageWidthOnImageNotifyFinished);

  // Does Reload.
  imageResource->loader()->didReceiveResponse(WrappedResourceResponse(
      ResourceResponse(testURL, "image/jpeg", dataLength, nullAtom)));
  imageResource->loader()->didReceiveData(data, dataLength);
  imageResource->loader()->didFinishLoading(0.0, dataLength, dataLength);

  // Checks |imageResource|'s status after reloading.
  EXPECT_EQ(ResourceStatus::Cached, imageResource->getStatus());
  EXPECT_FALSE(imageResource->errorOccurred());
  EXPECT_EQ(dataLength, imageResource->encodedSize());

  // Checks |observer| after reloading that it is notified of updates/finish.
  EXPECT_LT(originalImageChangedCount, observer->imageChangedCount());
  EXPECT_EQ(imageWidth, observer->imageWidthOnLastImageChanged());
  EXPECT_TRUE(observer->imageNotifyFinishedCalled());
  if (!alreadyNotifiedFinish) {
    // If imageNotifyFinished() has not been called before the reloaded
    // response is served, then imageNotifyFinished() should be called with
    // the new image (of width |imageWidth|).
    EXPECT_EQ(imageWidth, observer->imageWidthOnImageNotifyFinished());
  }

  // Checks |content| receives the correct image.
  EXPECT_TRUE(content->hasImage());
  EXPECT_FALSE(content->getImage()->isNull());
  EXPECT_EQ(imageWidth, content->getImage()->width());
  EXPECT_EQ(imageHeight, content->getImage()->height());
  EXPECT_TRUE(content->getImage()->isBitmapImage());
}

AtomicString buildContentRange(size_t rangeLength, size_t totalLength) {
  return AtomicString(String("bytes 0-" + String::number(rangeLength - 1) +
                             "/" + String::number(totalLength)));
}

ResourceFetcher* createFetcher() {
  return ResourceFetcher::create(
      MockFetchContext::create(MockFetchContext::kShouldLoadNewResource));
}

TEST(ImageResourceTest, MultipartImage) {
  ResourceFetcher* fetcher = createFetcher();
  KURL testURL(ParsedURLString, kTestURL);
  ScopedMockedURLLoad scopedMockedURLLoad(testURL, GetTestFilePath());

  // Emulate starting a real load, but don't expect any "real"
  // WebURLLoaderClient callbacks.
  ImageResource* imageResource =
      ImageResource::create(ResourceRequest(testURL));
  imageResource->setIdentifier(createUniqueIdentifier());
  fetcher->startLoad(imageResource);

  std::unique_ptr<MockImageResourceObserver> observer =
      MockImageResourceObserver::create(imageResource->getContent());
  EXPECT_EQ(ResourceStatus::Pending, imageResource->getStatus());

  // Send the multipart response. No image or data buffer is created. Note that
  // the response must be routed through ResourceLoader to ensure the load is
  // flagged as multipart.
  ResourceResponse multipartResponse(KURL(), "multipart/x-mixed-replace", 0,
                                     nullAtom);
  multipartResponse.setMultipartBoundary("boundary", strlen("boundary"));
  imageResource->loader()->didReceiveResponse(
      WrappedResourceResponse(multipartResponse), nullptr);
  EXPECT_FALSE(imageResource->resourceBuffer());
  EXPECT_FALSE(imageResource->getContent()->hasImage());
  EXPECT_EQ(0, observer->imageChangedCount());
  EXPECT_FALSE(observer->imageNotifyFinishedCalled());
  EXPECT_EQ("multipart/x-mixed-replace", imageResource->response().mimeType());

  const char firstPart[] =
      "--boundary\n"
      "Content-Type: image/svg+xml\n\n";
  imageResource->appendData(firstPart, strlen(firstPart));
  // Send the response for the first real part. No image or data buffer is
  // created.
  EXPECT_FALSE(imageResource->resourceBuffer());
  EXPECT_FALSE(imageResource->getContent()->hasImage());
  EXPECT_EQ(0, observer->imageChangedCount());
  EXPECT_FALSE(observer->imageNotifyFinishedCalled());
  EXPECT_EQ("image/svg+xml", imageResource->response().mimeType());

  const char secondPart[] =
      "<svg xmlns='http://www.w3.org/2000/svg' width='1' height='1'><rect "
      "width='1' height='1' fill='green'/></svg>\n";
  // The first bytes arrive. The data buffer is created, but no image is
  // created.
  imageResource->appendData(secondPart, strlen(secondPart));
  EXPECT_TRUE(imageResource->resourceBuffer());
  EXPECT_FALSE(imageResource->getContent()->hasImage());
  EXPECT_EQ(0, observer->imageChangedCount());
  EXPECT_FALSE(observer->imageNotifyFinishedCalled());

  // Add an observer to check an assertion error doesn't happen
  // (crbug.com/630983).
  std::unique_ptr<MockImageResourceObserver> observer2 =
      MockImageResourceObserver::create(imageResource->getContent());
  EXPECT_EQ(0, observer2->imageChangedCount());
  EXPECT_FALSE(observer2->imageNotifyFinishedCalled());

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
  EXPECT_EQ(1, observer->imageChangedCount());
  EXPECT_TRUE(observer->imageNotifyFinishedCalled());
  EXPECT_EQ(1, observer2->imageChangedCount());
  EXPECT_TRUE(observer2->imageNotifyFinishedCalled());
}

TEST(ImageResourceTest, CancelOnRemoveObserver) {
  KURL testURL(ParsedURLString, kTestURL);
  ScopedMockedURLLoad scopedMockedURLLoad(testURL, GetTestFilePath());

  ResourceFetcher* fetcher = createFetcher();

  // Emulate starting a real load.
  ImageResource* imageResource =
      ImageResource::create(ResourceRequest(testURL));
  imageResource->setIdentifier(createUniqueIdentifier());

  fetcher->startLoad(imageResource);
  memoryCache()->add(imageResource);

  std::unique_ptr<MockImageResourceObserver> observer =
      MockImageResourceObserver::create(imageResource->getContent());
  EXPECT_EQ(ResourceStatus::Pending, imageResource->getStatus());

  // The load should still be alive, but a timer should be started to cancel the
  // load inside removeClient().
  observer->removeAsObserver();
  EXPECT_EQ(ResourceStatus::Pending, imageResource->getStatus());
  EXPECT_TRUE(memoryCache()->resourceForURL(testURL));

  // Trigger the cancel timer, ensure the load was cancelled and the resource
  // was evicted from the cache.
  blink::testing::runPendingTasks();
  EXPECT_EQ(ResourceStatus::LoadError, imageResource->getStatus());
  EXPECT_FALSE(memoryCache()->resourceForURL(testURL));
}

TEST(ImageResourceTest, DecodedDataRemainsWhileHasClients) {
  ImageResource* imageResource = ImageResource::create(ResourceRequest());
  imageResource->setStatus(ResourceStatus::Pending);

  std::unique_ptr<MockImageResourceObserver> observer =
      MockImageResourceObserver::create(imageResource->getContent());

  // Send the image response.
  imageResource->responseReceived(
      ResourceResponse(KURL(), "multipart/x-mixed-replace", 0, nullAtom),
      nullptr);

  imageResource->responseReceived(
      ResourceResponse(KURL(), "image/jpeg", sizeof(kJpegImage), nullAtom),
      nullptr);
  imageResource->appendData(reinterpret_cast<const char*>(kJpegImage),
                            sizeof(kJpegImage));
  EXPECT_NE(0u, imageResource->encodedSizeMemoryUsageForTesting());
  imageResource->finish();
  EXPECT_EQ(0u, imageResource->encodedSizeMemoryUsageForTesting());
  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_TRUE(observer->imageNotifyFinishedCalled());

  // The prune comes when the ImageResource still has observers. The image
  // should not be deleted.
  imageResource->prune();
  EXPECT_TRUE(imageResource->isAlive());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());

  // The ImageResource no longer has observers. The decoded image data should be
  // deleted by prune.
  observer->removeAsObserver();
  imageResource->prune();
  EXPECT_FALSE(imageResource->isAlive());
  EXPECT_TRUE(imageResource->getContent()->hasImage());
  // TODO(hajimehoshi): Should check imageResource doesn't have decoded image
  // data.
}

TEST(ImageResourceTest, UpdateBitmapImages) {
  ImageResource* imageResource = ImageResource::create(ResourceRequest());
  imageResource->setStatus(ResourceStatus::Pending);

  std::unique_ptr<MockImageResourceObserver> observer =
      MockImageResourceObserver::create(imageResource->getContent());

  // Send the image response.
  imageResource->responseReceived(
      ResourceResponse(KURL(), "image/jpeg", sizeof(kJpegImage), nullAtom),
      nullptr);
  imageResource->appendData(reinterpret_cast<const char*>(kJpegImage),
                            sizeof(kJpegImage));
  imageResource->finish();
  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(2, observer->imageChangedCount());
  EXPECT_TRUE(observer->imageNotifyFinishedCalled());
  EXPECT_TRUE(imageResource->getContent()->getImage()->isBitmapImage());
}

TEST(ImageResourceTest, ReloadIfLoFiOrPlaceholderAfterFinished) {
  KURL testURL(ParsedURLString, kTestURL);
  ScopedMockedURLLoad scopedMockedURLLoad(testURL, GetTestFilePath());
  ResourceRequest request = ResourceRequest(testURL);
  request.setPreviewsState(WebURLRequest::ServerLoFiOn);
  ImageResource* imageResource = ImageResource::create(request);
  imageResource->setStatus(ResourceStatus::Pending);

  std::unique_ptr<MockImageResourceObserver> observer =
      MockImageResourceObserver::create(imageResource->getContent());
  ResourceFetcher* fetcher = createFetcher();

  // Send the image response.
  ResourceResponse resourceResponse(KURL(), "image/jpeg", sizeof(kJpegImage),
                                    nullAtom);
  resourceResponse.addHTTPHeaderField("chrome-proxy-content-transform",
                                      "empty-image");

  imageResource->responseReceived(resourceResponse, nullptr);
  imageResource->appendData(reinterpret_cast<const char*>(kJpegImage),
                            sizeof(kJpegImage));
  imageResource->finish();
  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(2, observer->imageChangedCount());
  EXPECT_EQ(kJpegImageWidth, observer->imageWidthOnLastImageChanged());
  // The observer should have been notified that the image load completed.
  EXPECT_TRUE(observer->imageNotifyFinishedCalled());
  EXPECT_EQ(kJpegImageWidth, observer->imageWidthOnImageNotifyFinished());
  EXPECT_TRUE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->height());

  // Call reloadIfLoFiOrPlaceholderImage() after the image has finished loading.
  imageResource->reloadIfLoFiOrPlaceholderImage(fetcher,
                                                Resource::kReloadAlways);

  EXPECT_EQ(3, observer->imageChangedCount());
  testThatReloadIsStartedThenServeReload(
      testURL, imageResource, imageResource->getContent(), observer.get(),
      WebCachePolicy::BypassingCache);
}

TEST(ImageResourceTest, ReloadIfLoFiOrPlaceholderViaResourceFetcher) {
  ResourceFetcher* fetcher = createFetcher();

  KURL testURL(ParsedURLString, kTestURL);
  ScopedMockedURLLoad scopedMockedURLLoad(testURL, GetTestFilePath());

  ResourceRequest request = ResourceRequest(testURL);
  request.setPreviewsState(WebURLRequest::ServerLoFiOn);
  FetchRequest fetchRequest(request, FetchInitiatorInfo());
  ImageResource* imageResource = ImageResource::fetch(fetchRequest, fetcher);
  ImageResourceContent* content = imageResource->getContent();

  std::unique_ptr<MockImageResourceObserver> observer =
      MockImageResourceObserver::create(content);

  // Send the image response.
  ResourceResponse resourceResponse(KURL(), "image/jpeg", sizeof(kJpegImage),
                                    nullAtom);
  resourceResponse.addHTTPHeaderField("chrome-proxy-content-transform",
                                      "empty-image");

  imageResource->loader()->didReceiveResponse(
      WrappedResourceResponse(resourceResponse));
  imageResource->loader()->didReceiveData(
      reinterpret_cast<const char*>(kJpegImage), sizeof(kJpegImage));
  imageResource->loader()->didFinishLoading(0.0, sizeof(kJpegImage),
                                            sizeof(kJpegImage));

  EXPECT_TRUE(observer->imageNotifyFinishedCalled());
  EXPECT_EQ(imageResource, fetcher->cachedResource(testURL));

  fetcher->reloadLoFiImages();

  EXPECT_EQ(3, observer->imageChangedCount());

  testThatReloadIsStartedThenServeReload(testURL, imageResource, content,
                                         observer.get(),
                                         WebCachePolicy::BypassingCache);

  memoryCache()->remove(imageResource);
}

TEST(ImageResourceTest, ReloadIfLoFiOrPlaceholderDuringFetch) {
  KURL testURL(ParsedURLString, kTestURL);
  ScopedMockedURLLoad scopedMockedURLLoad(testURL, GetTestFilePath());

  ResourceRequest request(testURL);
  request.setPreviewsState(WebURLRequest::ServerLoFiOn);
  FetchRequest fetchRequest(request, FetchInitiatorInfo());
  ResourceFetcher* fetcher = createFetcher();

  ImageResource* imageResource = ImageResource::fetch(fetchRequest, fetcher);
  std::unique_ptr<MockImageResourceObserver> observer =
      MockImageResourceObserver::create(imageResource->getContent());

  // Send the image response.
  ResourceResponse initialResourceResponse(testURL, "image/jpeg",
                                           sizeof(kJpegImage), nullAtom);
  initialResourceResponse.addHTTPHeaderField("chrome-proxy", "q=low");

  imageResource->loader()->didReceiveResponse(
      WrappedResourceResponse(initialResourceResponse));
  imageResource->loader()->didReceiveData(
      reinterpret_cast<const char*>(kJpegImage), sizeof(kJpegImage));

  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(1, observer->imageChangedCount());
  EXPECT_EQ(kJpegImageWidth, observer->imageWidthOnLastImageChanged());
  EXPECT_FALSE(observer->imageNotifyFinishedCalled());
  EXPECT_TRUE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->height());

  // Call reloadIfLoFiOrPlaceholderImage() while the image is still loading.
  imageResource->reloadIfLoFiOrPlaceholderImage(fetcher,
                                                Resource::kReloadAlways);

  EXPECT_EQ(2, observer->imageChangedCount());
  EXPECT_EQ(0, observer->imageWidthOnLastImageChanged());
  // The observer should not have been notified of completion yet, since the
  // image is still loading.
  EXPECT_FALSE(observer->imageNotifyFinishedCalled());

  testThatReloadIsStartedThenServeReload(
      testURL, imageResource, imageResource->getContent(), observer.get(),
      WebCachePolicy::BypassingCache);
}

TEST(ImageResourceTest, ReloadIfLoFiOrPlaceholderForPlaceholder) {
  KURL testURL(ParsedURLString, kTestURL);
  ScopedMockedURLLoad scopedMockedURLLoad(testURL, GetTestFilePath());

  ResourceFetcher* fetcher = createFetcher();
  FetchRequest request(testURL, FetchInitiatorInfo());
  request.setAllowImagePlaceholder();
  ImageResource* imageResource = ImageResource::fetch(request, fetcher);
  EXPECT_EQ(FetchRequest::AllowPlaceholder,
            request.placeholderImageRequestType());
  EXPECT_EQ("bytes=0-2047",
            imageResource->resourceRequest().httpHeaderField("range"));
  std::unique_ptr<MockImageResourceObserver> observer =
      MockImageResourceObserver::create(imageResource->getContent());

  ResourceResponse response(testURL, "image/jpeg",
                            kJpegImageSubrangeWithDimensionsLength, nullAtom);
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

  EXPECT_EQ(ResourceStatus::Cached, imageResource->getStatus());
  EXPECT_TRUE(imageResource->isPlaceholder());
  EXPECT_TRUE(observer->imageNotifyFinishedCalled());

  imageResource->reloadIfLoFiOrPlaceholderImage(fetcher,
                                                Resource::kReloadAlways);

  testThatReloadIsStartedThenServeReload(
      testURL, imageResource, imageResource->getContent(), observer.get(),
      WebCachePolicy::BypassingCache);
}

TEST(ImageResourceTest, SVGImage) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo");
  ImageResource* imageResource = ImageResource::create(ResourceRequest(url));
  std::unique_ptr<MockImageResourceObserver> observer =
      MockImageResourceObserver::create(imageResource->getContent());

  receiveResponse(imageResource, url, "image/svg+xml", kSvgImage,
                  strlen(kSvgImage));

  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(1, observer->imageChangedCount());
  EXPECT_TRUE(observer->imageNotifyFinishedCalled());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isBitmapImage());
}

TEST(ImageResourceTest, SuccessfulRevalidationJpeg) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo");
  ImageResource* imageResource = ImageResource::create(ResourceRequest(url));
  std::unique_ptr<MockImageResourceObserver> observer =
      MockImageResourceObserver::create(imageResource->getContent());

  receiveResponse(imageResource, url, "image/jpeg",
                  reinterpret_cast<const char*>(kJpegImage),
                  sizeof(kJpegImage));

  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(2, observer->imageChangedCount());
  EXPECT_TRUE(observer->imageNotifyFinishedCalled());
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
  EXPECT_EQ(2, observer->imageChangedCount());
  EXPECT_TRUE(observer->imageNotifyFinishedCalled());
  EXPECT_TRUE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->height());
}

TEST(ImageResourceTest, SuccessfulRevalidationSvg) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo");
  ImageResource* imageResource = ImageResource::create(ResourceRequest(url));
  std::unique_ptr<MockImageResourceObserver> observer =
      MockImageResourceObserver::create(imageResource->getContent());

  receiveResponse(imageResource, url, "image/svg+xml", kSvgImage,
                  strlen(kSvgImage));

  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(1, observer->imageChangedCount());
  EXPECT_TRUE(observer->imageNotifyFinishedCalled());
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
  EXPECT_EQ(1, observer->imageChangedCount());
  EXPECT_TRUE(observer->imageNotifyFinishedCalled());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_EQ(200, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(200, imageResource->getContent()->getImage()->height());
}

TEST(ImageResourceTest, FailedRevalidationJpegToJpeg) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo");
  ImageResource* imageResource = ImageResource::create(ResourceRequest(url));
  std::unique_ptr<MockImageResourceObserver> observer =
      MockImageResourceObserver::create(imageResource->getContent());

  receiveResponse(imageResource, url, "image/jpeg",
                  reinterpret_cast<const char*>(kJpegImage),
                  sizeof(kJpegImage));

  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(2, observer->imageChangedCount());
  EXPECT_TRUE(observer->imageNotifyFinishedCalled());
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
  EXPECT_EQ(4, observer->imageChangedCount());
  EXPECT_TRUE(observer->imageNotifyFinishedCalled());
  EXPECT_TRUE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_EQ(50, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(50, imageResource->getContent()->getImage()->height());
}

TEST(ImageResourceTest, FailedRevalidationJpegToSvg) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo");
  ImageResource* imageResource = ImageResource::create(ResourceRequest(url));
  std::unique_ptr<MockImageResourceObserver> observer =
      MockImageResourceObserver::create(imageResource->getContent());

  receiveResponse(imageResource, url, "image/jpeg",
                  reinterpret_cast<const char*>(kJpegImage),
                  sizeof(kJpegImage));

  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(2, observer->imageChangedCount());
  EXPECT_TRUE(observer->imageNotifyFinishedCalled());
  EXPECT_TRUE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->height());

  imageResource->setRevalidatingRequest(ResourceRequest(url));
  receiveResponse(imageResource, url, "image/svg+xml", kSvgImage,
                  strlen(kSvgImage));

  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(3, observer->imageChangedCount());
  EXPECT_TRUE(observer->imageNotifyFinishedCalled());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_EQ(200, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(200, imageResource->getContent()->getImage()->height());
}

TEST(ImageResourceTest, FailedRevalidationSvgToJpeg) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo");
  ImageResource* imageResource = ImageResource::create(ResourceRequest(url));
  std::unique_ptr<MockImageResourceObserver> observer =
      MockImageResourceObserver::create(imageResource->getContent());

  receiveResponse(imageResource, url, "image/svg+xml", kSvgImage,
                  strlen(kSvgImage));

  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(1, observer->imageChangedCount());
  EXPECT_TRUE(observer->imageNotifyFinishedCalled());
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
  EXPECT_EQ(3, observer->imageChangedCount());
  EXPECT_TRUE(observer->imageNotifyFinishedCalled());
  EXPECT_TRUE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->height());
}

TEST(ImageResourceTest, FailedRevalidationSvgToSvg) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo");
  ImageResource* imageResource = ImageResource::create(ResourceRequest(url));
  std::unique_ptr<MockImageResourceObserver> observer =
      MockImageResourceObserver::create(imageResource->getContent());

  receiveResponse(imageResource, url, "image/svg+xml", kSvgImage,
                  strlen(kSvgImage));

  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(1, observer->imageChangedCount());
  EXPECT_TRUE(observer->imageNotifyFinishedCalled());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_EQ(200, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(200, imageResource->getContent()->getImage()->height());

  imageResource->setRevalidatingRequest(ResourceRequest(url));
  receiveResponse(imageResource, url, "image/svg+xml", kSvgImage2,
                  strlen(kSvgImage2));

  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(2, observer->imageChangedCount());
  EXPECT_TRUE(observer->imageNotifyFinishedCalled());
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
  KURL testURL(ParsedURLString, kTestURL);
  ScopedMockedURLLoad scopedMockedURLLoad(testURL, GetTestFilePath());

  ResourceFetcher* fetcher = createFetcher();
  FetchRequest request(testURL, FetchInitiatorInfo());
  ImageResource* imageResource = ImageResource::fetch(request, fetcher);
  std::unique_ptr<MockImageResourceObserver> observer =
      MockImageResourceObserver::create(imageResource->getContent());

  imageResource->loader()->didReceiveResponse(
      WrappedResourceResponse(
          ResourceResponse(testURL, "image/jpeg", 18, nullAtom)),
      nullptr);

  EXPECT_EQ(0, observer->imageChangedCount());

  imageResource->loader()->didReceiveData("notactuallyanimage", 18);

  EXPECT_EQ(ResourceStatus::DecodeError, imageResource->getStatus());
  EXPECT_TRUE(observer->imageNotifyFinishedCalled());
  EXPECT_EQ(ResourceStatus::DecodeError,
            observer->statusOnImageNotifyFinished());
  EXPECT_EQ(1, observer->imageChangedCount());
  EXPECT_FALSE(imageResource->isLoading());
}

TEST(ImageResourceTest, DecodeErrorWithEmptyBody) {
  KURL testURL(ParsedURLString, kTestURL);
  ScopedMockedURLLoad scopedMockedURLLoad(testURL, GetTestFilePath());

  ResourceFetcher* fetcher = createFetcher();
  FetchRequest request(testURL, FetchInitiatorInfo());
  ImageResource* imageResource = ImageResource::fetch(request, fetcher);
  std::unique_ptr<MockImageResourceObserver> observer =
      MockImageResourceObserver::create(imageResource->getContent());

  imageResource->loader()->didReceiveResponse(
      WrappedResourceResponse(
          ResourceResponse(testURL, "image/jpeg", 0, nullAtom)),
      nullptr);

  EXPECT_EQ(ResourceStatus::Pending, imageResource->getStatus());
  EXPECT_FALSE(observer->imageNotifyFinishedCalled());
  EXPECT_EQ(0, observer->imageChangedCount());

  imageResource->loader()->didFinishLoading(0.0, 0, 0);

  EXPECT_EQ(ResourceStatus::DecodeError, imageResource->getStatus());
  EXPECT_TRUE(observer->imageNotifyFinishedCalled());
  EXPECT_EQ(ResourceStatus::DecodeError,
            observer->statusOnImageNotifyFinished());
  EXPECT_EQ(1, observer->imageChangedCount());
  EXPECT_FALSE(imageResource->isLoading());
}

// Testing DecodeError that occurs in didFinishLoading().
// This is similar to DecodeErrorWithEmptyBody, but with non-empty body.
TEST(ImageResourceTest, PartialContentWithoutDimensions) {
  KURL testURL(ParsedURLString, kTestURL);
  ScopedMockedURLLoad scopedMockedURLLoad(testURL, GetTestFilePath());

  ResourceRequest resourceRequest(testURL);
  resourceRequest.setHTTPHeaderField("range", "bytes=0-2");
  FetchRequest request(resourceRequest, FetchInitiatorInfo());
  ResourceFetcher* fetcher = createFetcher();
  ImageResource* imageResource = ImageResource::fetch(request, fetcher);
  std::unique_ptr<MockImageResourceObserver> observer =
      MockImageResourceObserver::create(imageResource->getContent());

  ResourceResponse partialResponse(testURL, "image/jpeg",
                                   kJpegImageSubrangeWithoutDimensionsLength,
                                   nullAtom);
  partialResponse.setHTTPStatusCode(206);
  partialResponse.setHTTPHeaderField(
      "content-range",
      buildContentRange(kJpegImageSubrangeWithoutDimensionsLength,
                        sizeof(kJpegImage)));

  imageResource->loader()->didReceiveResponse(
      WrappedResourceResponse(partialResponse));
  imageResource->loader()->didReceiveData(
      reinterpret_cast<const char*>(kJpegImage),
      kJpegImageSubrangeWithoutDimensionsLength);

  EXPECT_EQ(ResourceStatus::Pending, imageResource->getStatus());
  EXPECT_FALSE(observer->imageNotifyFinishedCalled());
  EXPECT_EQ(0, observer->imageChangedCount());

  imageResource->loader()->didFinishLoading(
      0.0, kJpegImageSubrangeWithoutDimensionsLength,
      kJpegImageSubrangeWithoutDimensionsLength);

  EXPECT_EQ(ResourceStatus::DecodeError, imageResource->getStatus());
  EXPECT_TRUE(observer->imageNotifyFinishedCalled());
  EXPECT_EQ(ResourceStatus::DecodeError,
            observer->statusOnImageNotifyFinished());
  EXPECT_EQ(1, observer->imageChangedCount());
  EXPECT_FALSE(imageResource->isLoading());
}

TEST(ImageResourceTest, FetchDisallowPlaceholder) {
  KURL testURL(ParsedURLString, kTestURL);
  ScopedMockedURLLoad scopedMockedURLLoad(testURL, GetTestFilePath());

  FetchRequest request(testURL, FetchInitiatorInfo());
  ImageResource* imageResource = ImageResource::fetch(request, createFetcher());
  EXPECT_EQ(FetchRequest::DisallowPlaceholder,
            request.placeholderImageRequestType());
  EXPECT_EQ(nullAtom,
            imageResource->resourceRequest().httpHeaderField("range"));
  EXPECT_FALSE(imageResource->isPlaceholder());
  std::unique_ptr<MockImageResourceObserver> observer =
      MockImageResourceObserver::create(imageResource->getContent());

  imageResource->loader()->didReceiveResponse(WrappedResourceResponse(
      ResourceResponse(testURL, "image/jpeg", sizeof(kJpegImage), nullAtom)));
  imageResource->loader()->didReceiveData(
      reinterpret_cast<const char*>(kJpegImage), sizeof(kJpegImage));
  imageResource->loader()->didFinishLoading(0.0, sizeof(kJpegImage),
                                            sizeof(kJpegImage));

  EXPECT_EQ(ResourceStatus::Cached, imageResource->getStatus());
  EXPECT_EQ(sizeof(kJpegImage), imageResource->encodedSize());
  EXPECT_FALSE(imageResource->isPlaceholder());
  EXPECT_LT(0, observer->imageChangedCount());
  EXPECT_EQ(kJpegImageWidth, observer->imageWidthOnLastImageChanged());
  EXPECT_TRUE(observer->imageNotifyFinishedCalled());
  EXPECT_EQ(kJpegImageWidth, observer->imageWidthOnImageNotifyFinished());

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
  ImageResource* imageResource = ImageResource::fetch(request, createFetcher());
  EXPECT_EQ(FetchRequest::DisallowPlaceholder,
            request.placeholderImageRequestType());
  EXPECT_EQ(nullAtom,
            imageResource->resourceRequest().httpHeaderField("range"));
  EXPECT_FALSE(imageResource->isPlaceholder());
}

TEST(ImageResourceTest, FetchAllowPlaceholderPostRequest) {
  KURL testURL(ParsedURLString, kTestURL);
  ScopedMockedURLLoad scopedMockedURLLoad(testURL, GetTestFilePath());
  ResourceRequest resourceRequest(testURL);
  resourceRequest.setHTTPMethod("POST");
  FetchRequest request(resourceRequest, FetchInitiatorInfo());
  request.setAllowImagePlaceholder();
  ImageResource* imageResource = ImageResource::fetch(request, createFetcher());
  EXPECT_EQ(FetchRequest::DisallowPlaceholder,
            request.placeholderImageRequestType());
  EXPECT_EQ(nullAtom,
            imageResource->resourceRequest().httpHeaderField("range"));
  EXPECT_FALSE(imageResource->isPlaceholder());

  imageResource->loader()->cancel();
}

TEST(ImageResourceTest, FetchAllowPlaceholderExistingRangeHeader) {
  KURL testURL(ParsedURLString, kTestURL);
  ScopedMockedURLLoad scopedMockedURLLoad(testURL, GetTestFilePath());
  ResourceRequest resourceRequest(testURL);
  resourceRequest.setHTTPHeaderField("range", "bytes=128-255");
  FetchRequest request(resourceRequest, FetchInitiatorInfo());
  request.setAllowImagePlaceholder();
  ImageResource* imageResource = ImageResource::fetch(request, createFetcher());
  EXPECT_EQ(FetchRequest::DisallowPlaceholder,
            request.placeholderImageRequestType());
  EXPECT_EQ("bytes=128-255",
            imageResource->resourceRequest().httpHeaderField("range"));
  EXPECT_FALSE(imageResource->isPlaceholder());

  imageResource->loader()->cancel();
}

TEST(ImageResourceTest, FetchAllowPlaceholderSuccessful) {
  KURL testURL(ParsedURLString, kTestURL);
  ScopedMockedURLLoad scopedMockedURLLoad(testURL, GetTestFilePath());

  FetchRequest request(testURL, FetchInitiatorInfo());
  request.setAllowImagePlaceholder();
  ImageResource* imageResource = ImageResource::fetch(request, createFetcher());
  EXPECT_EQ(FetchRequest::AllowPlaceholder,
            request.placeholderImageRequestType());
  EXPECT_EQ("bytes=0-2047",
            imageResource->resourceRequest().httpHeaderField("range"));
  EXPECT_TRUE(imageResource->isPlaceholder());
  std::unique_ptr<MockImageResourceObserver> observer =
      MockImageResourceObserver::create(imageResource->getContent());

  ResourceResponse response(testURL, "image/jpeg",
                            kJpegImageSubrangeWithDimensionsLength, nullAtom);
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

  EXPECT_EQ(ResourceStatus::Cached, imageResource->getStatus());
  EXPECT_EQ(kJpegImageSubrangeWithDimensionsLength,
            imageResource->encodedSize());
  EXPECT_TRUE(imageResource->isPlaceholder());
  EXPECT_LT(0, observer->imageChangedCount());
  EXPECT_EQ(kJpegImageWidth, observer->imageWidthOnLastImageChanged());
  EXPECT_TRUE(observer->imageNotifyFinishedCalled());
  EXPECT_EQ(kJpegImageWidth, observer->imageWidthOnImageNotifyFinished());

  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(1, imageResource->getContent()->getImage()->height());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isSVGImage());
}

TEST(ImageResourceTest, FetchAllowPlaceholderUnsuccessful) {
  KURL testURL(ParsedURLString, kTestURL);
  ScopedMockedURLLoad scopedMockedURLLoad(testURL, GetTestFilePath());

  FetchRequest request(testURL, FetchInitiatorInfo());
  request.setAllowImagePlaceholder();
  ImageResource* imageResource = ImageResource::fetch(request, createFetcher());
  EXPECT_EQ(FetchRequest::AllowPlaceholder,
            request.placeholderImageRequestType());
  EXPECT_EQ("bytes=0-2047",
            imageResource->resourceRequest().httpHeaderField("range"));
  EXPECT_TRUE(imageResource->isPlaceholder());
  std::unique_ptr<MockImageResourceObserver> observer =
      MockImageResourceObserver::create(imageResource->getContent());

  const char kBadData[] = "notanimageresponse";

  ResourceResponse badResponse(testURL, "image/jpeg", sizeof(kBadData),
                               nullAtom);
  badResponse.setHTTPStatusCode(206);
  badResponse.setHTTPHeaderField(
      "content-range", buildContentRange(sizeof(kBadData), sizeof(kJpegImage)));

  imageResource->loader()->didReceiveResponse(
      WrappedResourceResponse(badResponse));

  EXPECT_EQ(0, observer->imageChangedCount());

  imageResource->loader()->didReceiveData(kBadData, sizeof(kBadData));

  // The dimensions could not be extracted, so the full original image should be
  // loading.
  EXPECT_FALSE(observer->imageNotifyFinishedCalled());
  EXPECT_EQ(2, observer->imageChangedCount());

  testThatReloadIsStartedThenServeReload(
      testURL, imageResource, imageResource->getContent(), observer.get(),
      WebCachePolicy::BypassingCache);
}

TEST(ImageResourceTest, FetchAllowPlaceholderPartialContentWithoutDimensions) {
  KURL testURL(ParsedURLString, kTestURL);
  ScopedMockedURLLoad scopedMockedURLLoad(testURL, GetTestFilePath());

  FetchRequest request(testURL, FetchInitiatorInfo());
  request.setAllowImagePlaceholder();
  ImageResource* imageResource = ImageResource::fetch(request, createFetcher());
  EXPECT_EQ(FetchRequest::AllowPlaceholder,
            request.placeholderImageRequestType());
  EXPECT_EQ("bytes=0-2047",
            imageResource->resourceRequest().httpHeaderField("range"));
  EXPECT_TRUE(imageResource->isPlaceholder());
  std::unique_ptr<MockImageResourceObserver> observer =
      MockImageResourceObserver::create(imageResource->getContent());

  // TODO(hiroshige): Make the range request header and partial content length
  // consistent. https://crbug.com/689760.
  ResourceResponse partialResponse(testURL, "image/jpeg",
                                   kJpegImageSubrangeWithoutDimensionsLength,
                                   nullAtom);
  partialResponse.setHTTPStatusCode(206);
  partialResponse.setHTTPHeaderField(
      "content-range",
      buildContentRange(kJpegImageSubrangeWithoutDimensionsLength,
                        sizeof(kJpegImage)));

  imageResource->loader()->didReceiveResponse(
      WrappedResourceResponse(partialResponse));
  imageResource->loader()->didReceiveData(
      reinterpret_cast<const char*>(kJpegImage),
      kJpegImageSubrangeWithoutDimensionsLength);

  EXPECT_EQ(0, observer->imageChangedCount());

  imageResource->loader()->didFinishLoading(
      0.0, kJpegImageSubrangeWithoutDimensionsLength,
      kJpegImageSubrangeWithoutDimensionsLength);

  EXPECT_FALSE(observer->imageNotifyFinishedCalled());
  EXPECT_EQ(2, observer->imageChangedCount());

  testThatReloadIsStartedThenServeReload(
      testURL, imageResource, imageResource->getContent(), observer.get(),
      WebCachePolicy::BypassingCache);
}

TEST(ImageResourceTest, FetchAllowPlaceholderThenDisallowPlaceholder) {
  KURL testURL(ParsedURLString, kTestURL);
  ScopedMockedURLLoad scopedMockedURLLoad(testURL, GetTestFilePath());

  ResourceFetcher* fetcher = createFetcher();
  FetchRequest placeholderRequest(testURL, FetchInitiatorInfo());
  placeholderRequest.setAllowImagePlaceholder();
  ImageResource* imageResource =
      ImageResource::fetch(placeholderRequest, fetcher);
  std::unique_ptr<MockImageResourceObserver> observer =
      MockImageResourceObserver::create(imageResource->getContent());

  FetchRequest nonPlaceholderRequest(testURL, FetchInitiatorInfo());
  ImageResource* secondImageResource =
      ImageResource::fetch(nonPlaceholderRequest, fetcher);

  EXPECT_EQ(imageResource, secondImageResource);
  EXPECT_FALSE(observer->imageNotifyFinishedCalled());

  testThatReloadIsStartedThenServeReload(
      testURL, imageResource, imageResource->getContent(), observer.get(),
      WebCachePolicy::UseProtocolCachePolicy);
}

TEST(ImageResourceTest,
     FetchAllowPlaceholderThenDisallowPlaceholderAfterLoaded) {
  KURL testURL(ParsedURLString, kTestURL);
  ScopedMockedURLLoad scopedMockedURLLoad(testURL, GetTestFilePath());

  ResourceFetcher* fetcher = createFetcher();
  FetchRequest placeholderRequest(testURL, FetchInitiatorInfo());
  placeholderRequest.setAllowImagePlaceholder();
  ImageResource* imageResource =
      ImageResource::fetch(placeholderRequest, fetcher);
  std::unique_ptr<MockImageResourceObserver> observer =
      MockImageResourceObserver::create(imageResource->getContent());

  ResourceResponse response(testURL, "image/jpeg",
                            kJpegImageSubrangeWithDimensionsLength, nullAtom);
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

  EXPECT_EQ(ResourceStatus::Cached, imageResource->getStatus());
  EXPECT_EQ(kJpegImageSubrangeWithDimensionsLength,
            imageResource->encodedSize());
  EXPECT_TRUE(imageResource->isPlaceholder());
  EXPECT_LT(0, observer->imageChangedCount());
  EXPECT_TRUE(observer->imageNotifyFinishedCalled());

  FetchRequest nonPlaceholderRequest(testURL, FetchInitiatorInfo());
  ImageResource* secondImageResource =
      ImageResource::fetch(nonPlaceholderRequest, fetcher);
  EXPECT_EQ(imageResource, secondImageResource);

  testThatReloadIsStartedThenServeReload(
      testURL, imageResource, imageResource->getContent(), observer.get(),
      WebCachePolicy::UseProtocolCachePolicy);
}

TEST(ImageResourceTest, FetchAllowPlaceholderFullResponseDecodeSuccess) {
  const struct {
    int statusCode;
    AtomicString contentRange;
  } tests[] = {
      {200, nullAtom},
      {404, nullAtom},
      {206, buildContentRange(sizeof(kJpegImage), sizeof(kJpegImage))},
  };
  for (const auto& test : tests) {
    KURL testURL(ParsedURLString, kTestURL);
    ScopedMockedURLLoad scopedMockedURLLoad(testURL, GetTestFilePath());

    FetchRequest request(testURL, FetchInitiatorInfo());
    request.setAllowImagePlaceholder();
    ImageResource* imageResource =
        ImageResource::fetch(request, createFetcher());
    EXPECT_EQ(FetchRequest::AllowPlaceholder,
              request.placeholderImageRequestType());
    EXPECT_EQ("bytes=0-2047",
              imageResource->resourceRequest().httpHeaderField("range"));
    EXPECT_TRUE(imageResource->isPlaceholder());
    std::unique_ptr<MockImageResourceObserver> observer =
        MockImageResourceObserver::create(imageResource->getContent());

    ResourceResponse response(testURL, "image/jpeg", sizeof(kJpegImage),
                              nullAtom);
    response.setHTTPStatusCode(test.statusCode);
    if (test.contentRange != nullAtom)
      response.setHTTPHeaderField("content-range", test.contentRange);
    imageResource->loader()->didReceiveResponse(
        WrappedResourceResponse(response));
    imageResource->loader()->didReceiveData(
        reinterpret_cast<const char*>(kJpegImage), sizeof(kJpegImage));
    imageResource->loader()->didFinishLoading(0.0, sizeof(kJpegImage),
                                              sizeof(kJpegImage));

    EXPECT_EQ(ResourceStatus::Cached, imageResource->getStatus());
    EXPECT_EQ(sizeof(kJpegImage), imageResource->encodedSize());
    EXPECT_FALSE(imageResource->isPlaceholder());
    EXPECT_LT(0, observer->imageChangedCount());
    EXPECT_EQ(kJpegImageWidth, observer->imageWidthOnLastImageChanged());
    EXPECT_TRUE(observer->imageNotifyFinishedCalled());
    EXPECT_EQ(kJpegImageWidth, observer->imageWidthOnImageNotifyFinished());

    ASSERT_TRUE(imageResource->getContent()->hasImage());
    EXPECT_EQ(1, imageResource->getContent()->getImage()->width());
    EXPECT_EQ(1, imageResource->getContent()->getImage()->height());
    EXPECT_TRUE(imageResource->getContent()->getImage()->isBitmapImage());
  }
}

TEST(ImageResourceTest,
     FetchAllowPlaceholderFullResponseDecodeFailureNoReload) {
  static const char kBadImageData[] = "bad image data";

  const struct {
    int statusCode;
    AtomicString contentRange;
    size_t dataSize;
  } tests[] = {
      {200, nullAtom, sizeof(kBadImageData)},
      {206, buildContentRange(sizeof(kBadImageData), sizeof(kBadImageData)),
       sizeof(kBadImageData)},
      {204, nullAtom, 0},
  };
  for (const auto& test : tests) {
    KURL testURL(ParsedURLString, kTestURL);
    ScopedMockedURLLoad scopedMockedURLLoad(testURL, GetTestFilePath());

    FetchRequest request(testURL, FetchInitiatorInfo());
    request.setAllowImagePlaceholder();
    ImageResource* imageResource =
        ImageResource::fetch(request, createFetcher());
    EXPECT_EQ(FetchRequest::AllowPlaceholder,
              request.placeholderImageRequestType());
    EXPECT_EQ("bytes=0-2047",
              imageResource->resourceRequest().httpHeaderField("range"));
    EXPECT_TRUE(imageResource->isPlaceholder());
    std::unique_ptr<MockImageResourceObserver> observer =
        MockImageResourceObserver::create(imageResource->getContent());

    ResourceResponse response(testURL, "image/jpeg", test.dataSize, nullAtom);
    response.setHTTPStatusCode(test.statusCode);
    if (test.contentRange != nullAtom)
      response.setHTTPHeaderField("content-range", test.contentRange);
    imageResource->loader()->didReceiveResponse(
        WrappedResourceResponse(response));
    imageResource->loader()->didReceiveData(kBadImageData, test.dataSize);

    EXPECT_EQ(ResourceStatus::DecodeError, imageResource->getStatus());
    EXPECT_FALSE(imageResource->isPlaceholder());
  }
}

TEST(ImageResourceTest,
     FetchAllowPlaceholderFullResponseDecodeFailureWithReload) {
  const int kStatusCodes[] = {404, 500};
  for (int statusCode : kStatusCodes) {
    KURL testURL(ParsedURLString, kTestURL);
    ScopedMockedURLLoad scopedMockedURLLoad(testURL, GetTestFilePath());

    FetchRequest request(testURL, FetchInitiatorInfo());
    request.setAllowImagePlaceholder();
    ImageResource* imageResource =
        ImageResource::fetch(request, createFetcher());
    EXPECT_EQ(FetchRequest::AllowPlaceholder,
              request.placeholderImageRequestType());
    EXPECT_EQ("bytes=0-2047",
              imageResource->resourceRequest().httpHeaderField("range"));
    EXPECT_TRUE(imageResource->isPlaceholder());
    std::unique_ptr<MockImageResourceObserver> observer =
        MockImageResourceObserver::create(imageResource->getContent());

    static const char kBadImageData[] = "bad image data";

    ResourceResponse response(testURL, "image/jpeg", sizeof(kBadImageData),
                              nullAtom);
    response.setHTTPStatusCode(statusCode);
    imageResource->loader()->didReceiveResponse(
        WrappedResourceResponse(response));
    imageResource->loader()->didReceiveData(kBadImageData,
                                            sizeof(kBadImageData));

    EXPECT_FALSE(observer->imageNotifyFinishedCalled());

    // The dimensions could not be extracted, and the response code was a 4xx
    // error, so the full original image should be loading.
    testThatReloadIsStartedThenServeReload(
        testURL, imageResource, imageResource->getContent(), observer.get(),
        WebCachePolicy::BypassingCache);
  }
}

TEST(ImageResourceTest, PeriodicFlushTest) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;
  KURL testURL(ParsedURLString, kTestURL);
  ScopedMockedURLLoad scopedMockedURLLoad(testURL, GetTestFilePath());
  ResourceRequest request = ResourceRequest(testURL);
  ImageResource* imageResource = ImageResource::create(request);
  imageResource->setStatus(ResourceStatus::Pending);

  std::unique_ptr<MockImageResourceObserver> observer =
      MockImageResourceObserver::create(imageResource->getContent());

  // Send the image response.
  ResourceResponse resourceResponse(KURL(), "image/jpeg", sizeof(kJpegImage2),
                                    nullAtom);
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
  EXPECT_EQ(1, observer->imageChangedCount());

  platform->runForPeriodSeconds(1.);
  platform->advanceClockSeconds(1.);

  // Sanity check that we created an image after appending |meaningfulImageSize|
  // bytes just once.
  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_EQ(1, observer->imageChangedCount());

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
      EXPECT_EQ(flushCount, observer->imageChangedCount());

      ++bytesSent;
      platform->runForPeriodSeconds(0.2001);
    }
  }

  // Increasing time by a large number only causes one extra flush.
  platform->runForPeriodSeconds(10.);
  platform->advanceClockSeconds(10.);
  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(4, observer->imageChangedCount());

  // Append the rest of the data and finish (which causes another flush).
  imageResource->appendData(
      reinterpret_cast<const char*>(kJpegImage2) + bytesSent,
      sizeof(kJpegImage2) - bytesSent);
  imageResource->finish();

  EXPECT_FALSE(imageResource->errorOccurred());
  ASSERT_TRUE(imageResource->getContent()->hasImage());
  EXPECT_FALSE(imageResource->getContent()->getImage()->isNull());
  EXPECT_EQ(5, observer->imageChangedCount());
  EXPECT_TRUE(observer->imageNotifyFinishedCalled());
  EXPECT_TRUE(imageResource->getContent()->getImage()->isBitmapImage());
  EXPECT_EQ(50, imageResource->getContent()->getImage()->width());
  EXPECT_EQ(50, imageResource->getContent()->getImage()->height());

  WTF::setTimeFunctionsForTesting(nullptr);
}

}  // namespace
}  // namespace blink
