// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_single_script_update_checker.h"

#include <vector>
#include "base/bind.h"
#include "base/containers/queue.h"
#include "base/run_loop.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/load_flags.h"
#include "net/http/http_util.h"
#include "services/network/test/test_url_loader_factory.h"

namespace content {
namespace {

constexpr char kScriptURL[] = "https://example.com/script.js";
constexpr char kImportedScriptURL[] = "https://example.com/imported-script.js";
constexpr char kSuccessHeader[] =
    "HTTP/1.1 200 OK\n"
    "Content-Type: text/javascript\n\n";

class ServiceWorkerSingleScriptUpdateCheckerTest : public testing::Test {
 public:
  struct CheckResult {
    CheckResult(
        const GURL& script_url,
        ServiceWorkerSingleScriptUpdateChecker::Result compare_result,
        std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::PausedState>
            paused_state)
        : url(script_url),
          result(compare_result),
          paused_state(std::move(paused_state)) {}

    CheckResult(CheckResult&& ref) = default;

    CheckResult& operator=(CheckResult&& ref) = default;

    ~CheckResult() = default;

    GURL url;
    ServiceWorkerSingleScriptUpdateChecker::Result result;
    std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::PausedState>
        paused_state;
  };

  ServiceWorkerSingleScriptUpdateCheckerTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}
  ~ServiceWorkerSingleScriptUpdateCheckerTest() override = default;

  ServiceWorkerStorage* storage() { return helper_->context()->storage(); }

  void SetUp() override {
    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath());
    base::RunLoop run_loop;
    storage()->LazyInitializeForTest(run_loop.QuitClosure());
    run_loop.Run();
  }

  size_t TotalBytes(const std::vector<std::string>& data_chunks) {
    size_t bytes = 0;
    for (const auto& data : data_chunks)
      bytes += data.size();
    return bytes;
  }

  // Create an update checker which will always ask HTTP cache validation.
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker>
  CreateSingleScriptUpdateCheckerWithoutHttpCache(
      const char* url,
      std::unique_ptr<ServiceWorkerResponseReader> compare_reader,
      std::unique_ptr<ServiceWorkerResponseReader> copy_reader,
      std::unique_ptr<ServiceWorkerResponseWriter> writer,
      network::TestURLLoaderFactory* loader_factory,
      base::Optional<CheckResult>* out_check_result) {
    return CreateSingleScriptUpdateChecker(
        url, true /* is_main_script */, false /* force_bypass_cache */,
        blink::mojom::ServiceWorkerUpdateViaCache::kNone,
        base::TimeDelta() /* time_since_last_check */,
        std::move(compare_reader), std::move(copy_reader), std::move(writer),
        loader_factory, out_check_result);
  }

  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker>
  CreateSingleScriptUpdateChecker(
      const char* url,
      bool is_main_script,
      bool force_bypass_cache,
      blink::mojom::ServiceWorkerUpdateViaCache update_via_cache,
      base::TimeDelta time_since_last_check,
      std::unique_ptr<ServiceWorkerResponseReader> compare_reader,
      std::unique_ptr<ServiceWorkerResponseReader> copy_reader,
      std::unique_ptr<ServiceWorkerResponseWriter> writer,
      network::TestURLLoaderFactory* loader_factory,
      base::Optional<CheckResult>* out_check_result) {
    helper_->SetNetworkFactory(loader_factory);
    return std::make_unique<ServiceWorkerSingleScriptUpdateChecker>(
        GURL(url), is_main_script, force_bypass_cache, update_via_cache,
        time_since_last_check,
        helper_->url_loader_factory_getter()->GetNetworkFactory(),
        std::move(compare_reader), std::move(copy_reader), std::move(writer),
        base::BindOnce(
            [](base::Optional<CheckResult>* out_check_result_param,
               const GURL& script_url,
               ServiceWorkerSingleScriptUpdateChecker::Result result,
               std::unique_ptr<
                   ServiceWorkerSingleScriptUpdateChecker::PausedState>
                   paused_state) {
              *out_check_result_param =
                  CheckResult(script_url, result, std::move(paused_state));
            },
            out_check_result));
  }

  std::unique_ptr<network::TestURLLoaderFactory> CreateLoaderFactoryWithRespone(
      const GURL& url,
      std::string header,
      std::string body,
      net::Error error) {
    auto loader_factory = std::make_unique<network::TestURLLoaderFactory>();
    network::ResourceResponseHead head;
    head.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(header));
    network::URLLoaderCompletionStatus status(error);
    status.decoded_body_length = body.size();
    loader_factory->AddResponse(url, head, body, status);
    return loader_factory;
  }

 protected:
  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerSingleScriptUpdateCheckerTest);
};

TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, Identical_SingleSyncRead) {
  // Response body from the network.
  const std::string body_from_net("abcdef");

  // Stored data for |kScriptURL|.
  const std::vector<std::string> body_from_storage{body_from_net};

  std::unique_ptr<network::TestURLLoaderFactory> loader_factory =
      CreateLoaderFactoryWithRespone(GURL(kScriptURL), kSuccessHeader,
                                     body_from_net, net::OK);

  auto compare_reader = std::make_unique<MockServiceWorkerResponseReader>();
  auto copy_reader = std::make_unique<MockServiceWorkerResponseReader>();
  auto writer = std::make_unique<MockServiceWorkerResponseWriter>();
  MockServiceWorkerResponseReader* compare_reader_rawptr = compare_reader.get();
  compare_reader->ExpectReadOk(body_from_storage, TotalBytes(body_from_storage),
                               false /* async */);

  base::Optional<CheckResult> check_result;
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateCheckerWithoutHttpCache(
          kScriptURL, std::move(compare_reader), std::move(copy_reader),
          std::move(writer), loader_factory.get(), &check_result);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(check_result.has_value());
  EXPECT_EQ(check_result.value().result,
            ServiceWorkerSingleScriptUpdateChecker::Result::kIdentical);
  EXPECT_EQ(check_result.value().url, kScriptURL);
  EXPECT_TRUE(compare_reader_rawptr->AllExpectedReadsDone());
}

TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, Different_SingleSyncRead) {
  // Response body from the network.
  const std::string body_from_net("abcdef");

  // Stored data for |kScriptURL|.
  const std::vector<std::string> body_from_storage{"abxx"};

  std::unique_ptr<network::TestURLLoaderFactory> loader_factory =
      CreateLoaderFactoryWithRespone(GURL(kScriptURL), kSuccessHeader,
                                     body_from_net, net::OK);

  auto compare_reader = std::make_unique<MockServiceWorkerResponseReader>();
  auto copy_reader = std::make_unique<MockServiceWorkerResponseReader>();
  auto writer = std::make_unique<MockServiceWorkerResponseWriter>();
  MockServiceWorkerResponseReader* compare_reader_rawptr = compare_reader.get();
  compare_reader->ExpectReadOk(body_from_storage, TotalBytes(body_from_storage),
                               false /* async */);

  base::Optional<CheckResult> check_result;
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateCheckerWithoutHttpCache(
          kScriptURL, std::move(compare_reader), std::move(copy_reader),
          std::move(writer), loader_factory.get(), &check_result);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(check_result.has_value());
  EXPECT_EQ(check_result.value().result,
            ServiceWorkerSingleScriptUpdateChecker::Result::kDifferent);
  EXPECT_EQ(check_result.value().url, kScriptURL);
  EXPECT_TRUE(compare_reader_rawptr->AllExpectedReadsDone());
}

TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, Different_MultipleSyncRead) {
  // Response body from the network.
  const std::string body_from_net("abcdef");

  // Stored data for |kScriptURL| (the data for compare reader).
  // The comparison should stop in the second block of data.
  const std::vector<std::string> body_from_storage{"ab", "cx"};

  std::unique_ptr<network::TestURLLoaderFactory> loader_factory =
      CreateLoaderFactoryWithRespone(GURL(kScriptURL), kSuccessHeader,
                                     body_from_net, net::OK);

  auto compare_reader = std::make_unique<MockServiceWorkerResponseReader>();
  auto copy_reader = std::make_unique<MockServiceWorkerResponseReader>();
  auto writer = std::make_unique<MockServiceWorkerResponseWriter>();
  MockServiceWorkerResponseReader* compare_reader_rawptr = compare_reader.get();
  compare_reader->ExpectReadOk(body_from_storage, TotalBytes(body_from_storage),
                               false /* async */);

  base::Optional<CheckResult> check_result;
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateCheckerWithoutHttpCache(
          kScriptURL, std::move(compare_reader), std::move(copy_reader),
          std::move(writer), loader_factory.get(), &check_result);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(check_result.has_value());
  EXPECT_EQ(check_result.value().result,
            ServiceWorkerSingleScriptUpdateChecker::Result::kDifferent);
  EXPECT_EQ(check_result.value().url, kScriptURL);
  EXPECT_TRUE(compare_reader_rawptr->AllExpectedReadsDone());
}

TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, NetworkDataLong_SyncRead) {
  // Response body from the network.
  const std::string body_from_net("abcdef");

  // Stored data for |kScriptURL| (the data for compare reader).
  const std::vector<std::string> body_from_storage{"ab", "cd", ""};

  std::unique_ptr<network::TestURLLoaderFactory> loader_factory =
      CreateLoaderFactoryWithRespone(GURL(kScriptURL), kSuccessHeader,
                                     body_from_net, net::OK);

  auto compare_reader = std::make_unique<MockServiceWorkerResponseReader>();
  auto copy_reader = std::make_unique<MockServiceWorkerResponseReader>();
  auto writer = std::make_unique<MockServiceWorkerResponseWriter>();
  MockServiceWorkerResponseReader* compare_reader_rawptr = compare_reader.get();
  compare_reader->ExpectReadOk(body_from_storage, TotalBytes(body_from_storage),
                               false /* async */);

  base::Optional<CheckResult> check_result;
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateCheckerWithoutHttpCache(
          kScriptURL, std::move(compare_reader), std::move(copy_reader),
          std::move(writer), loader_factory.get(), &check_result);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(check_result.has_value());
  EXPECT_EQ(check_result.value().result,
            ServiceWorkerSingleScriptUpdateChecker::Result::kDifferent);
  EXPECT_EQ(check_result.value().url, kScriptURL);
  EXPECT_TRUE(compare_reader_rawptr->AllExpectedReadsDone());
}

TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, NetworkDataShort_SyncRead) {
  // Response body from the network.
  const std::string body_from_net("abcdef");

  // Stored data for |kScriptURL| (the data for compare reader).
  const std::vector<std::string> body_in_storage{"ab", "cd", "ef", "gh"};

  // Stored data that will actually be read from the compare reader.
  // The last 2 bytes of |body_in_storage| won't be read.
  const std::vector<std::string> body_read_from_storage{"ab", "cd", "ef"};

  std::unique_ptr<network::TestURLLoaderFactory> loader_factory =
      CreateLoaderFactoryWithRespone(GURL(kScriptURL), kSuccessHeader,
                                     body_from_net, net::OK);

  auto compare_reader = std::make_unique<MockServiceWorkerResponseReader>();
  auto copy_reader = std::make_unique<MockServiceWorkerResponseReader>();
  auto writer = std::make_unique<MockServiceWorkerResponseWriter>();
  MockServiceWorkerResponseReader* compare_reader_rawptr = compare_reader.get();
  compare_reader->ExpectReadOk(body_read_from_storage,
                               TotalBytes(body_in_storage), false /* async */);

  base::Optional<CheckResult> check_result;
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateCheckerWithoutHttpCache(
          kScriptURL, std::move(compare_reader), std::move(copy_reader),
          std::move(writer), loader_factory.get(), &check_result);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(check_result.has_value());
  EXPECT_EQ(check_result.value().result,
            ServiceWorkerSingleScriptUpdateChecker::Result::kDifferent);
  EXPECT_EQ(check_result.value().url, kScriptURL);
  EXPECT_TRUE(compare_reader_rawptr->AllExpectedReadsDone());
}

TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, Identical_SingleAsyncRead) {
  // Response body from the network.
  const std::string body_from_net("abcdef");

  // Stored data for |kScriptURL| (the data for compare reader).
  const std::vector<std::string> body_from_storage{body_from_net};

  std::unique_ptr<network::TestURLLoaderFactory> loader_factory =
      CreateLoaderFactoryWithRespone(GURL(kScriptURL), kSuccessHeader,
                                     body_from_net, net::OK);

  auto compare_reader = std::make_unique<MockServiceWorkerResponseReader>();
  auto copy_reader = std::make_unique<MockServiceWorkerResponseReader>();
  auto writer = std::make_unique<MockServiceWorkerResponseWriter>();
  MockServiceWorkerResponseReader* compare_reader_rawptr = compare_reader.get();
  compare_reader->ExpectReadOk(body_from_storage, TotalBytes(body_from_storage),
                               true /* async */);

  base::Optional<CheckResult> check_result;
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateCheckerWithoutHttpCache(
          kScriptURL, std::move(compare_reader), std::move(copy_reader),
          std::move(writer), loader_factory.get(), &check_result);

  // Update check stops in WriteHeader() due to the asynchronous read of the
  // |compare_reader|.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Continue the update check and trigger OnWriteHeadersComplete(). The resumed
  // update check stops again at CompareData().
  compare_reader_rawptr->CompletePendingRead();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_result.has_value());

  // Continue the update check and trigger OnCompareDataComplete(). This will
  // finish the entire update check.
  compare_reader_rawptr->CompletePendingRead();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(check_result.has_value());
  EXPECT_EQ(check_result.value().result,
            ServiceWorkerSingleScriptUpdateChecker::Result::kIdentical);
  EXPECT_EQ(check_result.value().url, kScriptURL);
  EXPECT_FALSE(check_result.value().paused_state);
  EXPECT_TRUE(compare_reader_rawptr->AllExpectedReadsDone());
}

