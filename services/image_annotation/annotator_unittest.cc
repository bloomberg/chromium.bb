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

// Example server requests / responses.

constexpr char kRequestTemplate[] = R"(
{
  "image_requests": [
    {
      "image_id": "%s",
      "image_bytes": "%s"
    }
  ],

  "feature_request": {
    "ocr_feature": {}
  }
}
)";

constexpr char kSuccessResponseTemplate[] = R"(
  {
    "results": [
      {
        "image_id": "%s",
        "annotations": {
          "ocr": [%s]
        }
      }
    ]
  }
)";

constexpr char kErrorResponseTemplate[] = R"(
  {
    "results": [
      {
        "image_id": "%s",
        "status": {
          "code": 8,
          "message": "Resource exhaused"
        }
      }
    ]
  }
)";

// Example image URLs.

constexpr char kImage1Url[] = "https://www.example.com/image1.jpg";
constexpr char kImage2Url[] = "https://www.example.com/image2.jpg";

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
  const std::unique_ptr<base::Value> json = base::JSONReader::Read(in);
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
  base::test::ScopedTaskEnvironment test_task_env;
  TestServerURLLoaderFactory test_url_factory("https://test_ia_server.com/v1:");

  Annotator annotator(GURL(kTestServerUrl),
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

    // HTTP request should have been made.
    test_url_factory.ExpectRequestAndSimulateResponse(
        "ocr",
        ReformatJson(base::StringPrintf(kRequestTemplate, kImage1Url, "AQID")),
        base::StringPrintf(kSuccessResponseTemplate, kImage1Url,
                           R"({
                                "detected_text": "Text 1",
                                "confidence_score": 1.0
                              },
                              {
                                "detected_text": "Text 2",
                                "confidence_score": 1.0
                              })"),
        net::HTTP_OK);
    test_task_env.RunUntilIdle();

    // HTTP response should have completed and callback should have been called.
    ASSERT_THAT(error, Eq(base::nullopt));
    EXPECT_THAT(ocr_text, Eq("Text 1\nText 2"));
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
    EXPECT_THAT(ocr_text, Eq("Text 1\nText 2"));
  }
}

// Test that HTTP failure is gracefully handled.
TEST(AnnotatorTest, HttpError) {
  base::test::ScopedTaskEnvironment test_task_env;
  TestServerURLLoaderFactory test_url_factory("https://test_ia_server.com/v1:");

  Annotator annotator(GURL(kTestServerUrl),
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

  // HTTP request should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "ocr",
      ReformatJson(base::StringPrintf(kRequestTemplate, kImage1Url, "AQID")),
      "", net::HTTP_INTERNAL_SERVER_ERROR);
  test_task_env.RunUntilIdle();

  // HTTP response should have completed and callback should have been called.
  EXPECT_THAT(error, Eq(mojom::AnnotateImageError::kFailure));
  EXPECT_THAT(ocr_text, Eq(base::nullopt));
}

