// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/image_annotation/annotator.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "net/http/http_status_code.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom-shared.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace image_annotation {

namespace {

using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::SizeIs;

constexpr char kTestServerUrl[] = "https://test_ia_server.com/v1:ocr";

// Example image URLs.

constexpr char kImage1Url[] = "https://www.example.com/image1.jpg";
constexpr char kImage2Url[] = "https://www.example.com/image2.jpg";
constexpr char kImage3Url[] = "https://www.example.com/image3.jpg";

// Example server requests / responses.

// Template for a request for a single image.
constexpr char kTemplateRequest[] = R"(
{
  "imageRequests": [{
    "imageId": "%s",
    "imageBytes": "%s",
    "engineParameters": [{
      "ocrParameters": {}
    }]
  }]
}
)";

// Batch request for |kImage1Url|, |kImage2Url| and |kImage3Url|.
constexpr char kBatchRequest[] = R"(
{
  "imageRequests": [
    {
      "imageId": "https://www.example.com/image1.jpg",
      "imageBytes": "AQID",
      "engineParameters": [{
        "ocrParameters": {}
      }]
    },
    {
      "imageId": "https://www.example.com/image2.jpg",
      "imageBytes": "BAUG",
      "engineParameters": [{
        "ocrParameters": {}
      }]
    },
    {
      "imageId": "https://www.example.com/image3.jpg",
      "imageBytes": "BwgJ",
      "engineParameters": [{
        "ocrParameters": {}
      }]
    }
  ]
})";

// Successful text extraction for |kImage1Url|.
constexpr char kSuccessResponse[] = R"(
{
  "results": [
    {
      "imageId": "https://www.example.com/image1.jpg",
      "engineResults": [{
        "ocrEngine": {
          "ocrRegions": [
            {
              "words": [
                 {
                   "detectedText": "Region",
                   "confidenceScore": 1.0
                 },
                 {
                   "detectedText": "1",
                   "confidenceScore": 1.0
                 }
              ]
            },
            {
              "words": [
                 {
                   "detectedText": "Region",
                   "confidenceScore": 1.0
                 },
                 {
                   "detectedText": "2",
                   "confidenceScore": 1.0
                 }
              ]
            }
          ]
        }
      }]
    }
  ]
}
)";

// Failed text extraction for |kImage1Url|.
constexpr char kErrorResponse[] = R"(
{
  "results": [{
    "imageId": "https://www.example.com/image1.jpg",
    "engineResults": [{
      "status": {
        "code": 8,
        "message": "Resource exhaused"
      }
    }]
  }]
}
)";

// Batch response containing successful annotations for |kImage1Url| and
// |kImage2Url|, and a failure for |kImage3Url|.
//
// The results also appear "out of order" (i.e. image 2 comes before image 1).
constexpr char kBatchResponse[] = R"(
{
  "results": [
    {
      "imageId": "https://www.example.com/image2.jpg",
      "engineResults": [{
        "ocrEngine": {
          "ocrRegions": [{
            "words": [{
              "detectedText": "2",
              "confidenceScore": 1.0
            }]
          }]
        }
      }]
    },
    {
      "imageId": "https://www.example.com/image1.jpg",
      "engineResults": [{
        "ocrEngine": {
          "ocrRegions": [{
            "words": [{
              "detectedText": "1",
              "confidenceScore": 1.0
            }]
          }]
        }
      }]
    },
    {
      "imageId": "https://www.example.com/image3.jpg",
      "engineResults": [{
        "status": {
          "code": 8,
          "message": "Resource exhaused"
        }
      }]
    }
  ]
})";

constexpr base::TimeDelta kThrottle = base::TimeDelta::FromSeconds(1);

// An image processor that holds and exposes the callbacks it is passed.
class TestImageProcessor : public mojom::ImageProcessor {
 public:
  mojom::ImageProcessorPtr GetPtr() {
    mojom::ImageProcessorPtr ptr;
    bindings_.AddBinding(this, mojo::MakeRequest(&ptr));
    return ptr;
  }

  void Reset() {
    bindings_.CloseAllBindings();
    callbacks_.clear();
  }

  void GetJpgImageData(GetJpgImageDataCallback callback) override {
    callbacks_.push_back(std::move(callback));
  }

