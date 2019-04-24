// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/image_annotation/annotator.h"

#include <cstring>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_data_decoder_service.h"
#include "services/data_decoder/public/mojom/constants.mojom.h"
#include "services/data_decoder/public/mojom/json_parser.mojom.h"
#include "services/image_annotation/image_annotation_metrics.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom-shared.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace image_annotation {

namespace {

using base::Bucket;
using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::SizeIs;
using testing::UnorderedElementsAre;

constexpr char kTestServerUrl[] = "https://ia-pa.googleapis.com/v1/annotation";

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
    "engineParameters": [
      {"ocrParameters": {}},
      {"descriptionParameters": {}}
    ]
  }]
}
)";

// Batch request for |kImage1Url|, |kImage2Url| and |kImage3Url|.
constexpr char kBatchRequest[] = R"(
{
  "imageRequests": [
    {
      "imageId": "https://www.example.com/image3.jpg",
      "imageBytes": "BwgJ",
      "engineParameters": [
        {"ocrParameters": {}},
        {"descriptionParameters": {}}
      ]
    },
    {
      "imageId": "https://www.example.com/image2.jpg",
      "imageBytes": "BAUG",
      "engineParameters": [
        {"ocrParameters": {}},
        {"descriptionParameters": {}}
      ]
    },
    {
      "imageId": "https://www.example.com/image1.jpg",
      "imageBytes": "AQID",
      "engineParameters": [
        {"ocrParameters": {}},
        {"descriptionParameters": {}}
      ]
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
        "status": {},
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
      },
      "ocrEngine": {}
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
        "status": {},
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
        "status": {},
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
        },
        "ocrEngine": {}
      }]
    }
  ]
})";

constexpr base::TimeDelta kThrottle = base::TimeDelta::FromSeconds(1);

// An image processor that holds and exposes the callbacks it is passed.
class TestImageProcessor : public mojom::ImageProcessor {
 public:
  TestImageProcessor() = default;

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

  DISALLOW_COPY_AND_ASSIGN(TestImageProcessor);
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

  // Expects that the earliest received request has the given URL, headers and
  // body, and replies with the given response.
  //
  // |expected_headers| is a map from header key string to either:
  //   a) a null optional, if the given header should not be present, or
  //   b) a non-null optional, if the given header should be present and match
  //      the optional value.
  //
  // Consumes the earliest received request (i.e. a subsequent call will apply
  // to the second-earliest received request and so on).
  void ExpectRequestAndSimulateResponse(
      const std::string& expected_url_suffix,
      const std::map<std::string, base::Optional<std::string>>&
          expected_headers,
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

    // Expect that specified headers are accurate.
    for (const auto& kv : expected_headers) {
      if (kv.second.has_value()) {
        std::string actual_value;
        EXPECT_THAT(request.headers.GetHeader(kv.first, &actual_value),
                    Eq(true));
        EXPECT_THAT(actual_value, Eq(*kv.second));
      } else {
        EXPECT_THAT(request.headers.HasHeader(kv.first), Eq(false));
      }
    }

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

  DISALLOW_COPY_AND_ASSIGN(TestServerURLLoaderFactory);
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
    CHECK_EQ(result->get_annotations().size(), 1u);
    CHECK_EQ(result->get_annotations()[0]->type, mojom::AnnotationType::kOcr);
    *ocr_text = std::move(result->get_annotations()[0]->text);
  }
}

}  // namespace