// Test that backend failure is gracefully handled.
TEST(AnnotatorTest, BackendError) {
  base::test::ScopedTaskEnvironment test_task_env;
  TestServerURLLoaderFactory test_url_factory("https://test_ia_server.com/v1:");

  Annotator annotator(GURL(kTestServerUrl),
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

  // HTTP request should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "ocr",
      ReformatJson(base::StringPrintf(kRequestTemplate, kImage1Url, "AQID")),
      base::StringPrintf(kErrorResponseTemplate, kImage1Url), net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // HTTP response should have completed and callback should have been called
  // with an error status.
  EXPECT_THAT(error, Eq(mojom::AnnotateImageError::kFailure));
  EXPECT_THAT(ocr_text, Eq(base::nullopt));
}

// Test that server failure (i.e. nonsense response) is gracefully handled.
TEST(AnnotatorTest, ServerError) {
  base::test::ScopedTaskEnvironment test_task_env;
  TestServerURLLoaderFactory test_url_factory("https://test_ia_server.com/v1:");

  Annotator annotator(GURL(kTestServerUrl),
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

  // HTTP request should have been made; respond with nonsense string.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "ocr",
      ReformatJson(base::StringPrintf(kRequestTemplate, kImage1Url, "AQID")),
      "Hello, world!", net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // HTTP response should have completed and callback should have been called
  // with an error status.
  EXPECT_THAT(error, Eq(mojom::AnnotateImageError::kFailure));
  EXPECT_THAT(ocr_text, Eq(base::nullopt));
}

// Test that work is reassigned if a processor fails.
TEST(AnnotatorTest, ProcessorFails) {
  base::test::ScopedTaskEnvironment test_task_env;
  TestServerURLLoaderFactory test_url_factory("https://test_ia_server.com/v1:");

  Annotator annotator(GURL(kTestServerUrl),
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

  // HTTP request for image 1 should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "ocr",
      ReformatJson(base::StringPrintf(kRequestTemplate, kImage1Url, "AQID")),
      base::StringPrintf(kSuccessResponseTemplate, kImage1Url,
                         R"({
                              "detected_text": "Some text",
                              "confidence_score": 1.0
                            })"),
      net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // Annotator should have called all callbacks, but request 1 received an error
  // when we returned empty bytes.
  ASSERT_THAT(error, ElementsAre(mojom::AnnotateImageError::kFailure,
                                 base::nullopt, base::nullopt));
  EXPECT_THAT(ocr_text, ElementsAre(base::nullopt, "Some text", "Some text"));
}

// Test that work is reassigned if processor dies.
TEST(AnnotatorTest, ProcessorDies) {
  base::test::ScopedTaskEnvironment test_task_env;
  TestServerURLLoaderFactory test_url_factory("https://test_ia_server.com/v1:");

  Annotator annotator(GURL(kTestServerUrl),
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

  // HTTP request for image 1 should have been made.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "ocr",
      ReformatJson(base::StringPrintf(kRequestTemplate, kImage1Url, "AQID")),
      base::StringPrintf(kSuccessResponseTemplate, kImage1Url,
                         R"({
                              "detected_text": "Some text",
                              "confidence_score": 1.0
                            })"),
      net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // Annotator should have called all callbacks, but request 1 was canceled when
  // we reset processor 1.
  ASSERT_THAT(error, ElementsAre(mojom::AnnotateImageError::kCanceled,
                                 base::nullopt, base::nullopt));
  EXPECT_THAT(ocr_text, ElementsAre(base::nullopt, "Some text", "Some text"));
}

// Test that multiple concurrent requests are handled.
TEST(AnnotatorTest, Concurrent) {
  base::test::ScopedTaskEnvironment test_task_env;
  TestServerURLLoaderFactory test_url_factory("https://test_ia_server.com/v1:");

  Annotator annotator(GURL(kTestServerUrl),
                      test_url_factory.AsSharedURLLoaderFactory());

  TestImageProcessor processor[2];
  base::Optional<mojom::AnnotateImageError> error[2];
  base::Optional<std::string> ocr_text[2];

  // Requests for images 1 and 2.
  annotator.AnnotateImage(
      kImage1Url, processor[0].GetPtr(),
      base::BindOnce(&ReportResult, &error[0], &ocr_text[0]));
  annotator.AnnotateImage(
      kImage2Url, processor[1].GetPtr(),
      base::BindOnce(&ReportResult, &error[1], &ocr_text[1]));
  test_task_env.RunUntilIdle();

  // Annotator should have asked processor 1 for image 1's pixels and processor
  // 2 for image 2's pixels.
  ASSERT_THAT(processor[0].callbacks(), SizeIs(1));
  ASSERT_THAT(processor[1].callbacks(), SizeIs(1));

  // Send back image data.
  std::move(processor[0].callbacks()[0]).Run({1, 2, 3});
  processor[0].callbacks().pop_back();
  std::move(processor[1].callbacks()[0]).Run({4, 5, 6});
  processor[1].callbacks().pop_back();
  test_task_env.RunUntilIdle();

  // HTTP request for image 1 should have been made first, then request for
  // image 2.
  test_url_factory.ExpectRequestAndSimulateResponse(
      "ocr",
      ReformatJson(base::StringPrintf(kRequestTemplate, kImage1Url, "AQID")),
      base::StringPrintf(kSuccessResponseTemplate, kImage1Url,
                         R"({
                              "detected_text": "Text 1",
                              "confidence_score": 1.0
                            })"),
      net::HTTP_OK);
  test_url_factory.ExpectRequestAndSimulateResponse(
      "ocr",
      ReformatJson(base::StringPrintf(kRequestTemplate, kImage2Url, "BAUG")),
      base::StringPrintf(kSuccessResponseTemplate, kImage1Url,
                         R"({
                              "detected_text": "Text 2",
                              "confidence_score": 1.0
                            })"),
      net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // Annotator should have called each callback with its corresponding text.
  ASSERT_THAT(error, ElementsAre(base::nullopt, base::nullopt));
  EXPECT_THAT(ocr_text, ElementsAre("Text 1", "Text 2"));
}

// Test that work is not duplicated if it is already ongoing.
TEST(AnnotatorTest, DuplicateWork) {
  base::test::ScopedTaskEnvironment test_task_env;
  TestServerURLLoaderFactory test_url_factory("https://test_ia_server.com/v1:");

  Annotator annotator(GURL(kTestServerUrl),
                      test_url_factory.AsSharedURLLoaderFactory());

  TestImageProcessor processor[3];
  base::Optional<mojom::AnnotateImageError> error[3];
  base::Optional<std::string> ocr_text[3];

  // First request annotation of image 1 with processor 1.
  annotator.AnnotateImage(
      kImage1Url, processor[0].GetPtr(),
      base::BindOnce(&ReportResult, &error[0], &ocr_text[0]));
  test_task_env.RunUntilIdle();

  // Annotator should have asked processor 1 for image 1's pixels.
  ASSERT_THAT(processor[0].callbacks(), SizeIs(1));
  ASSERT_THAT(processor[1].callbacks(), IsEmpty());
  ASSERT_THAT(processor[2].callbacks(), IsEmpty());

  // Now request annotation of image 1 with processor 2.
  annotator.AnnotateImage(
      kImage1Url, processor[1].GetPtr(),
      base::BindOnce(&ReportResult, &error[1], &ocr_text[1]));
  test_task_env.RunUntilIdle();

  // Annotator *should not* have asked processor 2 for image 1's pixels (since
  // processor 1 is already handling that).
  ASSERT_THAT(processor[0].callbacks(), SizeIs(1));
  ASSERT_THAT(processor[1].callbacks(), IsEmpty());
  ASSERT_THAT(processor[2].callbacks(), IsEmpty());

  // Get processor 1 to reply with bytes for image 1.
  std::move(processor[0].callbacks()[0]).Run({1, 2, 3});
  processor[0].callbacks().pop_back();
  test_task_env.RunUntilIdle();

  // Now request annotation of image 1 with processor 3.
  annotator.AnnotateImage(
      kImage1Url, processor[2].GetPtr(),
      base::BindOnce(&ReportResult, &error[2], &ocr_text[2]));
  test_task_env.RunUntilIdle();

  // Annotator *should not* have asked processor 3 for image 1's pixels (since
  // it has already sent image 1's pixels to the server).
  ASSERT_THAT(processor[0].callbacks(), IsEmpty());
  ASSERT_THAT(processor[1].callbacks(), IsEmpty());
  ASSERT_THAT(processor[2].callbacks(), IsEmpty());

  // HTTP request for image 1 should have been made (with bytes obtained from
  // processor 1).
  test_url_factory.ExpectRequestAndSimulateResponse(
      "ocr",
      ReformatJson(base::StringPrintf(kRequestTemplate, kImage1Url, "AQID")),
      base::StringPrintf(kSuccessResponseTemplate, kImage1Url,
                         R"({
                              "detected_text": "Some text",
                              "confidence_score": 1.0
                            })"),
      net::HTTP_OK);
  test_task_env.RunUntilIdle();

  // Annotator should have called all callbacks with annotation results.
  ASSERT_THAT(error, ElementsAre(base::nullopt, base::nullopt, base::nullopt));
  EXPECT_THAT(ocr_text, ElementsAre("Some text", "Some text", "Some text"));
}

}  // namespace image_annotation