  std::vector<GetJpgImageDataCallback>& callbacks() { return callbacks_; }

 private:
  std::vector<GetJpgImageDataCallback> callbacks_;

  mojo::BindingSet<mojom::ImageProcessor> bindings_;
};

// A class that supports test URL loading for the "server" use case: where
// all request URLs have the same prefix and differ only in suffix and body
// content.
class TestServerURLLoaderFactory {
 public:
  TestServerURLLoaderFactory(const std::string& server_url_prefix)
      : server_url_prefix_(server_url_prefix),
        shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &loader_factory_)) {}

  const std::vector<network::TestURLLoaderFactory::PendingRequest>& requests() {
    return *loader_factory_.pending_requests();
  }

  // Expects that the earliest received request has the given URL and body, and
  // replies with the given response.
  //
  // Consumes the earliest received request (i.e. a subsequent call will apply
  // to the second-earliest received request and so on).
  void ExpectRequestAndSimulateResponse(
      const std::string& expected_url_suffix,
      const std::string& expected_body,
      const std::string& response,
      const net::HttpStatusCode response_code) {
    const std::string expected_url = server_url_prefix_ + expected_url_suffix;

    const std::vector<network::TestURLLoaderFactory::PendingRequest>&
        pending_requests = *loader_factory_.pending_requests();

    CHECK(!pending_requests.empty());
    const network::ResourceRequest& request = pending_requests.front().request;

    // Assert that the earliest request is for the given URL.
    CHECK_EQ(request.url, GURL(expected_url));

    // Extract request body.
    std::string actual_body;
    if (request.request_body) {
      const std::vector<network::DataElement>* const elements =
          request.request_body->elements();

      // We only support the simplest body structure.
      CHECK(elements && elements->size() == 1 &&
            (*elements)[0].type() == network::mojom::DataElementType::kBytes);

      actual_body =
          std::string((*elements)[0].bytes(), (*elements)[0].length());
    }

    EXPECT_THAT(actual_body, Eq(expected_body));

    // Guaranteed to match the first request based on URL.
    loader_factory_.SimulateResponseForPendingRequest(expected_url, response,
                                                      response_code);
  }

  scoped_refptr<network::SharedURLLoaderFactory> AsSharedURLLoaderFactory() {
    return shared_loader_factory_;
  }

 private:
  const std::string server_url_prefix_;
  network::TestURLLoaderFactory loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_loader_factory_;
};

// Returns a "canonically" formatted version of a JSON string by parsing and
// then rewriting it.
std::string ReformatJson(const std::string& in) {
  const std::unique_ptr<base::Value> json =
      base::JSONReader::ReadDeprecated(in);
  CHECK(json);

  std::string out;
  base::JSONWriter::Write(*json, &out);

  return out;
}

// Receives the result of an annotation request and writes the result data into
// the given variables.
void ReportResult(base::Optional<mojom::AnnotateImageError>* const error,
                  base::Optional<std::string>* const ocr_text,
                  mojom::AnnotateImageResultPtr result) {
  if (result->which() == mojom::AnnotateImageResult::Tag::ERROR_CODE) {
    *error = result->get_error_code();
  } else {
    *ocr_text = std::move(result->get_ocr_text());
  }
}

}  // namespace