// Test that annotation works for one client, and that the cache is populated.
TEST(AnnotatorTest, SuccessAndCache) {
  base::test::ScopedTaskEnvironment test_task_env(
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  data_decoder::TestDataDecoderService test_dd_service;
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), std::string() /* api_key */, kThrottle,
      1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), test_dd_service.connector());
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
    const std::string request =
        ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID"));
    test_url_factory.ExpectRequestAndSimulateResponse(
        "annotation", {} /* expected_headers */, request, kSuccessResponse,
        net::HTTP_OK);
    test_task_env.RunUntilIdle();

    // HTTP response should have completed and callback should have been called.
    ASSERT_THAT(error, Eq(base::nullopt));
    EXPECT_THAT(ocr_text, Eq("Region 1\nRegion 2"));

    // Metrics should have been logged for the major actions of the service.
    histogram_tester.ExpectUniqueSample(metrics_internal::kCacheHit, false, 1);
    histogram_tester.ExpectUniqueSample(metrics_internal::kPixelFetchSuccess,
                                        true, 1);
    histogram_tester.ExpectUniqueSample(metrics_internal::kPixelFetchSuccess,
                                        true, 1);
    histogram_tester.ExpectUniqueSample(metrics_internal::kServerRequestSize,
                                        request.size() / 1024, 1);
    histogram_tester.ExpectUniqueSample(metrics_internal::kServerNetError,
                                        net::Error::OK, 1);
    histogram_tester.ExpectUniqueSample(
        metrics_internal::kServerHttpResponseCode, net::HTTP_OK, 1);
    histogram_tester.ExpectUniqueSample(metrics_internal::kServerResponseSize,
                                        std::strlen(kSuccessResponse), 1);
    histogram_tester.ExpectUniqueSample(
        base::StringPrintf(metrics_internal::kAnnotationStatus, "Ocr"),
        0 /* OK RPC status */, 1);
    histogram_tester.ExpectUniqueSample(
        base::StringPrintf(metrics_internal::kAnnotationConfidence, "Ocr"), 100,
        1);
    histogram_tester.ExpectUniqueSample(
        base::StringPrintf(metrics_internal::kAnnotationEmpty, "Ocr"), false,
        1);
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

    // Metrics should have been logged for a cache hit.
    EXPECT_THAT(histogram_tester.GetAllSamples(metrics_internal::kCacheHit),
                UnorderedElementsAre(Bucket(false, 1), Bucket(true, 1)));
  }
}

// Test that HTTP failure is gracefully handled.
TEST(AnnotatorTest, HttpError) {
  base::test::ScopedTaskEnvironment test_task_env(
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  data_decoder::TestDataDecoderService test_dd_service;
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), std::string() /* api_key */, kThrottle,
      1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), test_dd_service.connector());

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
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      "", net::HTTP_INTERNAL_SERVER_ERROR);
  test_task_env.RunUntilIdle();

  // HTTP response should have completed and callback should have been called.
  EXPECT_THAT(error, Eq(mojom::AnnotateImageError::kFailure));
  EXPECT_THAT(ocr_text, Eq(base::nullopt));

  // Metrics about the HTTP request failure should have been logged.
  histogram_tester.ExpectUniqueSample(metrics_internal::kServerNetError,
                                      net::Error::ERR_FAILED, 1);
  histogram_tester.ExpectUniqueSample(metrics_internal::kServerHttpResponseCode,
                                      net::HTTP_INTERNAL_SERVER_ERROR, 1);
  histogram_tester.ExpectUniqueSample(metrics_internal::kClientResult,
                                      ClientResult::kFailed, 1);
}

// Test that backend failure is gracefully handled.
TEST(AnnotatorTest, BackendError) {
  base::test::ScopedTaskEnvironment test_task_env(
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  data_decoder::TestDataDecoderService test_dd_service;
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), std::string() /* api_key */, kThrottle,
      1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), test_dd_service.connector());

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
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      kErrorResponse, net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // HTTP response should have completed and callback should have been called
  // with an error status.
  EXPECT_THAT(error, Eq(mojom::AnnotateImageError::kFailure));
  EXPECT_THAT(ocr_text, Eq(base::nullopt));

  // Metrics about the backend failure should have been logged.
  histogram_tester.ExpectUniqueSample(metrics_internal::kServerNetError,
                                      net::Error::OK, 1);
  histogram_tester.ExpectUniqueSample(metrics_internal::kServerHttpResponseCode,
                                      net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationStatus, "Ocr"),
      8 /* Failed RPC status */, 1);
  histogram_tester.ExpectUniqueSample(metrics_internal::kClientResult,
                                      ClientResult::kFailed, 1);
}