// Tests cache validation behavior when updateViaCache is 'all'.
TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, UpdateViaCache_All) {
  auto loader_factory = std::make_unique<network::TestURLLoaderFactory>();
  base::Optional<CheckResult> check_result;

  // Load the main script. Should not validate the cache.
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateChecker(
          kScriptURL, true /* is_main_script */, false /* force_bypass_cache */,
          blink::mojom::ServiceWorkerUpdateViaCache::kAll, base::TimeDelta(),
          std::make_unique<MockServiceWorkerResponseReader>(),
          std::make_unique<MockServiceWorkerResponseReader>(),
          std::make_unique<MockServiceWorkerResponseWriter>(),
          loader_factory.get(), &check_result);

  const network::ResourceRequest* request = nullptr;
  ASSERT_TRUE(loader_factory->IsPending(kScriptURL, &request));
  EXPECT_FALSE(request->load_flags & net::LOAD_VALIDATE_CACHE);

  // Load imported script. Should not validate the cache.
  checker = CreateSingleScriptUpdateChecker(
      kImportedScriptURL, false /* is_main_script */,
      false /* force_bypass_cache */,
      blink::mojom::ServiceWorkerUpdateViaCache::kAll, base::TimeDelta(),
      std::make_unique<MockServiceWorkerResponseReader>(),
      std::make_unique<MockServiceWorkerResponseReader>(),
      std::make_unique<MockServiceWorkerResponseWriter>(), loader_factory.get(),
      &check_result);

  ASSERT_TRUE(loader_factory->IsPending(kImportedScriptURL, &request));
  EXPECT_FALSE(request->load_flags & net::LOAD_VALIDATE_CACHE);
}

// Tests cache validation behavior when updateViaCache is 'none'.
TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, UpdateViaCache_None) {
  auto loader_factory = std::make_unique<network::TestURLLoaderFactory>();
  base::Optional<CheckResult> check_result;

  // Load the main script. Should validate the cache.
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateChecker(
          kScriptURL, true /* is_main_script */, false /* force_bypass_cache */,
          blink::mojom::ServiceWorkerUpdateViaCache::kNone, base::TimeDelta(),
          std::make_unique<MockServiceWorkerResponseReader>(),
          std::make_unique<MockServiceWorkerResponseReader>(),
          std::make_unique<MockServiceWorkerResponseWriter>(),
          loader_factory.get(), &check_result);

  const network::ResourceRequest* request = nullptr;
  ASSERT_TRUE(loader_factory->IsPending(kScriptURL, &request));
  EXPECT_TRUE(request->load_flags & net::LOAD_VALIDATE_CACHE);

  // Load imported script. Should validate the cache.
  checker = CreateSingleScriptUpdateChecker(
      kImportedScriptURL, false /* is_main_script */,
      false /* force_bypass_cache */,
      blink::mojom::ServiceWorkerUpdateViaCache::kNone, base::TimeDelta(),
      std::make_unique<MockServiceWorkerResponseReader>(),
      std::make_unique<MockServiceWorkerResponseReader>(),
      std::make_unique<MockServiceWorkerResponseWriter>(), loader_factory.get(),
      &check_result);

  ASSERT_TRUE(loader_factory->IsPending(kImportedScriptURL, &request));
  EXPECT_TRUE(request->load_flags & net::LOAD_VALIDATE_CACHE);
}