// Test that OCR works for one client, and that the cache is populated.
TEST(AnnotatorTest, SuccessAndCache) {
  base::test::ScopedTaskEnvironment test_task_env(
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory("https://test_ia_server.com/v1:");

  Annotator annotator(GURL(kTestServerUrl), kThrottle, 1 /* batch_size */,
                      1.0 /* min_ocr_confidence */,
                      test_url_factory.AsSharedURLLoaderFactory());
  TestImageProcessor processor;

  // First call performs original image annotation.
  {
    base::Optional<mojom::AnnotateImageError> error;
    base::Optional<std::string> ocr_text;

    annotator.AnnotateImage(kImage1Url, processor.GetPtr(),
                            base::BindOnce(&ReportResult, &error, &ocr_text));
    test_task_env.RunUntilIdle();

    // Annotator should have asked processor for pixels.
    ASSERT_THAT(processor.callbacks(), SizeIs(1));

    // Send back image data.
    std::move(processor.callbacks()[0]).Run({1, 2, 3});
    processor.callbacks().pop_back();
    test_task_env.RunUntilIdle();

    // No request should be sent yet (because service is waiting to batch up
    // multiple requests).
    EXPECT_THAT(test_url_factory.requests(), IsEmpty());
    test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));
    test_task_env.RunUntilIdle();

    // HTTP request should have been made.
    test_url_factory.ExpectRequestAndSimulateResponse(
        "ocr",
        ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
        kSuccessResponse, net::HTTP_OK);
    test_task_env.RunUntilIdle();

    // HTTP response should have completed and callback should have been called.
    ASSERT_THAT(error, Eq(base::nullopt));
    EXPECT_THAT(ocr_text, Eq("Region 1\nRegion 2"));
  }

  // Second call uses cached results.
  {
    base::Optional<mojom::AnnotateImageError> error;
    base::Optional<std::string> ocr_text;

    annotator.AnnotateImage(kImage1Url, processor.GetPtr(),
                            base::BindOnce(&ReportResult, &error, &ocr_text));
    test_task_env.RunUntilIdle();

    // Pixels shouldn't be requested.
    ASSERT_THAT(processor.callbacks(), IsEmpty());

    // Results should have been directly returned without any server call.
    ASSERT_THAT(error, Eq(base::nullopt));
    EXPECT_THAT(ocr_text, Eq("Region 1\nRegion 2"));
  }
}

// Test that HTTP failure is gracefully handled.
TEST(AnnotatorTest, HttpError) {
  base::test::ScopedTaskEnvironment test_task_env(
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory("https://test_ia_server.com/v1:");

  Annotator annotator(GURL(kTestServerUrl), kThrottle, 1 /* batch_size */,
                      1.0 /* min_ocr_confidence */,
                      test_url_factory.AsSharedURLLoaderFactory());

  TestImageProcessor processor;
  base::Optional<mojom::AnnotateImageError> error;
  base::Optional<std::string> ocr_text;

  annotator.AnnotateImage(kImage1Url, processor.GetPtr(),
                          base::BindOnce(&ReportResult, &error, &ocr_text));
  test_task_env.RunUntilIdle();

  // Annotator should have asked processor for pixels.
  ASSERT_THAT(processor.callbacks(), SizeIs(1));

  // Send back image data.
  std::move(processor.callbacks()[0]).Run({1, 2, 3});
  processor.callbacks().pop_back();
  test_task_env.RunUntilIdle();

  // No request should be sent yet (because service is waiting to batch up
  // multiple requests).
  EXPECT_THAT(test_url_factory.requests(), IsEmpty());
  test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // HTTP request should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "ocr",
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      "", net::HTTP_INTERNAL_SERVER_ERROR);
  test_task_env.RunUntilIdle();

  // HTTP response should have completed and callback should have been called.
  EXPECT_THAT(error, Eq(mojom::AnnotateImageError::kFailure));
  EXPECT_THAT(ocr_text, Eq(base::nullopt));
}