// Test that server failure (i.e. nonsense response) is gracefully handled.
TEST(AnnotatorTest, ServerError) {
  base::test::ScopedTaskEnvironment test_task_env(
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  data_decoder::TestDataDecoderService test_dd_service;
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), std::string() /* api_key */, kThrottle,
      1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), test_dd_service.connector());

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
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      "Hello, world!", net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // HTTP response should have completed and callback should have been called
  // with an error status.
  EXPECT_THAT(error, Eq(mojom::AnnotateImageError::kFailure));
  EXPECT_THAT(ocr_text, Eq(base::nullopt));

  // Metrics about the invalid response format should have been logged.
  histogram_tester.ExpectUniqueSample(metrics_internal::kServerNetError,
                                      net::Error::OK, 1);
  histogram_tester.ExpectUniqueSample(metrics_internal::kServerHttpResponseCode,
                                      net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(metrics_internal::kJsonParseSuccess,
                                      false, 1);
  histogram_tester.ExpectUniqueSample(metrics_internal::kClientResult,
                                      ClientResult::kFailed, 1);
}

// Test that work is reassigned if a processor fails.
TEST(AnnotatorTest, ProcessorFails) {
  base::test::ScopedTaskEnvironment test_task_env(
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  data_decoder::TestDataDecoderService test_dd_service;
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), std::string() /* api_key */, kThrottle,
      1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), test_dd_service.connector());

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
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      kSuccessResponse, net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // Annotator should have called all callbacks, but request 1 received an error
  // when we returned empty bytes.
  ASSERT_THAT(error, ElementsAre(mojom::AnnotateImageError::kFailure,
                                 base::nullopt, base::nullopt));
  EXPECT_THAT(ocr_text, ElementsAre(base::nullopt, "Region 1\nRegion 2",
                                    "Region 1\nRegion 2"));

  // Metrics about the pixel fetch failure should have been logged.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(metrics_internal::kPixelFetchSuccess),
      UnorderedElementsAre(Bucket(false, 1), Bucket(true, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(metrics_internal::kClientResult),
              UnorderedElementsAre(
                  Bucket(static_cast<int32_t>(ClientResult::kFailed), 1),
                  Bucket(static_cast<int32_t>(ClientResult::kSucceeded), 2)));
}

// Test that work is reassigned if processor dies.
TEST(AnnotatorTest, ProcessorDies) {
  base::test::ScopedTaskEnvironment test_task_env(
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  data_decoder::TestDataDecoderService test_dd_service;
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), std::string() /* api_key */, kThrottle,
      1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), test_dd_service.connector());

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
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      kSuccessResponse, net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // Annotator should have called all callbacks, but request 1 was canceled when
  // we reset processor 1.
  ASSERT_THAT(error, ElementsAre(mojom::AnnotateImageError::kCanceled,
                                 base::nullopt, base::nullopt));
  EXPECT_THAT(ocr_text, ElementsAre(base::nullopt, "Region 1\nRegion 2",
                                    "Region 1\nRegion 2"));

  // Metrics about the client cancelation should have been logged.
  EXPECT_THAT(histogram_tester.GetAllSamples(metrics_internal::kClientResult),
              UnorderedElementsAre(
                  Bucket(static_cast<int32_t>(ClientResult::kCanceled), 1),
                  Bucket(static_cast<int32_t>(ClientResult::kSucceeded), 2)));
}