// Tests cache validation behavior when updateViaCache is 'imports'.
TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, UpdateViaCache_Imports) {
  auto loader_factory = std::make_unique<network::TestURLLoaderFactory>();
  base::Optional<CheckResult> check_result;

  // Load main script. Should validate the cache.
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateChecker(
          kScriptURL, true /* is_main_script */, false /* force_bypass_cache */,
          blink::mojom::ServiceWorkerUpdateViaCache::kImports,
          base::TimeDelta(),
          std::make_unique<MockServiceWorkerResponseReader>(),
          std::make_unique<MockServiceWorkerResponseReader>(),
          std::make_unique<MockServiceWorkerResponseWriter>(),
          loader_factory.get(), &check_result);

  const network::ResourceRequest* request = nullptr;
  ASSERT_TRUE(loader_factory->IsPending(kScriptURL, &request));
  EXPECT_TRUE(request->load_flags & net::LOAD_VALIDATE_CACHE);

  // Load imported script. Should not validate the cache.
  checker = CreateSingleScriptUpdateChecker(
      kImportedScriptURL, false /* is_main_script */,
      false /* force_bypass_cache */,
      blink::mojom::ServiceWorkerUpdateViaCache::kImports, base::TimeDelta(),
      std::make_unique<MockServiceWorkerResponseReader>(),
      std::make_unique<MockServiceWorkerResponseReader>(),
      std::make_unique<MockServiceWorkerResponseWriter>(), loader_factory.get(),
      &check_result);

  ASSERT_TRUE(loader_factory->IsPending(kImportedScriptURL, &request));
  EXPECT_FALSE(request->load_flags & net::LOAD_VALIDATE_CACHE);
}

// Tests cache validation behavior when version's
// |force_bypass_cache_for_scripts_| is true.
TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, ForceBypassCache) {
  auto loader_factory = std::make_unique<network::TestURLLoaderFactory>();
  base::Optional<CheckResult> check_result;

  // Load main script. Should validate the cache.
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateChecker(
          kScriptURL, true /* is_main_script */, true /* force_bypass_cache */,
          blink::mojom::ServiceWorkerUpdateViaCache::kAll, base::TimeDelta(),
          std::make_unique<MockServiceWorkerResponseReader>(),
          std::make_unique<MockServiceWorkerResponseReader>(),
          std::make_unique<MockServiceWorkerResponseWriter>(),
          loader_factory.get(), &check_result);

  const network::ResourceRequest* request = nullptr;
  ASSERT_TRUE(loader_factory->IsPending(kScriptURL, &request));
  EXPECT_TRUE(request->load_flags & net::LOAD_VALIDATE_CACHE);

  // Load imported script. Should validate the cache.
  checker = CreateSingleScriptUpdateChecker(
      kImportedScriptURL, false /* is_main_script */,
      true /* force_bypass_cache */,
      blink::mojom::ServiceWorkerUpdateViaCache::kAll, base::TimeDelta(),
      std::make_unique<MockServiceWorkerResponseReader>(),
      std::make_unique<MockServiceWorkerResponseReader>(),
      std::make_unique<MockServiceWorkerResponseWriter>(), loader_factory.get(),
      &check_result);

  ASSERT_TRUE(loader_factory->IsPending(kImportedScriptURL, &request));
  EXPECT_TRUE(request->load_flags & net::LOAD_VALIDATE_CACHE);
}

// Tests cache validation behavior when more than 24 hours passed.
TEST_F(ServiceWorkerSingleScriptUpdateCheckerTest, MoreThan24Hours) {
  auto loader_factory = std::make_unique<network::TestURLLoaderFactory>();
  base::Optional<CheckResult> check_result;

  // Load main script. Should validate the cache.
  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> checker =
      CreateSingleScriptUpdateChecker(
          kScriptURL, true /* is_main_script */, false /* force_bypass_cache */,
          blink::mojom::ServiceWorkerUpdateViaCache::kAll,
          base::TimeDelta::FromDays(1) + base::TimeDelta::FromHours(1),
          std::make_unique<MockServiceWorkerResponseReader>(),
          std::make_unique<MockServiceWorkerResponseReader>(),
          std::make_unique<MockServiceWorkerResponseWriter>(),
          loader_factory.get(), &check_result);

  const network::ResourceRequest* request = nullptr;
  ASSERT_TRUE(loader_factory->IsPending(kScriptURL, &request));
  EXPECT_TRUE(request->load_flags & net::LOAD_VALIDATE_CACHE);

  // Load imported script. Should validate the cache.
  checker = CreateSingleScriptUpdateChecker(
      kImportedScriptURL, false /* is_main_script */,
      false /* force_bypass_cache */,
      blink::mojom::ServiceWorkerUpdateViaCache::kAll,
      base::TimeDelta::FromDays(1) + base::TimeDelta::FromHours(1),
      std::make_unique<MockServiceWorkerResponseReader>(),
      std::make_unique<MockServiceWorkerResponseReader>(),
      std::make_unique<MockServiceWorkerResponseWriter>(), loader_factory.get(),
      &check_result);

  ASSERT_TRUE(loader_factory->IsPending(kImportedScriptURL, &request));
  EXPECT_TRUE(request->load_flags & net::LOAD_VALIDATE_CACHE);
}

}  // namespace
}  // namespace content