// Test that backend failure is gracefully handled.
TEST(AnnotatorTest, BackendError) {
  base::test::ScopedTaskEnvironment test_task_env(
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory("https://test_ia_server.com/v1:");

  Annotator annotator(GURL(kTestServerUrl), kThrottle, 1 /* batch_size */,
                      1.0 /* min_ocr_confidence */,
                      test_url_factory.AsSharedURLLoaderFactory());

  TestImageProcessor processor;
  base::Optional<mojom::AnnotateImageError> error;
  base::Optional<std::string> ocr_text;

  annotator.AnnotateImage(kImage1Url, processor.GetPtr(),
                          base::BindOnce(&ReportResult, &error, &ocr_text));
  test_task_env.RunUntilIdle();

  // Annotator should have asked processor for pixels.
  ASSERT_THAT(processor.callbacks(), SizeIs(1));

  // Send back image data.
  std::move(processor.callbacks()[0]).Run({1, 2, 3});
  processor.callbacks().pop_back();
  test_task_env.RunUntilIdle();

  // No request should be sent yet (because service is waiting to batch up
  // multiple requests).
  EXPECT_THAT(test_url_factory.requests(), IsEmpty());
  test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // HTTP request should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "ocr",
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      kErrorResponse, net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // HTTP response should have completed and callback should have been called
  // with an error status.
  EXPECT_THAT(error, Eq(mojom::AnnotateImageError::kFailure));
  EXPECT_THAT(ocr_text, Eq(base::nullopt));
}

// Test that server failure (i.e. nonsense response) is gracefully handled.
TEST(AnnotatorTest, ServerError) {
  base::test::ScopedTaskEnvironment test_task_env(
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory("https://test_ia_server.com/v1:");

  Annotator annotator(GURL(kTestServerUrl), kThrottle, 1 /* batch_size */,
                      1.0 /* min_ocr_confidence */,
                      test_url_factory.AsSharedURLLoaderFactory());

  TestImageProcessor processor;
  base::Optional<mojom::AnnotateImageError> error;
  base::Optional<std::string> ocr_text;

  annotator.AnnotateImage(kImage1Url, processor.GetPtr(),
                          base::BindOnce(&ReportResult, &error, &ocr_text));
  test_task_env.RunUntilIdle();

  // Annotator should have asked processor for pixels.
  ASSERT_THAT(processor.callbacks(), SizeIs(1));

  // Send back image data.
  std::move(processor.callbacks()[0]).Run({1, 2, 3});
  processor.callbacks().pop_back();
  test_task_env.RunUntilIdle();

  // No request should be sent yet (because service is waiting to batch up
  // multiple requests).
  EXPECT_THAT(test_url_factory.requests(), IsEmpty());
  test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // HTTP request should have been made; respond with nonsense string.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "ocr",
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      "Hello, world!", net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // HTTP response should have completed and callback should have been called
  // with an error status.
  EXPECT_THAT(error, Eq(mojom::AnnotateImageError::kFailure));
  EXPECT_THAT(ocr_text, Eq(base::nullopt));
}

// Test that work is reassigned if a processor fails.
TEST(AnnotatorTest, ProcessorFails) {
  base::test::ScopedTaskEnvironment test_task_env(
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory("https://test_ia_server.com/v1:");

  Annotator annotator(GURL(kTestServerUrl), kThrottle, 1 /* batch_size */,
                      1.0 /* min_ocr_confidence */,
                      test_url_factory.AsSharedURLLoaderFactory());

  TestImageProcessor processor[3];
  base::Optional<mojom::AnnotateImageError> error[3];
  base::Optional<std::string> ocr_text[3];

  for (int i = 0; i < 3; ++i) {
    annotator.AnnotateImage(
        kImage1Url, processor[i].GetPtr(),
        base::BindOnce(&ReportResult, &error[i], &ocr_text[i]));
  }
  test_task_env.RunUntilIdle();

  // Annotator should have asked processor 1 for image 1's pixels.
  ASSERT_THAT(processor[0].callbacks(), SizeIs(1));
  ASSERT_THAT(processor[1].callbacks(), IsEmpty());
  ASSERT_THAT(processor[2].callbacks(), IsEmpty());

  // Make processor 1 fail by returning empty bytes.
  std::move(processor[0].callbacks()[0]).Run({});
  processor[0].callbacks().pop_back();
  test_task_env.RunUntilIdle();

  // Annotator should have asked processor 2 for image 1's pixels.
  ASSERT_THAT(processor[0].callbacks(), IsEmpty());
  ASSERT_THAT(processor[1].callbacks(), SizeIs(1));
  ASSERT_THAT(processor[2].callbacks(), IsEmpty());

  // Send back image data.
  std::move(processor[1].callbacks()[0]).Run({1, 2, 3});
  processor[1].callbacks().pop_back();
  test_task_env.RunUntilIdle();

  // No request should be sent yet (because service is waiting to batch up
  // multiple requests).
  EXPECT_THAT(test_url_factory.requests(), IsEmpty());
  test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // HTTP request for image 1 should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "ocr",
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      kSuccessResponse, net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // Annotator should have called all callbacks, but request 1 received an error
  // when we returned empty bytes.
  ASSERT_THAT(error, ElementsAre(mojom::AnnotateImageError::kFailure,
                                 base::nullopt, base::nullopt));
  EXPECT_THAT(ocr_text, ElementsAre(base::nullopt, "Region 1\nRegion 2",
                                    "Region 1\nRegion 2"));
}

// Test that work is reassigned if processor dies.
TEST(AnnotatorTest, ProcessorDies) {
  base::test::ScopedTaskEnvironment test_task_env(
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory("https://test_ia_server.com/v1:");

  Annotator annotator(GURL(kTestServerUrl), kThrottle, 1 /* batch_size */,
                      1.0 /* min_ocr_confidence */,
                      test_url_factory.AsSharedURLLoaderFactory());

  TestImageProcessor processor[3];
  base::Optional<mojom::AnnotateImageError> error[3];
  base::Optional<std::string> ocr_text[3];

  for (int i = 0; i < 3; ++i) {
    annotator.AnnotateImage(
        kImage1Url, processor[i].GetPtr(),
        base::BindOnce(&ReportResult, &error[i], &ocr_text[i]));
  }
  test_task_env.RunUntilIdle();

  // Annotator should have asked processor 1 for image 1's pixels.
  ASSERT_THAT(processor[0].callbacks(), SizeIs(1));
  ASSERT_THAT(processor[1].callbacks(), IsEmpty());
  ASSERT_THAT(processor[2].callbacks(), IsEmpty());

  // Kill processor 1.
  processor[0].Reset();
  test_task_env.RunUntilIdle();

  // Annotator should have asked processor 2 for image 1's pixels.
  ASSERT_THAT(processor[0].callbacks(), IsEmpty());
  ASSERT_THAT(processor[1].callbacks(), SizeIs(1));
  ASSERT_THAT(processor[2].callbacks(), IsEmpty());

  // Send back image data.
  std::move(processor[1].callbacks()[0]).Run({1, 2, 3});
  processor[1].callbacks().pop_back();
  test_task_env.RunUntilIdle();

  // No request should be sent yet (because service is waiting to batch up
  // multiple requests).
  EXPECT_THAT(test_url_factory.requests(), IsEmpty());
  test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // HTTP request for image 1 should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "ocr",
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      kSuccessResponse, net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // Annotator should have called all callbacks, but request 1 was canceled when
  // we reset processor 1.
  ASSERT_THAT(error, ElementsAre(mojom::AnnotateImageError::kCanceled,
                                 base::nullopt, base::nullopt));
  EXPECT_THAT(ocr_text, ElementsAre(base::nullopt, "Region 1\nRegion 2",
                                    "Region 1\nRegion 2"));
}

// Test that multiple concurrent requests are handled in the same batch.
TEST(AnnotatorTest, ConcurrentSameBatch) {
  base::test::ScopedTaskEnvironment test_task_env(
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory("https://test_ia_server.com/v1:");

  Annotator annotator(GURL(kTestServerUrl), kThrottle, 3 /* batch_size */,
                      1.0 /* min_ocr_confidence */,
                      test_url_factory.AsSharedURLLoaderFactory());

  TestImageProcessor processor[3];
  base::Optional<mojom::AnnotateImageError> error[3];
  base::Optional<std::string> ocr_text[3];

  // Request OCR for images 1, 2 and 3.
  annotator.AnnotateImage(
      kImage1Url, processor[0].GetPtr(),
      base::BindOnce(&ReportResult, &error[0], &ocr_text[0]));
  annotator.AnnotateImage(
      kImage2Url, processor[1].GetPtr(),
      base::BindOnce(&ReportResult, &error[1], &ocr_text[1]));
  annotator.AnnotateImage(
      kImage3Url, processor[2].GetPtr(),
      base::BindOnce(&ReportResult, &error[2], &ocr_text[2]));
  test_task_env.RunUntilIdle();

  // Annotator should have asked processor 1 for image 1's pixels, processor
  // 2 for image 2's pixels and processor 3 for image 3's pixels.
  ASSERT_THAT(processor[0].callbacks(), SizeIs(1));
  ASSERT_THAT(processor[1].callbacks(), SizeIs(1));
  ASSERT_THAT(processor[2].callbacks(), SizeIs(1));

  // Send back image data.
  std::move(processor[0].callbacks()[0]).Run({1, 2, 3});
  processor[0].callbacks().pop_back();
  std::move(processor[1].callbacks()[0]).Run({4, 5, 6});
  processor[1].callbacks().pop_back();
  std::move(processor[2].callbacks()[0]).Run({7, 8, 9});
  processor[2].callbacks().pop_back();
  test_task_env.RunUntilIdle();

  // No request should be sent yet (because service is waiting to batch up
  // multiple requests).
  EXPECT_THAT(test_url_factory.requests(), IsEmpty());
  test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // A single HTTP request for all images should have been sent.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "ocr", ReformatJson(kBatchRequest), kBatchResponse, net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // Annotator should have called each callback with its corresponding text or
  // failure.
  ASSERT_THAT(error, ElementsAre(base::nullopt, base::nullopt,
                                 mojom::AnnotateImageError::kFailure));
  EXPECT_THAT(ocr_text, ElementsAre("1", "2", base::nullopt));
}

// Test that multiple concurrent requests are handled in separate batches.
TEST(AnnotatorTest, ConcurrentSeparateBatches) {
  base::test::ScopedTaskEnvironment test_task_env(
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory("https://test_ia_server.com/v1:");

  Annotator annotator(GURL(kTestServerUrl), kThrottle, 3 /* batch_size */,
                      1.0 /* min_ocr_confidence */,
                      test_url_factory.AsSharedURLLoaderFactory());

  TestImageProcessor processor[2];
  base::Optional<mojom::AnnotateImageError> error[2];
  base::Optional<std::string> ocr_text[2];

  // Request OCR for image 1.
  annotator.AnnotateImage(
      kImage1Url, processor[0].GetPtr(),
      base::BindOnce(&ReportResult, &error[0], &ocr_text[0]));
  test_task_env.RunUntilIdle();

  // Annotator should have asked processor 1 for image 1's pixels.
  ASSERT_THAT(processor[0].callbacks(), SizeIs(1));
  ASSERT_THAT(processor[1].callbacks(), IsEmpty());

  // Send back image 1 data.
  std::move(processor[0].callbacks()[0]).Run({1, 2, 3});
  processor[0].callbacks().pop_back();
  test_task_env.RunUntilIdle();

  // No request should be sent yet (because service is waiting to batch up
  // multiple requests).
  EXPECT_THAT(test_url_factory.requests(), IsEmpty());
  test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Request OCR for image 2.
  annotator.AnnotateImage(
      kImage2Url, processor[1].GetPtr(),
      base::BindOnce(&ReportResult, &error[1], &ocr_text[1]));
  test_task_env.RunUntilIdle();

  // Annotator should have asked processor 2 for image 2's pixels.
  ASSERT_THAT(processor[0].callbacks(), IsEmpty());
  ASSERT_THAT(processor[1].callbacks(), SizeIs(1));

  // Send back image 2 data.
  std::move(processor[1].callbacks()[0]).Run({4, 5, 6});
  processor[1].callbacks().pop_back();
  test_task_env.RunUntilIdle();

  // Only the HTTP request for image 1 should have been made (the service is
  // still waiting to make the batch that will include the request for image
  // 2).
  test_url_factory.ExpectRequestAndSimulateResponse(
      "ocr",
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      R"({
           "results": [{
             "imageId": "https://www.example.com/image1.jpg",
             "engineResults": [{
               "ocrEngine": {
                 "ocrRegions": [{
                   "words": [{
                     "detectedText": "1",
                     "confidenceScore": 1.0
                   }]
                 }]
               }
             }]
           }]
         })",
      net::HTTP_OK);
  EXPECT_THAT(test_url_factory.requests(), IsEmpty());

  test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Now the HTTP request for image 2 should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "ocr",
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage2Url, "BAUG")),
      R"({
           "results": [{
             "imageId": "https://www.example.com/image2.jpg",
             "engineResults": [{
               "ocrEngine": {
                 "ocrRegions": [{
                   "words": [{
                     "detectedText": "2",
                     "confidenceScore": 1.0
                   }]
                 }]
               }
             }]
           }]
         })",
      net::HTTP_OK);

  test_task_env.RunUntilIdle();

  // Annotator should have called each callback with its corresponding text.
  ASSERT_THAT(error, ElementsAre(base::nullopt, base::nullopt));
  EXPECT_THAT(ocr_text, ElementsAre("1", "2"));
}