// Test that multiple concurrent requests are handled in the same batch.
TEST(AnnotatorTest, ConcurrentSameBatch) {
  base::test::ScopedTaskEnvironment test_task_env(
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  data_decoder::TestDataDecoderService test_dd_service;
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), std::string() /* api_key */, kThrottle,
      3 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), test_dd_service.connector());

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
      "annotation", {} /* expected_headers */, ReformatJson(kBatchRequest),
      kBatchResponse, net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // Annotator should have called each callback with its corresponding text or
  // failure.
  ASSERT_THAT(error, ElementsAre(base::nullopt, base::nullopt,
                                 mojom::AnnotateImageError::kFailure));
  EXPECT_THAT(ocr_text, ElementsAre("1", "2", base::nullopt));

  // Metrics should have been logged for a single server response with multiple
  // results included.
  histogram_tester.ExpectUniqueSample(metrics_internal::kServerNetError,
                                      net::Error::OK, 1);
  histogram_tester.ExpectUniqueSample(metrics_internal::kServerHttpResponseCode,
                                      net::HTTP_OK, 1);
  EXPECT_THAT(histogram_tester.GetAllSamples(base::StringPrintf(
                  metrics_internal::kAnnotationStatus, "Ocr")),
              UnorderedElementsAre(Bucket(8 /* Failed RPC status */, 1),
                                   Bucket(0 /* OK RPC status */, 2)));
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationConfidence, "Ocr"), 100,
      2);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationEmpty, "Ocr"), false, 2);
  EXPECT_THAT(histogram_tester.GetAllSamples(metrics_internal::kClientResult),
              UnorderedElementsAre(
                  Bucket(static_cast<int32_t>(ClientResult::kFailed), 1),
                  Bucket(static_cast<int32_t>(ClientResult::kSucceeded), 2)));
}

// Test that multiple concurrent requests are handled in separate batches.
TEST(AnnotatorTest, ConcurrentSeparateBatches) {
  base::test::ScopedTaskEnvironment test_task_env(
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  data_decoder::TestDataDecoderService test_dd_service;
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), std::string() /* api_key */, kThrottle,
      3 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), test_dd_service.connector());

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
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      R"({
           "results": [{
             "imageId": "https://www.example.com/image1.jpg",
             "engineResults": [{
               "status": {},
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
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage2Url, "BAUG")),
      R"({
           "results": [{
             "imageId": "https://www.example.com/image2.jpg",
             "engineResults": [{
               "status": {},
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

  // Metrics should have been logged for two server responses.
  histogram_tester.ExpectUniqueSample(metrics_internal::kServerNetError,
                                      net::Error::OK, 2);
  histogram_tester.ExpectUniqueSample(metrics_internal::kServerHttpResponseCode,
                                      net::HTTP_OK, 2);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationStatus, "Ocr"),
      0 /* OK RPC status */, 2);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationConfidence, "Ocr"), 100,
      2);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(metrics_internal::kAnnotationEmpty, "Ocr"), false, 2);
  histogram_tester.ExpectUniqueSample(metrics_internal::kClientResult,
                                      ClientResult::kSucceeded, 2);
}

// Test that work is not duplicated if it is already ongoing.
TEST(AnnotatorTest, DuplicateWork) {
  base::test::ScopedTaskEnvironment test_task_env(
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME);
  TestServerURLLoaderFactory test_url_factory(
      "https://ia-pa.googleapis.com/v1/");
  data_decoder::TestDataDecoderService test_dd_service;
  base::HistogramTester histogram_tester;

  Annotator annotator(
      GURL(kTestServerUrl), std::string() /* api_key */, kThrottle,
      1 /* batch_size */, 1.0 /* min_ocr_confidence */,
      test_url_factory.AsSharedURLLoaderFactory(), test_dd_service.connector());

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
      "annotation", {} /* expected_headers */,
      ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
      kSuccessResponse, net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // Annotator should have called all callbacks with annotation results.
  ASSERT_THAT(error, ElementsAre(base::nullopt, base::nullopt, base::nullopt,
                                 base::nullopt));
  EXPECT_THAT(ocr_text,
              ElementsAre("Region 1\nRegion 2", "Region 1\nRegion 2",
                          "Region 1\nRegion 2", "Region 1\nRegion 2"));

  // Metrics should have been logged for a single pixel fetch.
  histogram_tester.ExpectUniqueSample(metrics_internal::kPixelFetchSuccess,
                                      true, 1);
}

// Test that the specified API key is sent, but only to Google-associated server
// domains.
TEST(AnnotatorTest, ApiKey) {
  base::test::ScopedTaskEnvironment test_task_env(
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME);
  data_decoder::TestDataDecoderService test_dd_service;

  // A call to a secure Google-owner server URL should include the specified API
  // key.
  {
    TestServerURLLoaderFactory test_url_factory(
        "https://ia-pa.googleapis.com/v1/");

    Annotator annotator(GURL(kTestServerUrl), "my_api_key", kThrottle,
                        1 /* batch_size */, 1.0 /* min_ocr_confidence */,
                        test_url_factory.AsSharedURLLoaderFactory(),
                        test_dd_service.connector());
    TestImageProcessor processor;

    annotator.AnnotateImage(kImage1Url, processor.GetPtr(), base::DoNothing());
    test_task_env.RunUntilIdle();

    // Annotator should have asked processor for pixels.
    ASSERT_THAT(processor.callbacks(), SizeIs(1));

    // Send back image data.
    std::move(processor.callbacks()[0]).Run({1, 2, 3});
    processor.callbacks().pop_back();
    test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));
    test_task_env.RunUntilIdle();

    // HTTP request should have been made with the API key included.
    test_url_factory.ExpectRequestAndSimulateResponse(
        "annotation", {{Annotator::kGoogApiKeyHeader, "my_api_key"}},
        ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
        kSuccessResponse, net::HTTP_OK);
  }

  // A call to a Google-owned server URL should not include the API key if the
  // requests are made insecurely.
  {
    // Note: not HTTPS.
    TestServerURLLoaderFactory test_url_factory(
        "http://ia-pa.googleapis.com/v1/");

    Annotator annotator(GURL("http://ia-pa.googleapis.com/v1/annotation"),
                        "my_api_key", kThrottle, 1 /* batch_size */,
                        1.0 /* min_ocr_confidence */,
                        test_url_factory.AsSharedURLLoaderFactory(),
                        test_dd_service.connector());
    TestImageProcessor processor;

    annotator.AnnotateImage(kImage1Url, processor.GetPtr(), base::DoNothing());
    test_task_env.RunUntilIdle();

    // Annotator should have asked processor for pixels.
    ASSERT_THAT(processor.callbacks(), SizeIs(1));

    // Send back image data.
    std::move(processor.callbacks()[0]).Run({1, 2, 3});
    processor.callbacks().pop_back();
    test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));
    test_task_env.RunUntilIdle();

    // HTTP request should have been made without the API key included.
    test_url_factory.ExpectRequestAndSimulateResponse(
        "annotation", {{Annotator::kGoogApiKeyHeader, base::nullopt}},
        ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
        kSuccessResponse, net::HTTP_OK);
  }

  // A call to a non-Google-owned URL should not include the API key.
  {
    TestServerURLLoaderFactory test_url_factory("https://datascraper.com/");

    Annotator annotator(GURL("https://datascraper.com/annotation"),
                        "my_api_key", kThrottle, 1 /* batch_size */,
                        1.0 /* min_ocr_confidence */,
                        test_url_factory.AsSharedURLLoaderFactory(),
                        test_dd_service.connector());
    TestImageProcessor processor;

    annotator.AnnotateImage(kImage1Url, processor.GetPtr(), base::DoNothing());
    test_task_env.RunUntilIdle();

    // Annotator should have asked processor for pixels.
    ASSERT_THAT(processor.callbacks(), SizeIs(1));

    // Send back image data.
    std::move(processor.callbacks()[0]).Run({1, 2, 3});
    processor.callbacks().pop_back();
    test_task_env.FastForwardBy(base::TimeDelta::FromSeconds(1));
    test_task_env.RunUntilIdle();

    // HTTP request should have been made without the API key included.
    test_url_factory.ExpectRequestAndSimulateResponse(
        "annotation", {{Annotator::kGoogApiKeyHeader, base::nullopt}},
        ReformatJson(base::StringPrintf(kTemplateRequest, kImage1Url, "AQID")),
        kSuccessResponse, net::HTTP_OK);
  }
}

// TODO(crbug.com/916420): add unit tests for description annotations.

}  // namespace image_annotation