// Test that work is not duplicated if it is already ongoing.
TEST(AnnotatorTest, DuplicateWork) {
  base::test::ScopedTaskEnvironment test_task_env(
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory("https://test_ia_server.com/v1:");

  Annotator annotator(GURL(kTestServerUrl), kThrottle, 1 /* batch_size */,
                      1.0 /* min_ocr_confidence */,
                      test_url_factory.AsSharedURLLoaderFactory());

  TestImageProcessor processor[4];
  base::Optional<mojom::AnnotateImageError> error[4];
  base::Optional<std::string> ocr_text[4];

  // First request annotation of the image with processor 1.
  annotator.AnnotateImage(
      kImage1Url, processor[0].GetPtr(),
      base::BindOnce(&ReportResult, &error[0], &ocr_text[0]));
  test_task_env.RunUntilIdle();

  // Annotator should have asked processor 1 for the image's pixels.
  ASSERT_THAT(processor[0].callbacks(), SizeIs(1));
  ASSERT_THAT(processor[1].callbacks(), IsEmpty());
  ASSERT_THAT(processor[2].callbacks(), IsEmpty());
  ASSERT_THAT(processor[3].callbacks(), IsEmpty());

  // Now request annotation of the image with processor 2.
  annotator.AnnotateImage(
      kImage1Url, processor[1].GetPtr(),
      base::BindOnce(&ReportResult, &error[1], &ocr_text[1]));
  test_task_env.RunUntilIdle();

  // Annotator *should not* have asked processor 2 for the image's pixels (since
  // processor 1 is already handling that).
  ASSERT_THAT(processor[0].callbacks(), SizeIs(1));
  ASSERT_THAT(processor[1].callbacks(), IsEmpty());
  ASSERT_THAT(processor[2].callbacks(), IsEmpty());
  ASSERT_THAT(processor[3].callbacks(), IsEmpty());

  // Get processor 1 to reply with bytes for the image.
  std::move(processor[0].callbacks()[0]).Run({1, 2, 3});
  processor[0].callbacks().pop_back();
  test_task_env.RunUntilIdle();

  // Now request annotation of the image with processor 3.
  annotator.AnnotateImage(
      kImage1Url, processor[2].GetPtr(),
      base::BindOnce(&ReportResult, &error[2], &ocr_text[2]));
  test_task_env.RunUntilIdle();

  // Annotator *should not* have asked processor 3 for the image's pixels (since
  // it has already has the pixels in the HTTP request queue).
  ASSERT_THAT(processor[0].callbacks(), IsEmpty());
  ASSERT_THAT(processor[1].callbacks(), IsEmpty());
  ASSERT_THAT(processor[2].callbacks(), IsEmpty());
  ASSERT_THAT(processor[3].callbacks(), IsEmpty());
  EXPECT_THAT(test_url_factory.requests(), IsEmpty());

  // Allow batch HTTP request to be sent off and then request annotation of the
  // image with processor 4.
  test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_THAT(test_url_factory.requests(), SizeIs(1));
  annotator.AnnotateImage(
      kImage1Url, processor[3].GetPtr(),
      base::BindOnce(&ReportResult, &error[3], &ocr_text[3]));
  test_task_env.RunUntilIdle();

  // Annotator *should not* have asked processor 4 for the image's pixels (since
  // an HTTP request for the image is already in process).
  ASSERT_THAT(processor[0].callbacks(), IsEmpty());
  ASSERT_THAT(processor[1].callbacks(), IsEmpty());
  ASSERT_THAT(processor[2].callbacks(), IsEmpty());
  ASSERT_THAT(processor[3].callbacks(), IsEmpty());

  // HTTP request for the image should have been made (with bytes obtained from
  // processor 1).
  test_url_factory.ExpectRequestAndSimulateResponse(
      "ocr",
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      kSuccessResponse, net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // Annotator should have called all callbacks with annotation results.
  ASSERT_THAT(error, ElementsAre(base::nullopt, base::nullopt, base::nullopt,
                                 base::nullopt));
  EXPECT_THAT(ocr_text,
              ElementsAre("Region 1\nRegion 2", "Region 1\nRegion 2",
                          "Region 1\nRegion 2", "Region 1\nRegion 2"));
}

}  // namespace image_annotation
