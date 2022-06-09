// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_registry.h"

#include "base/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/quota_manager_proxy_sync.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/origin_trials/scoped_test_origin_trial_policy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"

namespace content {

namespace {

struct ReadResponseHeadResult {
  int result;
  network::mojom::URLResponseHeadPtr response_head;
  absl::optional<mojo_base::BigBuffer> metadata;
};

struct GetStorageUsageForStorageKeyResult {
  blink::ServiceWorkerStatusCode status;
  int64_t usage;
};

storage::mojom::ServiceWorkerResourceRecordPtr
CreateResourceRecord(int64_t resource_id, const GURL& url, int64_t size_bytes) {
  EXPECT_TRUE(url.is_valid());
  return storage::mojom::ServiceWorkerResourceRecord::New(resource_id, url,
                                                          size_bytes);
}

storage::mojom::ServiceWorkerRegistrationDataPtr CreateRegistrationData(
    int64_t registration_id,
    int64_t version_id,
    const GURL& scope,
    const blink::StorageKey& key,
    const GURL& script_url,
    const std::vector<storage::mojom::ServiceWorkerResourceRecordPtr>&
        resources) {
  auto data = storage::mojom::ServiceWorkerRegistrationData::New();
  data->registration_id = registration_id;
  data->version_id = version_id;
  data->scope = scope;
  data->key = key;
  data->script = script_url;
  data->navigation_preload_state = blink::mojom::NavigationPreloadState::New();
  data->is_active = true;

  int64_t resources_total_size_bytes = 0;
  for (auto& resource : resources) {
    resources_total_size_bytes += resource->size_bytes;
  }
  data->resources_total_size_bytes = resources_total_size_bytes;

  return data;
}

int WriteResponse(
    mojo::Remote<storage::mojom::ServiceWorkerStorageControl>& storage,
    int64_t id,
    const std::string& headers,
    mojo_base::BigBuffer body) {
  mojo::Remote<storage::mojom::ServiceWorkerResourceWriter> writer;
  storage->CreateResourceWriter(id, writer.BindNewPipeAndPassReceiver());

  int rv = 0;
  {
    auto response_head = network::mojom::URLResponseHead::New();
    response_head->request_time = base::Time::Now();
    response_head->response_time = base::Time::Now();
    response_head->headers = new net::HttpResponseHeaders(headers);
    response_head->content_length = body.size();

    base::RunLoop loop;
    writer->WriteResponseHead(std::move(response_head),
                              base::BindLambdaForTesting([&](int result) {
                                rv = result;
                                loop.Quit();
                              }));
    loop.Run();
    if (rv < 0)
      return rv;
  }

  {
    base::RunLoop loop;
    writer->WriteData(std::move(body),
                      base::BindLambdaForTesting([&](int result) {
                        rv = result;
                        loop.Quit();
                      }));
    loop.Run();
  }

  return rv;
}

int WriteStringResponse(
    mojo::Remote<storage::mojom::ServiceWorkerStorageControl>& storage,
    int64_t id,
    const std::string& headers,
    const std::string& body) {
  mojo_base::BigBuffer buffer(
      base::as_bytes(base::make_span(body.data(), body.length())));
  return WriteResponse(storage, id, headers, std::move(buffer));
}

int WriteBasicResponse(
    mojo::Remote<storage::mojom::ServiceWorkerStorageControl>& storage,
    int64_t id) {
  const char kHttpHeaders[] = "HTTP/1.0 200 HONKYDORY\0Content-Length: 5\0\0";
  const char kHttpBody[] = "Hello";
  std::string headers(kHttpHeaders, base::size(kHttpHeaders));
  return WriteStringResponse(storage, id, headers, std::string(kHttpBody));
}

ReadResponseHeadResult ReadResponseHead(
    mojo::Remote<storage::mojom::ServiceWorkerStorageControl>& storage,
    int64_t id) {
  mojo::Remote<storage::mojom::ServiceWorkerResourceReader> reader;
  storage->CreateResourceReader(id, reader.BindNewPipeAndPassReceiver());

  ReadResponseHeadResult out;
  base::RunLoop loop;
  reader->ReadResponseHead(base::BindLambdaForTesting(
      [&](int result, network::mojom::URLResponseHeadPtr response_head,
          absl::optional<mojo_base::BigBuffer> metadata) {
        out.result = result;
        out.response_head = std::move(response_head);
        out.metadata = std::move(metadata);
        loop.Quit();
      }));
  loop.Run();
  return out;
}

bool VerifyBasicResponse(
    mojo::Remote<storage::mojom::ServiceWorkerStorageControl>& storage,
    int64_t id,
    bool expected_positive_result) {
  const std::string kExpectedHttpBody("Hello");
  ReadResponseHeadResult out = ReadResponseHead(storage, id);
  if (expected_positive_result)
    EXPECT_LT(0, out.result);
  if (out.result <= 0)
    return false;

  mojo::Remote<storage::mojom::ServiceWorkerResourceReader> reader;
  storage->CreateResourceReader(id, reader.BindNewPipeAndPassReceiver());

  const int kBigEnough = 512;
  MockServiceWorkerDataPipeStateNotifier notifier;
  mojo::ScopedDataPipeConsumerHandle data_consumer;
  base::RunLoop loop;
  reader->ReadData(
      kBigEnough, notifier.BindNewPipeAndPassRemote(),
      base::BindLambdaForTesting([&](mojo::ScopedDataPipeConsumerHandle pipe) {
        data_consumer = std::move(pipe);
        loop.Quit();
      }));
  loop.Run();

  std::string body = ReadDataPipe(std::move(data_consumer));
  int rv = notifier.WaitUntilComplete();

  EXPECT_EQ(static_cast<int>(kExpectedHttpBody.size()), rv);
  if (rv <= 0)
    return false;

  bool status_match =
      std::string("HONKYDORY") == out.response_head->headers->GetStatusText();
  bool data_match = kExpectedHttpBody == body;

  EXPECT_EQ(out.response_head->headers->GetStatusText(), "HONKYDORY");
  EXPECT_EQ(body, kExpectedHttpBody);
  return status_match && data_match;
}

int WriteResponseMetadata(
    mojo::Remote<storage::mojom::ServiceWorkerStorageControl>& storage,
    int64_t id,
    const std::string& metadata) {
  mojo_base::BigBuffer buffer(
      base::as_bytes(base::make_span(metadata.data(), metadata.length())));

  mojo::Remote<storage::mojom::ServiceWorkerResourceMetadataWriter>
      metadata_writer;
  storage->CreateResourceMetadataWriter(
      id, metadata_writer.BindNewPipeAndPassReceiver());
  int rv = 0;
  base::RunLoop loop;
  metadata_writer->WriteMetadata(std::move(buffer),
                                 base::BindLambdaForTesting([&](int result) {
                                   rv = result;
                                   loop.Quit();
                                 }));
  loop.Run();
  return rv;
}

int WriteMetadata(ServiceWorkerVersion* version,
                  const GURL& url,
                  const std::string& metadata) {
  const std::vector<uint8_t> data(metadata.begin(), metadata.end());
  EXPECT_TRUE(version);
  net::TestCompletionCallback cb;
  version->script_cache_map()->WriteMetadata(url, data, cb.callback());
  return cb.WaitForResult();
}

int ClearMetadata(ServiceWorkerVersion* version, const GURL& url) {
  EXPECT_TRUE(version);
  net::TestCompletionCallback cb;
  version->script_cache_map()->ClearMetadata(url, cb.callback());
  return cb.WaitForResult();
}

bool VerifyResponseMetadata(
    mojo::Remote<storage::mojom::ServiceWorkerStorageControl>& storage,
    int64_t id,
    const std::string& expected_metadata) {
  mojo::Remote<storage::mojom::ServiceWorkerResourceReader> reader;
  storage->CreateResourceReader(id, reader.BindNewPipeAndPassReceiver());
  ReadResponseHeadResult out = ReadResponseHead(storage, id);
  if (!out.metadata.has_value())
    return false;
  EXPECT_EQ(0, memcmp(expected_metadata.data(), out.metadata->data(),
                      expected_metadata.length()));
  return true;
}

}  // namespace

class ServiceWorkerRegistryTest : public testing::Test {
 public:
  ServiceWorkerRegistryTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    CHECK(user_data_directory_.CreateUniqueTempDir());
    user_data_directory_path_ = user_data_directory_.GetPath();
    special_storage_policy_ =
        base::MakeRefCounted<storage::MockSpecialStoragePolicy>();
    InitializeTestHelper();
  }

  void TearDown() override {
    helper_.reset();
    disk_cache::FlushCacheThreadForTesting();
    content::RunAllTasksUntilIdle();
  }

  EmbeddedWorkerTestHelper* helper() { return helper_.get(); }
  ServiceWorkerContextCore* context() { return helper_->context(); }
  ServiceWorkerRegistry* registry() { return context()->registry(); }
  mojo::Remote<storage::mojom::ServiceWorkerStorageControl>& storage_control() {
    return registry()->GetRemoteStorageControl();
  }

  storage::MockSpecialStoragePolicy* special_storage_policy() {
    return special_storage_policy_.get();
  }

  storage::QuotaManagerProxy* quota_manager_proxy() {
    return registry()->quota_manager_proxy_.get();
  }

  size_t inflight_call_count() { return registry()->inflight_calls_.size(); }

  void InitializeTestHelper() {
    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(
        user_data_directory_path_, special_storage_policy_.get());
  }

  void SimulateRestart() {
    // Need to reset |helper_| then wait for scheduled tasks to be finished
    // because |helper_| has TestBrowserContext and the dtor schedules storage
    // cleanup tasks.
    helper_.reset();
    base::RunLoop().RunUntilIdle();
    InitializeTestHelper();
  }

  void EnsureRemoteCallsAreExecuted() {
    storage_control().FlushForTesting();
    // ServiceWorkerRegistry has an internal queue for inflight remote calls.
    // Run all tasks until all calls in the queue are executed.
    content::RunAllTasksUntilIdle();
  }

  std::vector<blink::StorageKey> GetRegisteredStorageKeys() {
    std::vector<blink::StorageKey> result;
    base::RunLoop loop;
    registry()->GetRegisteredStorageKeys(base::BindLambdaForTesting(
        [&](const std::vector<blink::StorageKey>& storage_keys) {
          result = storage_keys;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  blink::ServiceWorkerStatusCode FindRegistrationForClientUrl(
      const GURL& document_url,
      const blink::StorageKey& key,
      scoped_refptr<ServiceWorkerRegistration>& out_registration) {
    blink::ServiceWorkerStatusCode result;
    base::RunLoop loop;
    registry()->FindRegistrationForClientUrl(
        document_url, key,
        base::BindLambdaForTesting(
            [&](blink::ServiceWorkerStatusCode status,
                scoped_refptr<ServiceWorkerRegistration> registration) {
              result = status;
              out_registration = std::move(registration);
              loop.Quit();
            }));
    loop.Run();
    return result;
  }

  blink::ServiceWorkerStatusCode FindRegistrationForScope(
      const GURL& scope,
      const blink::StorageKey& key,
      scoped_refptr<ServiceWorkerRegistration>& out_registration) {
    blink::ServiceWorkerStatusCode result;
    base::RunLoop loop;
    registry()->FindRegistrationForScope(
        scope, key,
        base::BindLambdaForTesting(
            [&](blink::ServiceWorkerStatusCode status,
                scoped_refptr<ServiceWorkerRegistration> registration) {
              result = status;
              out_registration = std::move(registration);
              loop.Quit();
            }));
    loop.Run();
    return result;
  }

  blink::ServiceWorkerStatusCode FindRegistrationForId(
      int64_t registration_id,
      const blink::StorageKey& key,
      scoped_refptr<ServiceWorkerRegistration>& out_registration) {
    blink::ServiceWorkerStatusCode result;
    base::RunLoop loop;
    registry()->FindRegistrationForId(
        registration_id, key,
        base::BindLambdaForTesting(
            [&](blink::ServiceWorkerStatusCode status,
                scoped_refptr<ServiceWorkerRegistration> registration) {
              result = status;
              out_registration = std::move(registration);
              loop.Quit();
            }));
    loop.Run();
    return result;
  }

  blink::ServiceWorkerStatusCode FindRegistrationForIdOnly(
      int64_t registration_id,
      scoped_refptr<ServiceWorkerRegistration>& out_registration) {
    blink::ServiceWorkerStatusCode result;
    base::RunLoop loop;
    registry()->FindRegistrationForIdOnly(
        registration_id,
        base::BindLambdaForTesting(
            [&](blink::ServiceWorkerStatusCode status,
                scoped_refptr<ServiceWorkerRegistration> registration) {
              result = status;
              out_registration = std::move(registration);
              loop.Quit();
            }));
    loop.Run();
    return result;
  }

  blink::ServiceWorkerStatusCode StoreRegistration(
      scoped_refptr<ServiceWorkerRegistration> registration,
      scoped_refptr<ServiceWorkerVersion> version) {
    blink::ServiceWorkerStatusCode result;
    base::RunLoop loop;
    registry()->StoreRegistration(
        registration.get(), version.get(),
        base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
          result = status;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  blink::ServiceWorkerStatusCode DeleteRegistration(
      scoped_refptr<ServiceWorkerRegistration> registration,
      const blink::StorageKey& key) {
    blink::ServiceWorkerStatusCode result;
    base::RunLoop loop;
    registry()->DeleteRegistration(
        registration, key,
        base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
          result = status;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  blink::ServiceWorkerStatusCode UpdateToActiveState(
      const ServiceWorkerRegistration* registration) {
    base::RunLoop loop;
    blink::ServiceWorkerStatusCode result;
    registry()->UpdateToActiveState(
        registration->id(), registration->key(),
        base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
          result = status;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  blink::ServiceWorkerStatusCode UpdateLastUpdateCheckTime(
      const ServiceWorkerRegistration* registration) {
    base::RunLoop loop;
    blink::ServiceWorkerStatusCode result;
    registry()->UpdateLastUpdateCheckTime(
        registration->id(), registration->key(),
        registration->last_update_check(),
        base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
          result = status;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  GetStorageUsageForStorageKeyResult GetStorageUsageForStorageKey(
      const blink::StorageKey& key) {
    GetStorageUsageForStorageKeyResult result;
    base::RunLoop loop;
    registry()->GetStorageUsageForStorageKey(
        key, base::BindLambdaForTesting(
                 [&](blink::ServiceWorkerStatusCode status, int64_t usage) {
                   result.status = status;
                   result.usage = usage;
                   loop.Quit();
                 }));
    loop.Run();
    return result;
  }

  blink::ServiceWorkerStatusCode GetAllRegistrationsInfos(
      std::vector<ServiceWorkerRegistrationInfo>* registrations) {
    absl::optional<blink::ServiceWorkerStatusCode> result;
    base::RunLoop loop;
    registry()->GetAllRegistrationsInfos(base::BindLambdaForTesting(
        [&](blink::ServiceWorkerStatusCode status,
            const std::vector<ServiceWorkerRegistrationInfo>& infos) {
          result = status;
          *registrations = infos;
          loop.Quit();
        }));
    EXPECT_FALSE(result.has_value());  // always async
    loop.Run();
    return result.value();
  }

  blink::ServiceWorkerStatusCode GetRegistrationsForStorageKey(
      const blink::StorageKey& key,
      std::vector<scoped_refptr<ServiceWorkerRegistration>>& registrations) {
    blink::ServiceWorkerStatusCode result;
    base::RunLoop loop;
    registry()->GetRegistrationsForStorageKey(
        key,
        base::BindLambdaForTesting(
            [&](blink::ServiceWorkerStatusCode status,
                const std::vector<scoped_refptr<ServiceWorkerRegistration>>&
                    found_registrations) {
              result = status;
              registrations = found_registrations;
              loop.Quit();
            }));
    loop.Run();
    return result;
  }

  std::vector<int64_t> GetPurgeableResourceIds() {
    std::vector<int64_t> ids;
    base::RunLoop loop;
    storage_control()->GetPurgeableResourceIdsForTest(
        base::BindLambdaForTesting(
            [&](storage::mojom::ServiceWorkerDatabaseStatus status,
                const std::vector<int64_t>& resource_ids) {
              EXPECT_EQ(status,
                        storage::mojom::ServiceWorkerDatabaseStatus::kOk);
              ids = resource_ids;
              loop.Quit();
            }));
    loop.Run();
    return ids;
  }

  std::vector<int64_t> GetPurgingResources() {
    std::vector<int64_t> ids;
    base::RunLoop loop;
    storage_control()->GetPurgingResourceIdsForTest(base::BindLambdaForTesting(
        [&](storage::mojom::ServiceWorkerDatabaseStatus status,
            const std::vector<int64_t>& resource_ids) {
          EXPECT_EQ(status, storage::mojom::ServiceWorkerDatabaseStatus::kOk);
          ids = resource_ids;
          loop.Quit();
        }));
    loop.Run();
    return ids;
  }

  void StoreRegistrationData(
      storage::mojom::ServiceWorkerRegistrationDataPtr registration_data,
      std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> resources) {
    base::RunLoop loop;
    storage_control()->StoreRegistration(
        std::move(registration_data), std::move(resources),
        base::BindLambdaForTesting(
            [&](storage::mojom::ServiceWorkerDatabaseStatus status,
                uint64_t /*deleted_resources_size*/) {
              ASSERT_EQ(storage::mojom::ServiceWorkerDatabaseStatus::kOk,
                        status);
              loop.Quit();
            }));
    loop.Run();
  }

 private:
  // |user_data_directory_| must be declared first to preserve destructor order.
  base::ScopedTempDir user_data_directory_;
  base::FilePath user_data_directory_path_;

  BrowserTaskEnvironment task_environment_;
  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
};

TEST_F(ServiceWorkerRegistryTest, RegisteredStorageKeyCount) {
  {
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(GetRegisteredStorageKeys().empty());
    histogram_tester.ExpectUniqueSample(
        "ServiceWorker.RegisteredStorageKeyCount", 0, 1);
  }

  std::pair<GURL, GURL> scope_and_script_pairs[] = {
      {GURL("https://www.example.com/scope/"),
       GURL("https://www.example.com/script.js")},
      {GURL("https://www.example.com/scope/foo"),
       GURL("https://www.example.com/script.js")},
      {GURL("https://www.test.com/scope/foobar"),
       GURL("https://www.test.com/script.js")},
      {GURL("https://example.com/scope/"),
       GURL("https://example.com/script.js")},
  };
  std::vector<scoped_refptr<ServiceWorkerRegistration>> registrations;
  int64_t dummy_resource_id = 1;
  for (const auto& pair : scope_and_script_pairs) {
    blink::StorageKey key(url::Origin::Create(pair.first));
    registrations.emplace_back(CreateServiceWorkerRegistrationAndVersion(
        context(), pair.first, pair.second, key, dummy_resource_id));
    ++dummy_resource_id;
  }

  // Store all registrations.
  for (const auto& registration : registrations) {
    EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
              StoreRegistration(registration, registration->waiting_version()));
  }

  SimulateRestart();

  {
    base::HistogramTester histogram_tester;
    EXPECT_EQ(3UL, GetRegisteredStorageKeys().size());
    histogram_tester.ExpectUniqueSample(
        "ServiceWorker.RegisteredStorageKeyCount", 3, 1);
  }

  // Re-initializing shouldn't re-record the histogram.
  {
    base::HistogramTester histogram_tester;
    EXPECT_EQ(3UL, GetRegisteredStorageKeys().size());
    histogram_tester.ExpectTotalCount("ServiceWorker.RegisteredStorageKeyCount",
                                      0);
  }
}

TEST_F(ServiceWorkerRegistryTest, CreateNewRegistration) {
  EnsureRemoteCallsAreExecuted();

  const GURL kScope("http://www.test.not/scope/");
  const blink::StorageKey kKey(url::Origin::Create(kScope));

  scoped_refptr<ServiceWorkerRegistration> registration;

  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = kScope;

  storage::QuotaManagerProxySync quota_manager_proxy_sync(
      quota_manager_proxy());

  base::RunLoop loop;
  registry()->CreateNewRegistration(
      std::move(options), kKey,
      base::BindLambdaForTesting(
          [&](scoped_refptr<ServiceWorkerRegistration> new_registration) {
            EXPECT_EQ(new_registration->scope(), kScope);
            registration = new_registration;
            loop.Quit();
          }));
  loop.Run();

  // Check default bucket exists.com.
  storage::QuotaErrorOr<storage::BucketInfo> result =
      quota_manager_proxy_sync.GetBucket(kKey, storage::kDefaultBucketName,
                                         blink::mojom::StorageType::kTemporary);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->name, storage::kDefaultBucketName);
  EXPECT_EQ(result->storage_key, kKey);
  EXPECT_GT(result->id.value(), 0);
}

TEST_F(ServiceWorkerRegistryTest, GetOrCreateBucketError) {
  const GURL kScope("http://www.test.not/scope/");
  const blink::StorageKey kKey(url::Origin::Create(kScope));

  scoped_refptr<ServiceWorkerRegistration> registration;

  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = kScope;

  helper()->quota_manager()->SetDisableDatabase(true);
  storage::QuotaManagerProxySync quota_manager_proxy_sync(
      quota_manager_proxy());

  base::RunLoop loop;
  registry()->CreateNewRegistration(
      std::move(options), kKey,
      base::BindLambdaForTesting(
          [&](scoped_refptr<ServiceWorkerRegistration> new_registration) {
            EXPECT_EQ(new_registration, nullptr);
            loop.Quit();
          }));
  loop.Run();
}

TEST_F(ServiceWorkerRegistryTest, StoreFindUpdateDeleteRegistration) {
  const GURL kScope("http://www.test.not/scope/");
  const blink::StorageKey kKey(url::Origin::Create(kScope));
  const GURL kDocumentUrl("http://www.test.not/scope/document.html");
  const GURL kResource1("http://www.test.not/scope/resource1.js");
  const int64_t kResource1Size = 1591234;
  const GURL kResource2("http://www.test.not/scope/resource2.js");
  const int64_t kResource2Size = 51;
  const int64_t kRegistrationId = 0;
  const int64_t kVersionId = 0;
  const base::Time kToday = base::Time::Now();
  const base::Time kYesterday = kToday - base::Days(1);
  std::set<blink::mojom::WebFeature> used_features = {
      blink::mojom::WebFeature::kServiceWorkerControlledPage,
      blink::mojom::WebFeature::kReferrerPolicyHeader,
      blink::mojom::WebFeature::kLocationOrigin};

  scoped_refptr<ServiceWorkerRegistration> found_registration;

  // We shouldn't find anything without having stored anything.
  EXPECT_EQ(
      blink::ServiceWorkerStatusCode::kErrorNotFound,
      FindRegistrationForClientUrl(kDocumentUrl, kKey, found_registration));
  EXPECT_FALSE(found_registration.get());

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            FindRegistrationForScope(kScope, kKey, found_registration));
  EXPECT_FALSE(found_registration.get());

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            FindRegistrationForId(kRegistrationId, kKey, found_registration));
  EXPECT_FALSE(found_registration.get());

  // Because nothing was found we shouldn't have notified the quota manager
  // about any accesses.
  EXPECT_EQ(0,
            helper()->quota_manager_proxy()->notify_storage_accessed_count());

  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> resources;
  resources.push_back(CreateResourceRecord(1, kResource1, kResource1Size));
  resources.push_back(CreateResourceRecord(2, kResource2, kResource2Size));

  // Store something.
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = kScope;
  scoped_refptr<ServiceWorkerRegistration> live_registration =
      new ServiceWorkerRegistration(options, kKey, kRegistrationId,
                                    context()->AsWeakPtr());
  scoped_refptr<ServiceWorkerVersion> live_version = new ServiceWorkerVersion(
      live_registration.get(), kResource1, blink::mojom::ScriptType::kClassic,
      kVersionId,
      mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
      context()->AsWeakPtr());
  live_version->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  live_version->SetStatus(ServiceWorkerVersion::INSTALLED);
  live_version->script_cache_map()->SetResources(resources);
  live_version->set_used_features(
      std::set<blink::mojom::WebFeature>(used_features));
  network::CrossOriginEmbedderPolicy coep_require_corp;
  coep_require_corp.value =
      network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;
  live_version->set_cross_origin_embedder_policy(coep_require_corp);
  live_registration->SetWaitingVersion(live_version);
  live_registration->set_last_update_check(kYesterday);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StoreRegistration(live_registration, live_version));

  // Should have notified for the store.
  EXPECT_EQ(1,
            helper()->quota_manager_proxy()->notify_storage_modified_count());
  // Still shouldn't have notified any accesses.
  EXPECT_EQ(0,
            helper()->quota_manager_proxy()->notify_storage_accessed_count());

  // Now we should find it and get the live ptr back immediately.
  EXPECT_EQ(
      blink::ServiceWorkerStatusCode::kOk,
      FindRegistrationForClientUrl(kDocumentUrl, kKey, found_registration));
  EXPECT_EQ(1,
            helper()->quota_manager_proxy()->notify_storage_accessed_count());
  EXPECT_EQ(live_registration, found_registration);
  EXPECT_EQ(kResource1Size + kResource2Size,
            live_registration->resources_total_size_bytes());
  EXPECT_EQ(kResource1Size + kResource2Size,
            found_registration->resources_total_size_bytes());
  EXPECT_EQ(used_features,
            found_registration->waiting_version()->used_features());
  EXPECT_EQ(
      found_registration->waiting_version()->cross_origin_embedder_policy(),
      coep_require_corp);
  found_registration = nullptr;

  // But FindRegistrationForScope is always async.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForScope(kScope, kKey, found_registration));
  EXPECT_EQ(2,
            helper()->quota_manager_proxy()->notify_storage_accessed_count());
  EXPECT_EQ(live_registration, found_registration);
  found_registration = nullptr;

  // Can be found by id too.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForId(kRegistrationId, kKey, found_registration));
  EXPECT_EQ(3,
            helper()->quota_manager_proxy()->notify_storage_accessed_count());
  ASSERT_TRUE(found_registration.get());
  EXPECT_EQ(kRegistrationId, found_registration->id());
  EXPECT_EQ(live_registration, found_registration);
  found_registration = nullptr;

  // Can be found by just the id too.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForIdOnly(kRegistrationId, found_registration));
  EXPECT_EQ(4,
            helper()->quota_manager_proxy()->notify_storage_accessed_count());
  ASSERT_TRUE(found_registration.get());
  EXPECT_EQ(kRegistrationId, found_registration->id());
  EXPECT_EQ(live_registration, found_registration);
  found_registration = nullptr;

  // Drop the live registration, but keep the version live.
  live_registration = nullptr;

  // Now FindRegistrationForClientUrl should be async.
  EXPECT_EQ(
      blink::ServiceWorkerStatusCode::kOk,
      FindRegistrationForClientUrl(kDocumentUrl, kKey, found_registration));
  EXPECT_EQ(5,
            helper()->quota_manager_proxy()->notify_storage_accessed_count());
  ASSERT_TRUE(found_registration.get());
  EXPECT_EQ(kRegistrationId, found_registration->id());
  EXPECT_TRUE(found_registration->HasOneRef());

  // Check that sizes are populated correctly
  EXPECT_EQ(live_version.get(), found_registration->waiting_version());
  EXPECT_EQ(kResource1Size + kResource2Size,
            found_registration->resources_total_size_bytes());
  std::vector<ServiceWorkerRegistrationInfo> all_registrations;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetAllRegistrationsInfos(&all_registrations));
  EXPECT_EQ(1u, all_registrations.size());
  ServiceWorkerRegistrationInfo info = all_registrations[0];
  EXPECT_EQ(kResource1Size + kResource2Size, info.stored_version_size_bytes);
  all_registrations.clear();

  // Finding by StorageKey should provide the same result if the StorageKey's
  // origin is kScope.
  std::vector<scoped_refptr<ServiceWorkerRegistration>>
      registrations_for_storage_key;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetRegistrationsForStorageKey(kKey, registrations_for_storage_key));
  EXPECT_EQ(1u, registrations_for_storage_key.size());
  registrations_for_storage_key.clear();

  EXPECT_EQ(
      blink::ServiceWorkerStatusCode::kOk,
      GetRegistrationsForStorageKey(
          blink::StorageKey::CreateFromStringForTesting("http://example.com/"),
          registrations_for_storage_key));
  EXPECT_TRUE(registrations_for_storage_key.empty());

  found_registration = nullptr;

  // Drop the live version too.
  live_version = nullptr;

  // And FindRegistrationForScope is always async.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForScope(kScope, kKey, found_registration));
  EXPECT_EQ(6,
            helper()->quota_manager_proxy()->notify_storage_accessed_count());
  ASSERT_TRUE(found_registration.get());
  EXPECT_EQ(kRegistrationId, found_registration->id());
  EXPECT_TRUE(found_registration->HasOneRef());
  EXPECT_FALSE(found_registration->active_version());
  ASSERT_TRUE(found_registration->waiting_version());
  EXPECT_EQ(kYesterday, found_registration->last_update_check());
  EXPECT_EQ(ServiceWorkerVersion::INSTALLED,
            found_registration->waiting_version()->status());

  // Update to active and update the last check time.
  scoped_refptr<ServiceWorkerVersion> temp_version =
      found_registration->waiting_version();
  temp_version->SetStatus(ServiceWorkerVersion::ACTIVATED);
  found_registration->SetActiveVersion(temp_version);
  temp_version = nullptr;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            UpdateToActiveState(found_registration.get()));
  found_registration->set_last_update_check(kToday);
  UpdateLastUpdateCheckTime(found_registration.get());

  found_registration = nullptr;

  // Trying to update a unstored registration to active should fail.
  scoped_refptr<ServiceWorkerRegistration> unstored_registration =
      new ServiceWorkerRegistration(options, kKey, kRegistrationId + 1,
                                    context()->AsWeakPtr());
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            UpdateToActiveState(unstored_registration.get()));
  unstored_registration = nullptr;

  // The Find methods should return a registration with an active version
  // and the expected update time.
  EXPECT_EQ(
      blink::ServiceWorkerStatusCode::kOk,
      FindRegistrationForClientUrl(kDocumentUrl, kKey, found_registration));
  EXPECT_EQ(7,
            helper()->quota_manager_proxy()->notify_storage_accessed_count());
  ASSERT_TRUE(found_registration.get());
  EXPECT_EQ(kRegistrationId, found_registration->id());
  EXPECT_TRUE(found_registration->HasOneRef());
  EXPECT_FALSE(found_registration->waiting_version());
  ASSERT_TRUE(found_registration->active_version());
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATED,
            found_registration->active_version()->status());
  EXPECT_EQ(kToday, found_registration->last_update_check());

  // Confirm that we only notified a modification once.
  EXPECT_EQ(1,
            helper()->quota_manager_proxy()->notify_storage_modified_count());
}

TEST_F(ServiceWorkerRegistryTest, InstallingRegistrationsAreFindable) {
  const GURL kScope("http://www.test.not/scope/");
  const blink::StorageKey kKey(url::Origin::Create(kScope));
  const GURL kScript("http://www.test.not/script.js");
  const GURL kDocumentUrl("http://www.test.not/scope/document.html");
  const int64_t kVersionId = 0;

  scoped_refptr<ServiceWorkerRegistration> found_registration;

  // Create an unstored registration.
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = kScope;
  scoped_refptr<ServiceWorkerRegistration> live_registration =
      CreateNewServiceWorkerRegistration(registry(), options, kKey);
  scoped_refptr<ServiceWorkerVersion> live_version = new ServiceWorkerVersion(
      live_registration.get(), kScript, blink::mojom::ScriptType::kClassic,
      kVersionId,
      mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
      context()->AsWeakPtr());
  live_version->SetStatus(ServiceWorkerVersion::INSTALLING);
  live_registration->SetWaitingVersion(live_version);

  const int64_t kRegistrationId = live_registration->id();

  // Should not be findable, including by GetAllRegistrationsInfos.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            FindRegistrationForId(kRegistrationId, kKey, found_registration));
  EXPECT_FALSE(found_registration.get());

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            FindRegistrationForIdOnly(kRegistrationId, found_registration));
  EXPECT_FALSE(found_registration.get());

  EXPECT_EQ(
      blink::ServiceWorkerStatusCode::kErrorNotFound,
      FindRegistrationForClientUrl(kDocumentUrl, kKey, found_registration));
  EXPECT_FALSE(found_registration.get());

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            FindRegistrationForScope(kScope, kKey, found_registration));
  EXPECT_FALSE(found_registration.get());

  std::vector<ServiceWorkerRegistrationInfo> all_registrations;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetAllRegistrationsInfos(&all_registrations));
  EXPECT_TRUE(all_registrations.empty());

  std::vector<scoped_refptr<ServiceWorkerRegistration>>
      registrations_for_storage_key;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetRegistrationsForStorageKey(kKey, registrations_for_storage_key));
  EXPECT_TRUE(registrations_for_storage_key.empty());

  EXPECT_EQ(
      blink::ServiceWorkerStatusCode::kOk,
      GetRegistrationsForStorageKey(
          blink::StorageKey::CreateFromStringForTesting("http://example.com/"),
          registrations_for_storage_key));
  EXPECT_TRUE(registrations_for_storage_key.empty());

  // Notify storage of it being installed.
  registry()->NotifyInstallingRegistration(live_registration.get());

  // Now should be findable.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForId(kRegistrationId, kKey, found_registration));
  EXPECT_EQ(live_registration, found_registration);
  found_registration = nullptr;

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForIdOnly(kRegistrationId, found_registration));
  EXPECT_EQ(live_registration, found_registration);
  found_registration = nullptr;

  EXPECT_EQ(
      blink::ServiceWorkerStatusCode::kOk,
      FindRegistrationForClientUrl(kDocumentUrl, kKey, found_registration));
  EXPECT_EQ(live_registration, found_registration);
  found_registration = nullptr;

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForScope(kScope, kKey, found_registration));
  EXPECT_EQ(live_registration, found_registration);
  found_registration = nullptr;

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetAllRegistrationsInfos(&all_registrations));
  EXPECT_EQ(1u, all_registrations.size());
  all_registrations.clear();

  // Finding by StorageKey should provide the same result if the StorageKey's
  // origin is kScope.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetRegistrationsForStorageKey(kKey, registrations_for_storage_key));
  EXPECT_EQ(1u, registrations_for_storage_key.size());
  registrations_for_storage_key.clear();

  EXPECT_EQ(
      blink::ServiceWorkerStatusCode::kOk,
      GetRegistrationsForStorageKey(
          blink::StorageKey::CreateFromStringForTesting("http://example.com/"),
          registrations_for_storage_key));
  EXPECT_TRUE(registrations_for_storage_key.empty());

  // Notify storage of installation no longer happening.
  registry()->NotifyDoneInstallingRegistration(
      live_registration.get(), nullptr, blink::ServiceWorkerStatusCode::kOk);

  // Once again, should not be findable.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            FindRegistrationForId(kRegistrationId, kKey, found_registration));
  EXPECT_FALSE(found_registration.get());

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            FindRegistrationForIdOnly(kRegistrationId, found_registration));
  EXPECT_FALSE(found_registration.get());

  EXPECT_EQ(
      blink::ServiceWorkerStatusCode::kErrorNotFound,
      FindRegistrationForClientUrl(kDocumentUrl, kKey, found_registration));
  EXPECT_FALSE(found_registration.get());

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            FindRegistrationForScope(kScope, kKey, found_registration));
  EXPECT_FALSE(found_registration.get());

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetAllRegistrationsInfos(&all_registrations));
  EXPECT_TRUE(all_registrations.empty());

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            GetRegistrationsForStorageKey(kKey, registrations_for_storage_key));
  EXPECT_TRUE(registrations_for_storage_key.empty());

  EXPECT_EQ(
      blink::ServiceWorkerStatusCode::kOk,
      GetRegistrationsForStorageKey(
          blink::StorageKey::CreateFromStringForTesting("http://example.com/"),
          registrations_for_storage_key));
  EXPECT_TRUE(registrations_for_storage_key.empty());

  // Installing registrations should not trigger accessed count
  EXPECT_EQ(0,
            helper()->quota_manager_proxy()->notify_storage_accessed_count());
}

TEST_F(ServiceWorkerRegistryTest, FindRegistration_LongestScopeMatch) {
  const GURL kDocumentUrl("http://www.example.com/scope/foo");
  const blink::StorageKey kKey(url::Origin::Create(kDocumentUrl));
  scoped_refptr<ServiceWorkerRegistration> found_registration;

  // Registration for "/scope/".
  const GURL kScope1("http://www.example.com/scope/");
  const GURL kScript1("http://www.example.com/script1.js");
  scoped_refptr<ServiceWorkerRegistration> live_registration1 =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope1, kScript1,
                                                kKey,
                                                /*resource_id=*/1);

  // Registration for "/scope/foo".
  const GURL kScope2("http://www.example.com/scope/foo");
  const GURL kScript2("http://www.example.com/script2.js");
  scoped_refptr<ServiceWorkerRegistration> live_registration2 =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope2, kScript2,
                                                kKey,
                                                /*resource_id=*/2);

  // Registration for "/scope/foobar".
  const GURL kScope3("http://www.example.com/scope/foobar");
  const GURL kScript3("http://www.example.com/script3.js");
  scoped_refptr<ServiceWorkerRegistration> live_registration3 =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope3, kScript3,
                                                kKey,
                                                /*resource_id=*/3);

  // Notify storage of them being installed.
  registry()->NotifyInstallingRegistration(live_registration1.get());
  registry()->NotifyInstallingRegistration(live_registration2.get());
  registry()->NotifyInstallingRegistration(live_registration3.get());

  // Registrations in the installing state shouldn't trigger a modified
  // notification.
  EXPECT_EQ(0,
            helper()->quota_manager_proxy()->notify_storage_modified_count());

  // Find a registration among installing ones.
  EXPECT_EQ(
      blink::ServiceWorkerStatusCode::kOk,
      FindRegistrationForClientUrl(kDocumentUrl, kKey, found_registration));
  EXPECT_EQ(live_registration2, found_registration);
  found_registration = nullptr;

  // Store registrations.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StoreRegistration(live_registration1,
                              live_registration1->waiting_version()));
  EXPECT_EQ(1,
            helper()->quota_manager_proxy()->notify_storage_modified_count());
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StoreRegistration(live_registration2,
                              live_registration2->waiting_version()));
  EXPECT_EQ(2,
            helper()->quota_manager_proxy()->notify_storage_modified_count());
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StoreRegistration(live_registration3,
                              live_registration3->waiting_version()));
  EXPECT_EQ(3,
            helper()->quota_manager_proxy()->notify_storage_modified_count());

  // Notify storage of installations no longer happening.
  registry()->NotifyDoneInstallingRegistration(
      live_registration1.get(), nullptr, blink::ServiceWorkerStatusCode::kOk);
  registry()->NotifyDoneInstallingRegistration(
      live_registration2.get(), nullptr, blink::ServiceWorkerStatusCode::kOk);
  registry()->NotifyDoneInstallingRegistration(
      live_registration3.get(), nullptr, blink::ServiceWorkerStatusCode::kOk);

  EXPECT_EQ(0,
            helper()->quota_manager_proxy()->notify_storage_accessed_count());

  // Find a registration among installed ones.
  EXPECT_EQ(
      blink::ServiceWorkerStatusCode::kOk,
      FindRegistrationForClientUrl(kDocumentUrl, kKey, found_registration));
  EXPECT_EQ(1,
            helper()->quota_manager_proxy()->notify_storage_accessed_count());
  EXPECT_EQ(live_registration2, found_registration);
}

// Tests that fields of ServiceWorkerRegistrationInfo are filled correctly.
TEST_F(ServiceWorkerRegistryTest, RegistrationInfoFields) {
  const GURL kScope("http://www.example.com/scope/");
  const blink::StorageKey kKey(url::Origin::Create(kScope));
  const GURL kScript("http://www.example.com/script1.js");
  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope, kScript,
                                                kKey,
                                                /*resource_id=*/1);

  // Set some fields to check ServiceWorkerStorage serializes/deserializes
  // these fields correctly.
  registration->SetUpdateViaCache(
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  registration->EnableNavigationPreload(true);
  registration->SetNavigationPreloadHeader("header");

  registry()->NotifyInstallingRegistration(registration.get());
  ASSERT_EQ(StoreRegistration(registration, registration->waiting_version()),
            blink::ServiceWorkerStatusCode::kOk);

  std::vector<ServiceWorkerRegistrationInfo> all_registrations;
  EXPECT_EQ(GetAllRegistrationsInfos(&all_registrations),
            blink::ServiceWorkerStatusCode::kOk);
  ASSERT_EQ(all_registrations.size(), 1UL);

  ServiceWorkerRegistrationInfo info = all_registrations[0];
  EXPECT_EQ(info.scope, registration->scope());
  EXPECT_EQ(info.key, registration->key());
  EXPECT_EQ(info.update_via_cache, registration->update_via_cache());
  EXPECT_EQ(info.registration_id, registration->id());
  EXPECT_EQ(info.navigation_preload_enabled,
            registration->navigation_preload_state().enabled);
  EXPECT_EQ(info.navigation_preload_header_length,
            registration->navigation_preload_state().header.size());
}

TEST_F(ServiceWorkerRegistryTest, OriginTrialsAbsentEntryAndEmptyEntry) {
  const GURL origin1("http://www1.example.com");
  const blink::StorageKey key1(url::Origin::Create(origin1));
  const GURL scope1("http://www1.example.com/foo/");
  const GURL script1(origin1.spec() + "/script.js");
  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> resources1;
  resources1.push_back(CreateResourceRecord(1, script1, 100));
  storage::mojom::ServiceWorkerRegistrationDataPtr data1 =
      CreateRegistrationData(
          /*registration_id=*/100,
          /*version_id=*/1000,
          /*scope=*/scope1,
          /*key=*/key1,
          /*script_url=*/script1, resources1);
  // Don't set origin_trial_tokens to simulate old database entry.
  StoreRegistrationData(std::move(data1), std::move(resources1));

  const GURL origin2("http://www2.example.com");
  const blink::StorageKey key2(url::Origin::Create(origin2));
  const GURL scope2("http://www2.example.com/foo/");
  const GURL script2(origin2.spec() + "/script.js");
  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> resources2;
  resources2.push_back(CreateResourceRecord(2, script2, 200));
  storage::mojom::ServiceWorkerRegistrationDataPtr data2 =
      CreateRegistrationData(
          /*registration_id=*/200,
          /*version_id=*/2000,
          /*scope=*/scope2,
          /*key=*/key2,
          /*script_url=*/script2, resources2);
  // Set empty origin_trial_tokens.
  data2->origin_trial_tokens = blink::TrialTokenValidator::FeatureToTokensMap();
  StoreRegistrationData(std::move(data2), std::move(resources2));

  scoped_refptr<ServiceWorkerRegistration> found_registration;

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForClientUrl(scope1, key1, found_registration));
  ASSERT_TRUE(found_registration->active_version());
  // origin_trial_tokens must be unset.
  EXPECT_FALSE(found_registration->active_version()->origin_trial_tokens());

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForClientUrl(scope2, key2, found_registration));
  ASSERT_TRUE(found_registration->active_version());
  // Empty origin_trial_tokens must exist.
  ASSERT_TRUE(found_registration->active_version()->origin_trial_tokens());
  EXPECT_TRUE(
      found_registration->active_version()->origin_trial_tokens()->empty());
}

// Tests loading a registration that has no navigation preload state.
TEST_F(ServiceWorkerRegistryTest, AbsentNavigationPreloadState) {
  const GURL origin1("http://www1.example.com");
  const blink::StorageKey key1(url::Origin::Create(origin1));
  const GURL scope1("http://www1.example.com/foo/");
  const GURL script1(origin1.spec() + "/script.js");
  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> resources1;
  resources1.push_back(CreateResourceRecord(1, script1, 100));
  storage::mojom::ServiceWorkerRegistrationDataPtr data1 =
      CreateRegistrationData(
          /*registration_id=*/100,
          /*version_id=*/1000,
          /*scope=*/scope1,
          /*key=*/key1,
          /*script_url=*/script1, resources1);
  // Don't set navigation preload state to simulate old database entry.
  StoreRegistrationData(std::move(data1), std::move(resources1));

  scoped_refptr<ServiceWorkerRegistration> found_registration;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForClientUrl(scope1, key1, found_registration));
  const blink::mojom::NavigationPreloadState& registration_state =
      found_registration->navigation_preload_state();
  EXPECT_FALSE(registration_state.enabled);
  EXPECT_EQ("true", registration_state.header);
  ASSERT_TRUE(found_registration->active_version());
  const blink::mojom::NavigationPreloadState& state =
      found_registration->active_version()->navigation_preload_state();
  EXPECT_FALSE(state.enabled);
  EXPECT_EQ("true", state.header);
}

// Tests loading a registration with an enabled navigation preload state, as
// well as a custom header value.
TEST_F(ServiceWorkerRegistryTest, EnabledNavigationPreloadState) {
  const GURL kScope("https://valid.example.com/scope");
  const blink::StorageKey kKey(url::Origin::Create(kScope));
  const GURL kScript("https://valid.example.com/script.js");
  const std::string kHeaderValue("custom header value");

  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope, kScript,
                                                kKey,
                                                /*resource_id=*/1);
  ServiceWorkerVersion* version = registration->waiting_version();
  version->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration->SetActiveVersion(version);
  registration->EnableNavigationPreload(true);
  registration->SetNavigationPreloadHeader(kHeaderValue);

  ASSERT_EQ(StoreRegistration(registration, version),
            blink::ServiceWorkerStatusCode::kOk);

  // Simulate browser shutdown and restart.
  registration = nullptr;
  version = nullptr;
  SimulateRestart();

  scoped_refptr<ServiceWorkerRegistration> found_registration;
  EXPECT_EQ(FindRegistrationForClientUrl(kScope, kKey, found_registration),
            blink::ServiceWorkerStatusCode::kOk);
  const blink::mojom::NavigationPreloadState& registration_state =
      found_registration->navigation_preload_state();
  EXPECT_TRUE(registration_state.enabled);
  EXPECT_EQ(registration_state.header, kHeaderValue);
  ASSERT_TRUE(found_registration->active_version());
  const blink::mojom::NavigationPreloadState& state =
      found_registration->active_version()->navigation_preload_state();
  EXPECT_TRUE(state.enabled);
  EXPECT_EQ(state.header, kHeaderValue);
}

// Tests storing the script response time for DevTools.
TEST_F(ServiceWorkerRegistryTest, ScriptResponseTime) {
  // Make a registration.
  const GURL kScope("https://example.com/scope");
  const blink::StorageKey kKey(url::Origin::Create(kScope));
  const GURL kScript("https://example.com/script.js");
  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope, kScript,
                                                kKey,
                                                /*resource_id=*/1);
  ServiceWorkerVersion* version = registration->waiting_version();

  // Give it a main script response info.
  network::mojom::URLResponseHead response_head;
  response_head.headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response_head.response_time = base::Time::FromJsTime(19940123);
  version->SetMainScriptResponse(
      std::make_unique<ServiceWorkerVersion::MainScriptResponse>(
          response_head));
  EXPECT_TRUE(version->main_script_response_);
  EXPECT_EQ(response_head.response_time,
            version->script_response_time_for_devtools_);
  EXPECT_EQ(response_head.response_time,
            version->GetInfo().script_response_time);

  // Store the registration.
  EXPECT_EQ(StoreRegistration(registration, version),
            blink::ServiceWorkerStatusCode::kOk);

  // Simulate browser shutdown and restart.
  registration = nullptr;
  version = nullptr;
  SimulateRestart();

  // Read the registration. The main script's response time should be gettable.
  scoped_refptr<ServiceWorkerRegistration> found_registration;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForClientUrl(kScope, kKey, found_registration));
  ASSERT_TRUE(found_registration);
  auto* waiting_version = found_registration->waiting_version();
  ASSERT_TRUE(waiting_version);
  EXPECT_FALSE(waiting_version->main_script_response_);
  EXPECT_EQ(response_head.response_time,
            waiting_version->script_response_time_for_devtools_);
  EXPECT_EQ(response_head.response_time,
            waiting_version->GetInfo().script_response_time);
}

// Tests loading a registration with a disabled navigation preload
// state.
TEST_F(ServiceWorkerRegistryTest, DisabledNavigationPreloadState) {
  const GURL kScope("https://valid.example.com/scope");
  const blink::StorageKey kKey(url::Origin::Create(kScope));
  const GURL kScript("https://valid.example.com/script.js");
  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope, kScript,
                                                kKey,
                                                /*resource_id=*/1);
  ServiceWorkerVersion* version = registration->waiting_version();
  version->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration->SetActiveVersion(version);
  registration->EnableNavigationPreload(false);

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StoreRegistration(registration, version));

  // Simulate browser shutdown and restart.
  registration = nullptr;
  version = nullptr;
  SimulateRestart();

  scoped_refptr<ServiceWorkerRegistration> found_registration;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForClientUrl(kScope, kKey, found_registration));
  const blink::mojom::NavigationPreloadState& registration_state =
      found_registration->navigation_preload_state();
  EXPECT_FALSE(registration_state.enabled);
  EXPECT_EQ("true", registration_state.header);
  ASSERT_TRUE(found_registration->active_version());
  const blink::mojom::NavigationPreloadState& state =
      found_registration->active_version()->navigation_preload_state();
  EXPECT_FALSE(state.enabled);
  EXPECT_EQ("true", state.header);
}

// Tests that storage policy changes are observed.
TEST_F(ServiceWorkerRegistryTest, StoragePolicyChange) {
  const GURL kScope("http://www.example.com/scope/");
  const GURL kScriptUrl("http://www.example.com/script.js");
  const auto kOrigin(url::Origin::Create(kScope));
  const blink::StorageKey kKey(kOrigin);

  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope, kScriptUrl,
                                                kKey,
                                                /*resource_id=*/1);

  ASSERT_EQ(StoreRegistration(registration, registration->waiting_version()),
            blink::ServiceWorkerStatusCode::kOk);
  EXPECT_FALSE(registry()->ShouldPurgeOnShutdownForTesting(kKey));

  {
    // Update storage policy to mark the origin should be purged on shutdown.
    special_storage_policy()->AddSessionOnly(kOrigin.GetURL());
    special_storage_policy()->NotifyPolicyChanged();
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_TRUE(registry()->ShouldPurgeOnShutdownForTesting(kKey));
}

// Tests that callbacks of storage operations are always called even when the
// remote storage is disconnected.
TEST_F(ServiceWorkerRegistryTest, RemoteStorageDisconnection) {
  const GURL kScope("http://www.example.com/scope/");
  const GURL kScriptUrl("http://www.example.com/script.js");
  const auto kOrigin(url::Origin::Create(kScope));
  const blink::StorageKey kKey(kOrigin);

  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope, kScriptUrl,
                                                kKey,
                                                /*resource_id=*/1);

  ASSERT_EQ(StoreRegistration(registration, registration->waiting_version()),
            blink::ServiceWorkerStatusCode::kOk);

  GetStorageUsageForStorageKeyResult result =
      GetStorageUsageForStorageKey(kKey);
  ASSERT_EQ(result.status, blink::ServiceWorkerStatusCode::kOk);

  // This will disconnect mojo connection of the remote storage.
  helper()->SimulateStorageRestartForTesting();

  // The connection should be recovered and inflight calls should be retried
  // automatically.
  result = GetStorageUsageForStorageKey(kKey);
  ASSERT_EQ(result.status, blink::ServiceWorkerStatusCode::kOk);
  result = GetStorageUsageForStorageKey(kKey);
  ASSERT_EQ(result.status, blink::ServiceWorkerStatusCode::kOk);
}

// Tests that inflight remote calls are retried after the remote storage is
// restarted.
TEST_F(ServiceWorkerRegistryTest, RetryInflightCalls) {
  EnsureRemoteCallsAreExecuted();

  const GURL kScope1("https://www.example.com/scope/");
  const GURL kScriptUrl1("https://www.example.com/script.js");
  const blink::StorageKey kKey1(url::Origin::Create(kScope1));
  scoped_refptr<ServiceWorkerRegistration> registration1 =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope1, kScriptUrl1,
                                                kKey1,
                                                /*resource_id=*/1);

  const GURL kScope2("https://www2.example.com/scope/foo");
  const GURL kScriptUrl2("https://www2.example.com/foo/script.js");
  const blink::StorageKey kKey2(url::Origin::Create(kScope2));
  scoped_refptr<ServiceWorkerRegistration> registration2 =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope2, kScriptUrl2,
                                                kKey2,
                                                /*resource_id=*/2);

  // Store two registrations. Restart the remote storage several times.
  {
    helper()->SimulateStorageRestartForTesting();

    base::RunLoop loop1;
    registry()->StoreRegistration(
        registration1.get(), registration1->waiting_version(),
        base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
          EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kOk);
          loop1.Quit();
        }));

    helper()->SimulateStorageRestartForTesting();

    base::RunLoop loop2;
    registry()->StoreRegistration(
        registration2.get(), registration2->waiting_version(),
        base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
          EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kOk);
          loop2.Quit();
        }));

    EXPECT_EQ(inflight_call_count(), 2U);
    helper()->SimulateStorageRestartForTesting();

    loop1.Run();
    loop2.Run();
    EXPECT_EQ(inflight_call_count(), 0U);
  }

  // Get registered storage keys.
  {
    base::RunLoop loop;
    registry()->GetRegisteredStorageKeys(base::BindLambdaForTesting(
        [&](const std::vector<blink::StorageKey>& storage_keys) {
          EXPECT_THAT(storage_keys,
                      testing::UnorderedElementsAreArray({kKey1, kKey2}));
          loop.Quit();
        }));

    EXPECT_EQ(inflight_call_count(), 1U);
    helper()->SimulateStorageRestartForTesting();

    loop.Run();
    EXPECT_EQ(inflight_call_count(), 0U);
  }

  // Finding registrations stored in the previous block.
  {
    base::RunLoop loop1;
    registry()->FindRegistrationForClientUrl(
        kScope1, kKey1,
        base::BindLambdaForTesting(
            [&](blink::ServiceWorkerStatusCode status,
                scoped_refptr<ServiceWorkerRegistration> found_registration) {
              EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kOk);
              EXPECT_EQ(found_registration, registration1);
              loop1.Quit();
            }));

    base::RunLoop loop2;
    const GURL kNotInScope("http://www.example.com/not-in-scope");
    registry()->FindRegistrationForScope(
        kNotInScope, blink::StorageKey(url::Origin::Create(kNotInScope)),
        base::BindLambdaForTesting(
            [&](blink::ServiceWorkerStatusCode status,
                scoped_refptr<ServiceWorkerRegistration> found_registration) {
              EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kErrorNotFound);
              EXPECT_EQ(found_registration, nullptr);
              loop2.Quit();
            }));

    EXPECT_EQ(inflight_call_count(), 2U);
    helper()->SimulateStorageRestartForTesting();

    loop1.Run();
    loop2.Run();
    EXPECT_EQ(inflight_call_count(), 0U);
  }

  // Get both of the registrations by these APIs.
  {
    base::RunLoop loop1;
    registry()->GetRegistrationsForStorageKey(
        kKey1,
        base::BindLambdaForTesting(
            [&](blink::ServiceWorkerStatusCode status,
                const std::vector<scoped_refptr<ServiceWorkerRegistration>>&
                    registrations) {
              EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kOk);
              EXPECT_EQ(registrations.size(), 1U);
              loop1.Quit();
            }));

    base::RunLoop loop2;
    registry()->GetAllRegistrationsInfos(base::BindLambdaForTesting(
        [&](blink::ServiceWorkerStatusCode status,
            const std::vector<ServiceWorkerRegistrationInfo>& registrations) {
          EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kOk);
          EXPECT_EQ(registrations.size(), 2U);
          loop2.Quit();
        }));

    EXPECT_EQ(inflight_call_count(), 2U);
    helper()->SimulateStorageRestartForTesting();

    loop1.Run();
    loop2.Run();
    EXPECT_EQ(inflight_call_count(), 0U);
  }

  // Delete `registrations` from the storage.
  {
    base::RunLoop loop;
    registry()->DeleteRegistration(
        registration2, kKey2,
        base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
          EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kOk);
          loop.Quit();
        }));

    EXPECT_EQ(inflight_call_count(), 1U);
    helper()->SimulateStorageRestartForTesting();

    loop.Run();
    EXPECT_EQ(inflight_call_count(), 0U);
  }

  // Update fields of `registration1` in the storage.
  {
    base::RunLoop loop1;
    registry()->UpdateToActiveState(
        registration1->id(), kKey1,
        base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
          EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kOk);
          loop1.Quit();
        }));

    base::RunLoop loop2;
    registry()->UpdateLastUpdateCheckTime(
        registration1->id(), kKey1, base::Time::Now(),
        base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
          EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kOk);
          loop2.Quit();
        }));

    base::RunLoop loop3;
    registry()->UpdateNavigationPreloadEnabled(
        registration1->id(), kKey1, /*enable=*/true,
        base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
          EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kOk);
          loop3.Quit();
        }));

    base::RunLoop loop4;
    registry()->UpdateNavigationPreloadHeader(
        registration1->id(), kKey1, "header",
        base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
          EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kOk);
          loop4.Quit();
        }));

    EXPECT_EQ(inflight_call_count(), 4U);
    helper()->SimulateStorageRestartForTesting();

    loop1.Run();
    loop2.Run();
    loop3.Run();
    loop4.Run();
    EXPECT_EQ(inflight_call_count(), 0U);
  }
}

// Tests that FindRegistrationForId methods are retried after the remote storage
// is restarted. Separated from other FindRegistration method tests because
// these methods skip remote calls when live registrations are alive.
TEST_F(ServiceWorkerRegistryTest, RetryInflightCalls_FindRegistrationForId) {
  // Prerequisite: Store two registrations.
  const GURL origin1("https://www.example.com");
  const blink::StorageKey key1(url::Origin::Create(origin1));
  const GURL scope1("https://www.example.com/foo/");
  const GURL script1(origin1.spec() + "/script.js");
  const int64_t registration_id1 = 1;
  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> resources1;
  resources1.push_back(CreateResourceRecord(1, script1, 100));
  storage::mojom::ServiceWorkerRegistrationDataPtr data1 =
      CreateRegistrationData(registration_id1,
                             /*version_id=*/1000,
                             /*scope=*/scope1,
                             /*key=*/key1,
                             /*script_url=*/script1, resources1);
  StoreRegistrationData(std::move(data1), std::move(resources1));

  const GURL origin2("https://www.example.com");
  const blink::StorageKey key2(url::Origin::Create(origin2));
  const GURL scope2("https://www.example.com/bar/");
  const GURL script2(origin2.spec() + "/script.js");
  const int64_t registration_id2 = 2;
  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> resources2;
  resources2.push_back(CreateResourceRecord(2, script2, 200));
  storage::mojom::ServiceWorkerRegistrationDataPtr data2 =
      CreateRegistrationData(registration_id2,
                             /*version_id=*/2000,
                             /*scope=*/scope2,
                             /*key=*/key2,
                             /*script_url=*/script2, resources2);
  StoreRegistrationData(std::move(data2), std::move(resources2));

  base::RunLoop loop1;
  registry()->FindRegistrationForId(
      registration_id1, key1,
      base::BindLambdaForTesting(
          [&](blink::ServiceWorkerStatusCode status,
              scoped_refptr<ServiceWorkerRegistration> found_registration) {
            EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kOk);
            EXPECT_EQ(found_registration->id(), registration_id1);
            loop1.Quit();
          }));

  base::RunLoop loop2;
  registry()->FindRegistrationForIdOnly(
      registration_id2,
      base::BindLambdaForTesting(
          [&](blink::ServiceWorkerStatusCode status,
              scoped_refptr<ServiceWorkerRegistration> found_registration) {
            EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kOk);
            EXPECT_EQ(found_registration->id(), registration_id2);
            loop2.Quit();
          }));

  EXPECT_EQ(inflight_call_count(), 2U);
  helper()->SimulateStorageRestartForTesting();

  loop1.Run();
  loop2.Run();
  EXPECT_EQ(inflight_call_count(), 0U);
}

TEST_F(ServiceWorkerRegistryTest,
       RetryInflightCalls_CreateNewRegistrationAndVersion) {
  EnsureRemoteCallsAreExecuted();

  const GURL kScope("http://www.example.com/scope/");
  const blink::StorageKey kKey(url::Origin::Create(kScope));
  const GURL kScriptUrl("http://www.example.com/script.js");

  scoped_refptr<ServiceWorkerRegistration> registration;

  {
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = kScope;
    storage::QuotaManagerProxySync quota_manager_proxy_sync(
        quota_manager_proxy());

    base::RunLoop loop;
    registry()->CreateNewRegistration(
        std::move(options), kKey,
        base::BindLambdaForTesting(
            [&](scoped_refptr<ServiceWorkerRegistration> new_registration) {
              EXPECT_EQ(new_registration->scope(), kScope);
              registration = new_registration;
              loop.Quit();
            }));

    helper()->SimulateStorageRestartForTesting();
    EXPECT_EQ(inflight_call_count(), 1U);

    loop.Run();
    EXPECT_EQ(inflight_call_count(), 0U);

    // Check default bucket exists.com.
    storage::QuotaErrorOr<storage::BucketInfo> result =
        quota_manager_proxy_sync.GetBucket(
            kKey, storage::kDefaultBucketName,
            blink::mojom::StorageType::kTemporary);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result->name, storage::kDefaultBucketName);
    EXPECT_EQ(result->storage_key, kKey);
    EXPECT_GT(result->id.value(), 0);
  }

  {
    base::RunLoop loop;
    registry()->CreateNewVersion(
        registration, kScriptUrl, blink::mojom::ScriptType::kClassic,
        base::BindLambdaForTesting(
            [&](scoped_refptr<ServiceWorkerVersion> new_version) {
              EXPECT_EQ(new_version->script_url(), kScriptUrl);
              EXPECT_EQ(new_version->registration_id(), registration->id());
              loop.Quit();
            }));

    helper()->SimulateStorageRestartForTesting();
    EXPECT_EQ(inflight_call_count(), 1U);

    loop.Run();
    EXPECT_EQ(inflight_call_count(), 0U);
  }
}

TEST_F(ServiceWorkerRegistryTest, RetryInflightCalls_UserData) {
  // Prerequisite: Store two registrations.
  const GURL kScope1("http://www.example.com/scope/");
  const GURL kScriptUrl1("http://www.example.com/script.js");
  const auto kOrigin1(url::Origin::Create(kScope1));
  const blink::StorageKey kKey1(kOrigin1);
  scoped_refptr<ServiceWorkerRegistration> registration1 =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope1, kScriptUrl1,
                                                kKey1,
                                                /*resource_id=*/1);
  ASSERT_EQ(StoreRegistration(registration1, registration1->waiting_version()),
            blink::ServiceWorkerStatusCode::kOk);

  const GURL kScope2("http://www.example.com/scope/foo");
  const GURL kScriptUrl2("http://www.example.com/fooscript.js");
  const auto kOrigin2(url::Origin::Create(kScope2));
  const blink::StorageKey kKey2(kOrigin2);
  scoped_refptr<ServiceWorkerRegistration> registration2 =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope2, kScriptUrl2,
                                                kKey2,
                                                /*resource_id=*/2);
  ASSERT_EQ(StoreRegistration(registration2, registration2->waiting_version()),
            blink::ServiceWorkerStatusCode::kOk);

  // Store some user data.
  {
    base::RunLoop loop1;
    registry()->StoreUserData(
        registration1->id(), kKey1,
        {{"key1", "value1"}, {"prefixed_key1", "prefixed_value1"}},
        base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
          EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kOk);
          loop1.Quit();
        }));

    base::RunLoop loop2;
    registry()->StoreUserData(
        registration2->id(), kKey2,
        {{"key2", "value2"}, {"prefixed_key2", "prefixed_value2"}},
        base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
          EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kOk);
          loop2.Quit();
        }));

    EXPECT_EQ(inflight_call_count(), 2U);
    helper()->SimulateStorageRestartForTesting();

    loop1.Run();
    loop2.Run();
    EXPECT_EQ(inflight_call_count(), 0U);
  }

  // Tests that get methods for `registration1` work.
  {
    base::RunLoop loop1;
    registry()->GetUserData(
        registration1->id(), {{"key1"}},
        base::BindLambdaForTesting([&](const std::vector<std::string>& values,
                                       blink::ServiceWorkerStatusCode status) {
          EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kOk);
          EXPECT_EQ(values.size(), 1U);
          loop1.Quit();
        }));

    base::RunLoop loop2;
    registry()->GetUserDataByKeyPrefix(
        registration1->id(), "prefixed",
        base::BindLambdaForTesting([&](const std::vector<std::string>& values,
                                       blink::ServiceWorkerStatusCode status) {
          EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kOk);
          EXPECT_EQ(values.size(), 1U);
          loop2.Quit();
        }));

    base::RunLoop loop3;
    registry()->GetUserKeysAndDataByKeyPrefix(
        registration1->id(), "prefixed",
        base::BindLambdaForTesting(
            [&](blink::ServiceWorkerStatusCode status,
                const base::flat_map<std::string, std::string>& user_data) {
              EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kOk);
              EXPECT_EQ(user_data.size(), 1U);
              loop3.Quit();
            }));

    EXPECT_EQ(inflight_call_count(), 3U);
    helper()->SimulateStorageRestartForTesting();

    loop1.Run();
    loop2.Run();
    loop3.Run();
    EXPECT_EQ(inflight_call_count(), 0U);
  }

  // Tests that get methods for all registrations work.
  {
    base::RunLoop loop1;
    registry()->GetUserDataForAllRegistrations(
        "key2",
        base::BindLambdaForTesting(
            [&](const std::vector<std::pair<int64_t, std::string>>& user_data,
                blink::ServiceWorkerStatusCode status) {
              EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kOk);
              EXPECT_EQ(user_data.size(), 1U);
              loop1.Quit();
            }));

    base::RunLoop loop2;
    registry()->GetUserDataForAllRegistrationsByKeyPrefix(
        "prefixed",
        base::BindLambdaForTesting(
            [&](const std::vector<std::pair<int64_t, std::string>>& user_data,
                blink::ServiceWorkerStatusCode status) {
              EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kOk);
              EXPECT_EQ(user_data.size(), 2U);
              loop2.Quit();
            }));

    EXPECT_EQ(inflight_call_count(), 2U);
    helper()->SimulateStorageRestartForTesting();

    loop1.Run();
    loop2.Run();
    EXPECT_EQ(inflight_call_count(), 0U);
  }

  // Tests that clear methods work.
  {
    base::RunLoop loop1;
    registry()->ClearUserData(
        registration1->id(), {{"key1"}},
        base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
          EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kOk);
          loop1.Quit();
        }));

    base::RunLoop loop2;
    registry()->ClearUserDataByKeyPrefixes(
        registration2->id(), {{"key2"}},
        base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
          EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kOk);
          loop2.Quit();
        }));

    base::RunLoop loop3;
    registry()->ClearUserDataForAllRegistrationsByKeyPrefix(
        "prefixed",
        base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
          EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kOk);
          loop3.Quit();
        }));

    EXPECT_EQ(inflight_call_count(), 3U);
    helper()->SimulateStorageRestartForTesting();

    loop1.Run();
    loop2.Run();
    loop3.Run();
    EXPECT_EQ(inflight_call_count(), 0U);
  }
}

TEST_F(ServiceWorkerRegistryTest, RetryInflightCalls_DeleteAndStartOver) {
  EnsureRemoteCallsAreExecuted();

  base::RunLoop loop;
  registry()->DeleteAndStartOver(
      base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
        DCHECK_EQ(status, blink::ServiceWorkerStatusCode::kOk);
        loop.Quit();
      }));

  EXPECT_EQ(inflight_call_count(), 1U);
  helper()->SimulateStorageRestartForTesting();

  base::HistogramTester histogram_tester;
  loop.Run();
  EXPECT_EQ(inflight_call_count(), 0U);
  const size_t kExpectedRetryCountForRecovery = 0;
  const size_t kExpectedSampleCount = 1;
  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.Storage.RetryCountForRecovery",
      kExpectedRetryCountForRecovery, kExpectedSampleCount);
}

TEST_F(ServiceWorkerRegistryTest, RetryInflightCalls_PerformStorageCleanup) {
  EnsureRemoteCallsAreExecuted();

  base::RunLoop loop;
  registry()->PerformStorageCleanup(
      base::BindLambdaForTesting([&]() { loop.Quit(); }));

  EXPECT_EQ(inflight_call_count(), 1U);
  helper()->SimulateStorageRestartForTesting();

  base::HistogramTester histogram_tester;
  loop.Run();
  EXPECT_EQ(inflight_call_count(), 0U);
  const size_t kExpectedRetryCountForRecovery = 0;
  const size_t kExpectedSampleCount = 1;
  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.Storage.RetryCountForRecovery",
      kExpectedRetryCountForRecovery, kExpectedSampleCount);
}

TEST_F(ServiceWorkerRegistryTest, RetryInflightCalls_Disable) {
  EnsureRemoteCallsAreExecuted();

  // This schedules a Disable() remote call.
  registry()->PrepareForDeleteAndStartOver();

  EXPECT_EQ(inflight_call_count(), 1U);
  helper()->SimulateStorageRestartForTesting();

  base::HistogramTester histogram_tester;
  EnsureRemoteCallsAreExecuted();
  EXPECT_EQ(inflight_call_count(), 0U);
  const size_t kExpectedRetryCountForRecovery = 0;
  const size_t kExpectedSampleCount = 1;
  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.Storage.RetryCountForRecovery",
      kExpectedRetryCountForRecovery, kExpectedSampleCount);
}

// Similar to `StoragePolicyChange` test but restart the remote storage to make
// sure ApplyPolicyUpdates() is retried.
TEST_F(ServiceWorkerRegistryTest, RetryInflightCalls_ApplyPolicyUpdates) {
  EnsureRemoteCallsAreExecuted();

  const GURL kScope("http://www.example.com/scope/");
  const GURL kScriptUrl("http://www.example.com/script.js");
  const auto kOrigin(url::Origin::Create(kScope));
  const blink::StorageKey kKey(kOrigin);

  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateServiceWorkerRegistrationAndVersion(context(), kScope, kScriptUrl,
                                                kKey,
                                                /*resource_id=*/1);

  ASSERT_EQ(StoreRegistration(registration, registration->waiting_version()),
            blink::ServiceWorkerStatusCode::kOk);
  EXPECT_FALSE(registry()->ShouldPurgeOnShutdownForTesting(kKey));

  // Update storage policy to mark the origin should be purged on shutdown.
  special_storage_policy()->AddSessionOnly(kOrigin.GetURL());
  special_storage_policy()->NotifyPolicyChanged();

  helper()->SimulateStorageRestartForTesting();

  EnsureRemoteCallsAreExecuted();
  // All Mojo calls must be done at this point.
  EXPECT_EQ(inflight_call_count(), 0U);

  EXPECT_TRUE(registry()->ShouldPurgeOnShutdownForTesting(kKey));
}

// Regression test for https://crbug.com/1165784.
// Tests that callbacks of ServiceWorkerRegistry are always called. Calls
// ServiceWorkerRegistry methods and destroys the instance immediately by
// simulating restarts.
TEST_F(ServiceWorkerRegistryTest, DestroyRegistryDuringInflightCall) {
  {
    base::RunLoop loop;
    registry()->GetRegisteredStorageKeys(base::BindLambdaForTesting(
        [&](const std::vector<blink::StorageKey>& storage_keys) {
          EXPECT_TRUE(storage_keys.empty());
          loop.Quit();
        }));
    SimulateRestart();
    loop.Run();
  }

  {
    base::RunLoop loop;
    registry()->GetStorageUsageForStorageKey(
        blink::StorageKey::CreateFromStringForTesting("https://example.com/"),
        base::BindLambdaForTesting(
            [&](blink::ServiceWorkerStatusCode status, int64_t usage) {
              EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kErrorFailed);
              loop.Quit();
            }));
    SimulateRestart();
    loop.Run();
  }

  {
    base::RunLoop loop;
    registry()->PerformStorageCleanup(loop.QuitClosure());
    SimulateRestart();
    loop.Run();
  }
}

TEST_F(ServiceWorkerRegistryTest,
       DestroyRegistryDuringInflightCall_StoreUserData) {
  base::RunLoop loop;
  registry()->StoreUserData(
      /*registration_id=*/1,
      blink::StorageKey::CreateFromStringForTesting("https://example.com/"),
      {{"key", "value"}},
      base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
        EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kErrorFailed);
        loop.Quit();
      }));
  SimulateRestart();
  loop.Run();
}

TEST_F(ServiceWorkerRegistryTest,
       DestroyRegistryDuringInflightCall_ClearUserData) {
  base::RunLoop loop;
  registry()->ClearUserData(
      /*registration_id=*/1, {{"key"}},
      base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
        EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kErrorFailed);
        loop.Quit();
      }));
  SimulateRestart();
  loop.Run();
}

TEST_F(ServiceWorkerRegistryTest,
       DestroyRegistryDuringInflightCall_ClearUserDataByKeyPrefixes) {
  base::RunLoop loop;
  registry()->ClearUserDataByKeyPrefixes(
      /*registration_id=*/1, {{"prefix"}},
      base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
        EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kErrorFailed);
        loop.Quit();
      }));
  SimulateRestart();
  loop.Run();
}

TEST_F(
    ServiceWorkerRegistryTest,
    DestroyRegistryDuringInflightCall_ClearUserDataForAllRegistrationsByKeyPrefix) {
  base::RunLoop loop;
  registry()->ClearUserDataForAllRegistrationsByKeyPrefix(
      "prefix",
      base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
        EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kErrorFailed);
        loop.Quit();
      }));
  SimulateRestart();
  loop.Run();
}

TEST_F(ServiceWorkerRegistryTest,
       DestroyRegistryDuringInflightCall_GetUserDataForAllRegistrations) {
  base::RunLoop loop;
  registry()->GetUserDataForAllRegistrations(
      "key",
      base::BindLambdaForTesting(
          [&](const std::vector<std::pair<int64_t, std::string>>& user_data,
              blink::ServiceWorkerStatusCode status) {
            EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kErrorFailed);
            loop.Quit();
          }));
  SimulateRestart();
  loop.Run();
}

TEST_F(
    ServiceWorkerRegistryTest,
    DestroyRegistryDuringInflightCall_GetUserDataForAllRegistrationsByKeyPrefix) {
  base::RunLoop loop;
  registry()->GetUserDataForAllRegistrationsByKeyPrefix(
      "prefix",
      base::BindLambdaForTesting(
          [&](const std::vector<std::pair<int64_t, std::string>>& user_data,
              blink::ServiceWorkerStatusCode status) {
            EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kErrorFailed);
            loop.Quit();
          }));
  SimulateRestart();
  loop.Run();
}

class ServiceWorkerRegistryOriginTrialsTest : public ServiceWorkerRegistryTest {
 private:
  blink::ScopedTestOriginTrialPolicy origin_trial_policy_;
};

TEST_F(ServiceWorkerRegistryOriginTrialsTest, FromMainScript) {
  const GURL kScope("https://valid.example.com/scope");
  const blink::StorageKey kKey(url::Origin::Create(kScope));
  const GURL kScript("https://valid.example.com/script.js");
  const int64_t kRegistrationId = 1;
  const int64_t kVersionId = 1;
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = kScope;
  scoped_refptr<ServiceWorkerRegistration> registration =
      new ServiceWorkerRegistration(options, kKey, kRegistrationId,
                                    context()->AsWeakPtr());
  scoped_refptr<ServiceWorkerVersion> version = new ServiceWorkerVersion(
      registration.get(), kScript, blink::mojom::ScriptType::kClassic,
      kVersionId,
      mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
      context()->AsWeakPtr());

  network::mojom::URLResponseHead response_head;
  response_head.ssl_info = net::SSLInfo();
  response_head.ssl_info->cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  EXPECT_TRUE(response_head.ssl_info->is_valid());
  // SSL3 TLS_DHE_RSA_WITH_AES_256_CBC_SHA
  response_head.ssl_info->connection_status = 0x300039;

  const std::string kHTTPHeaderLine("HTTP/1.1 200 OK\n\n");
  const std::string kOriginTrial("Origin-Trial");
  // Token for Feature1 which expires 2033-05-18.
  // generate_token.py valid.example.com Feature1 --expire-timestamp=2000000000
  // TODO(horo): Generate this sample token during the build.
  const std::string kFeature1Token(
      "AtiUXksymWhTv5ipBE7853JytiYb0RMj3wtEBjqu3PeufQPwV1oEaNjHt4R/oEBfcK0UiWlA"
      "P2b9BE2/eThqcAYAAABYeyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5leGFtcGxlLmNvbTo0"
      "NDMiLCAiZmVhdHVyZSI6ICJGZWF0dXJlMSIsICJleHBpcnkiOiAyMDAwMDAwMDAwfQ==");
  // Token for Feature2 which expires 2033-05-18.
  // generate_token.py valid.example.com Feature2 --expire-timestamp=2000000000
  // TODO(horo): Generate this sample token during the build.
  const std::string kFeature2Token1(
      "ApmHVC6Dpez0KQNBy13o6cGuoB5AgzOLN0keQMyAN5mjebCwR0MA8/IyjKQIlyom2RuJVg/u"
      "LmnqEpldfewkbA8AAABYeyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5leGFtcGxlLmNvbTo0"
      "NDMiLCAiZmVhdHVyZSI6ICJGZWF0dXJlMiIsICJleHBpcnkiOiAyMDAwMDAwMDAwfQ==");
  // Token for Feature2 which expires 2036-07-18.
  // generate_token.py valid.example.com Feature2 --expire-timestamp=2100000000
  // TODO(horo): Generate this sample token during the build.
  const std::string kFeature2Token2(
      "AmV2SSxrYstE2zSwZToy7brAbIJakd146apC/6+VDflLmc5yDfJlHGILe5+ZynlcliG7clOR"
      "fHhXCzS5Lh1v4AAAAABYeyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5leGFtcGxlLmNvbTo0"
      "NDMiLCAiZmVhdHVyZSI6ICJGZWF0dXJlMiIsICJleHBpcnkiOiAyMTAwMDAwMDAwfQ==");
  // Token for Feature3 which expired 2001-09-09.
  // generate_token.py valid.example.com Feature3 --expire-timestamp=1000000000
  const std::string kFeature3ExpiredToken(
      "AtSAc03z4qvid34W4MHMxyRFUJKlubZ+P5cs5yg6EiBWcagVbnm5uBgJMJN34pag7D5RywGV"
      "ol2RFf+4Sdm1hQ4AAABYeyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5leGFtcGxlLmNvbTo0"
      "NDMiLCAiZmVhdHVyZSI6ICJGZWF0dXJlMyIsICJleHBpcnkiOiAxMDAwMDAwMDAwfQ==");
  response_head.headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_head.headers->AddHeader(kOriginTrial, kFeature1Token);
  response_head.headers->AddHeader(kOriginTrial, kFeature2Token1);
  response_head.headers->AddHeader(kOriginTrial, kFeature2Token2);
  response_head.headers->AddHeader(kOriginTrial, kFeature3ExpiredToken);
  version->SetMainScriptResponse(
      std::make_unique<ServiceWorkerVersion::MainScriptResponse>(
          response_head));
  ASSERT_TRUE(version->origin_trial_tokens());
  const blink::TrialTokenValidator::FeatureToTokensMap& tokens =
      *version->origin_trial_tokens();
  ASSERT_EQ(2UL, tokens.size());
  ASSERT_EQ(1UL, tokens.at("Feature1").size());
  EXPECT_EQ(kFeature1Token, tokens.at("Feature1")[0]);
  ASSERT_EQ(2UL, tokens.at("Feature2").size());
  EXPECT_EQ(kFeature2Token1, tokens.at("Feature2")[0]);
  EXPECT_EQ(kFeature2Token2, tokens.at("Feature2")[1]);

  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> records;
  records.push_back(
      storage::mojom::ServiceWorkerResourceRecord::New(1, kScript, 100));
  version->script_cache_map()->SetResources(records);
  version->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  version->SetStatus(ServiceWorkerVersion::INSTALLED);
  registration->SetActiveVersion(version);

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StoreRegistration(registration, version));
  // Simulate browser shutdown and restart.
  registration = nullptr;
  version = nullptr;
  SimulateRestart();

  scoped_refptr<ServiceWorkerRegistration> found_registration;
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationForClientUrl(kScope, kKey, found_registration));
  ASSERT_TRUE(found_registration->active_version());
  const blink::TrialTokenValidator::FeatureToTokensMap& found_tokens =
      *found_registration->active_version()->origin_trial_tokens();
  ASSERT_EQ(2UL, found_tokens.size());
  ASSERT_EQ(1UL, found_tokens.at("Feature1").size());
  EXPECT_EQ(kFeature1Token, found_tokens.at("Feature1")[0]);
  ASSERT_EQ(2UL, found_tokens.at("Feature2").size());
  EXPECT_EQ(kFeature2Token1, found_tokens.at("Feature2")[0]);
  EXPECT_EQ(kFeature2Token2, found_tokens.at("Feature2")[1]);
}

class ServiceWorkerRegistryResourceTest : public ServiceWorkerRegistryTest {
 public:
  void SetUp() override {
    ServiceWorkerRegistryTest::SetUp();

    scope_ = GURL("http://www.test.not/scope/");
    key_ = blink::StorageKey(url::Origin::Create(scope_));
    script_ = GURL("http://www.test.not/script.js");
    import_ = GURL("http://www.test.not/import.js");
    document_url_ = GURL("http://www.test.not/scope/document.html");
    resource_id1_ = GetNewResourceIdSync(storage_control());
    resource_id2_ = GetNewResourceIdSync(storage_control());
    resource_id1_size_ = 239193;
    resource_id2_size_ = 59923;

    // Cons up a new registration+version with two script resources.
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = scope_;
    registration_ =
        CreateNewServiceWorkerRegistration(registry(), options, key_);
    scoped_refptr<ServiceWorkerVersion> version = CreateNewServiceWorkerVersion(
        registry(), registration_.get(), script_, options.type);
    version->set_fetch_handler_existence(
        ServiceWorkerVersion::FetchHandlerExistence::DOES_NOT_EXIST);
    version->SetStatus(ServiceWorkerVersion::INSTALLED);

    std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> resources;
    resources.push_back(
        CreateResourceRecord(resource_id1_, script_, resource_id1_size_));
    resources.push_back(
        CreateResourceRecord(resource_id2_, import_, resource_id2_size_));
    version->script_cache_map()->SetResources(resources);

    registration_->SetWaitingVersion(version);

    registration_id_ = registration_->id();
    version_id_ = version->version_id();

    // Add the resources ids to the uncommitted list.
    blink::StorageKey key(url::Origin::Create(scope_));
    registry()->StoreUncommittedResourceId(resource_id1_, key);
    registry()->StoreUncommittedResourceId(resource_id2_, key);
    EnsureRemoteCallsAreExecuted();

    std::vector<int64_t> verify_ids = GetUncommittedResourceIds();
    EXPECT_EQ(2u, verify_ids.size());

    // And dump something in the disk cache for them.
    WriteBasicResponse(storage_control(), resource_id1_);
    WriteBasicResponse(storage_control(), resource_id2_);
    EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id1_, true));
    EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id2_, true));

    // Storing the registration/version should take the resources ids out
    // of the uncommitted list.
    EXPECT_EQ(
        blink::ServiceWorkerStatusCode::kOk,
        StoreRegistration(registration_, registration_->waiting_version()));
    verify_ids = GetUncommittedResourceIds();
    EXPECT_TRUE(verify_ids.empty());
  }

  std::vector<int64_t> GetUncommittedResourceIds() {
    std::vector<int64_t> ids;
    base::RunLoop loop;
    storage_control()->GetUncommittedResourceIdsForTest(
        base::BindLambdaForTesting(
            [&](storage::mojom::ServiceWorkerDatabaseStatus status,
                const std::vector<int64_t>& resource_ids) {
              EXPECT_EQ(status,
                        storage::mojom::ServiceWorkerDatabaseStatus::kOk);
              ids = resource_ids;
              loop.Quit();
            }));
    loop.Run();
    return ids;
  }

 protected:
  GURL scope_;
  blink::StorageKey key_;
  GURL script_;
  GURL import_;
  GURL document_url_;
  int64_t registration_id_;
  int64_t version_id_;
  int64_t resource_id1_;
  uint64_t resource_id1_size_;
  int64_t resource_id2_;
  uint64_t resource_id2_size_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
};

TEST_F(ServiceWorkerRegistryResourceTest,
       WriteMetadataWithServiceWorkerResponseMetadataWriter) {
  const char kMetadata1[] = "Test metadata";
  const char kMetadata2[] = "small";
  int64_t new_resource_id_ = GetNewResourceIdSync(storage_control());
  // Writing metadata to nonexistent resoirce ID must fail.
  EXPECT_GE(0, WriteResponseMetadata(storage_control(), new_resource_id_,
                                     kMetadata1));

  // Check metadata is written.
  EXPECT_EQ(
      static_cast<int>(strlen(kMetadata1)),
      WriteResponseMetadata(storage_control(), resource_id1_, kMetadata1));
  EXPECT_TRUE(
      VerifyResponseMetadata(storage_control(), resource_id1_, kMetadata1));
  EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id1_, true));

  // Check metadata is written and truncated.
  EXPECT_EQ(
      static_cast<int>(strlen(kMetadata2)),
      WriteResponseMetadata(storage_control(), resource_id1_, kMetadata2));
  EXPECT_TRUE(
      VerifyResponseMetadata(storage_control(), resource_id1_, kMetadata2));
  EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id1_, true));

  // Check metadata is deleted.
  EXPECT_EQ(0, WriteResponseMetadata(storage_control(), resource_id1_, ""));
  EXPECT_FALSE(VerifyResponseMetadata(storage_control(), resource_id1_, ""));
  EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id1_, true));
}

TEST_F(ServiceWorkerRegistryResourceTest,
       WriteMetadataWithServiceWorkerScriptCacheMap) {
  const char kMetadata1[] = "Test metadata";
  const char kMetadata2[] = "small";
  ServiceWorkerVersion* version = registration_->waiting_version();
  EXPECT_TRUE(version);

  // Writing metadata to nonexistent URL must fail.
  EXPECT_GE(0,
            WriteMetadata(version, GURL("http://www.test.not/nonexistent.js"),
                          kMetadata1));
  // Clearing metadata of nonexistent URL must fail.
  EXPECT_GE(0,
            ClearMetadata(version, GURL("http://www.test.not/nonexistent.js")));

  // Check metadata is written.
  EXPECT_EQ(static_cast<int>(strlen(kMetadata1)),
            WriteMetadata(version, script_, kMetadata1));
  EXPECT_TRUE(
      VerifyResponseMetadata(storage_control(), resource_id1_, kMetadata1));
  EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id1_, true));

  // Check metadata is written and truncated.
  EXPECT_EQ(static_cast<int>(strlen(kMetadata2)),
            WriteMetadata(version, script_, kMetadata2));
  EXPECT_TRUE(
      VerifyResponseMetadata(storage_control(), resource_id1_, kMetadata2));
  EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id1_, true));

  // Check metadata is deleted.
  EXPECT_EQ(0, ClearMetadata(version, script_));
  EXPECT_FALSE(VerifyResponseMetadata(storage_control(), resource_id1_, ""));
  EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id1_, true));
}

TEST_F(ServiceWorkerRegistryResourceTest, DeleteRegistration_NoLiveVersion) {
  // Deleting the registration should result in the resources being added to the
  // purgeable list and then doomed in the disk cache and removed from that
  // list.
  base::RunLoop loop;
  storage_control()->SetPurgingCompleteCallbackForTest(loop.QuitClosure());
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            DeleteRegistration(registration_,
                               blink::StorageKey(url::Origin::Create(scope_))));
  // At this point registration_->waiting_version() has a remote reference, so
  // the resources should be in the purgeable list.
  EXPECT_EQ(2u, GetPurgeableResourceIds().size());

  registration_->SetWaitingVersion(nullptr);
  loop.Run();

  // registration_->waiting_version() is cleared. The resources should be
  // purged at this point.
  EXPECT_TRUE(GetPurgeableResourceIds().empty());
  EXPECT_FALSE(VerifyBasicResponse(storage_control(), resource_id1_, false));
  EXPECT_FALSE(VerifyBasicResponse(storage_control(), resource_id2_, false));
}

TEST_F(ServiceWorkerRegistryResourceTest, DeleteRegistration_WaitingVersion) {
  // Deleting the registration should result in the resources being added to the
  // purgeable list and then doomed in the disk cache and removed from that
  // list.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            DeleteRegistration(registration_,
                               blink::StorageKey(url::Origin::Create(scope_))));
  EXPECT_EQ(2u, GetPurgeableResourceIds().size());

  EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id1_, false));
  EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id2_, false));

  // Doom the version. The resources should be purged.
  base::RunLoop loop;
  storage_control()->SetPurgingCompleteCallbackForTest(loop.QuitClosure());
  registration_->waiting_version()->Doom();
  loop.Run();
  EXPECT_TRUE(GetPurgeableResourceIds().empty());

  EXPECT_FALSE(VerifyBasicResponse(storage_control(), resource_id1_, false));
  EXPECT_FALSE(VerifyBasicResponse(storage_control(), resource_id2_, false));
}

TEST_F(ServiceWorkerRegistryResourceTest, DeleteRegistration_ActiveVersion) {
  // Promote the worker to active and add a controllee.
  registration_->SetActiveVersion(registration_->waiting_version());
  registration_->active_version()->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registry()->UpdateToActiveState(
      registration_->id(),
      blink::StorageKey(url::Origin::Create(registration_->scope())),
      base::DoNothing());
  ServiceWorkerRemoteContainerEndpoint remote_endpoint;
  base::WeakPtr<ServiceWorkerContainerHost> container_host =
      CreateContainerHostForWindow(
          GlobalRenderFrameHostId(/*mock process_id=*/33,
                                  /*mock frame_routing_id=*/1),
          /*is_parent_frame_secure=*/true, context()->AsWeakPtr(),
          &remote_endpoint);
  registration_->active_version()->AddControllee(container_host.get());

  // Deleting the registration should move the resources to the purgeable list
  // but keep them available.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            DeleteRegistration(registration_,
                               blink::StorageKey(url::Origin::Create(scope_))));
  EXPECT_EQ(2u, GetPurgeableResourceIds().size());

  EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id1_, true));
  EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id2_, true));

  // Dooming the version should cause the resources to be deleted.
  base::RunLoop loop;
  storage_control()->SetPurgingCompleteCallbackForTest(loop.QuitClosure());
  registration_->active_version()->RemoveControllee(
      container_host->client_uuid());
  registration_->active_version()->Doom();
  loop.Run();
  EXPECT_TRUE(GetPurgeableResourceIds().empty());

  EXPECT_FALSE(VerifyBasicResponse(storage_control(), resource_id1_, false));
  EXPECT_FALSE(VerifyBasicResponse(storage_control(), resource_id2_, false));
}

TEST_F(ServiceWorkerRegistryResourceTest, UpdateRegistration) {
  // Promote the worker to active worker and add a controllee.
  registration_->SetActiveVersion(registration_->waiting_version());
  registration_->active_version()->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registry()->UpdateToActiveState(registration_->id(), registration_->key(),
                                  base::DoNothing());
  ServiceWorkerRemoteContainerEndpoint remote_endpoint;
  base::WeakPtr<ServiceWorkerContainerHost> container_host =
      CreateContainerHostForWindow(
          GlobalRenderFrameHostId(/*mock process_id=*/33,
                                  /*mock frame_routing_id=*/1),
          /*is_parent_frame_secure=*/true, context()->AsWeakPtr(),
          &remote_endpoint);
  registration_->active_version()->AddControllee(container_host.get());

  // Make an updated registration.
  scoped_refptr<ServiceWorkerVersion> live_version =
      CreateNewServiceWorkerVersion(registry(), registration_.get(), script_,
                                    blink::mojom::ScriptType::kClassic);
  live_version->SetStatus(ServiceWorkerVersion::NEW);
  registration_->SetWaitingVersion(live_version);
  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> records;
  records.push_back(CreateResourceRecord(10, live_version->script_url(), 100));
  live_version->script_cache_map()->SetResources(records);
  live_version->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);

  // Writing the registration should move the old version's resources to the
  // purgeable list but keep them available.
  EXPECT_EQ(
      blink::ServiceWorkerStatusCode::kOk,
      StoreRegistration(registration_.get(), registration_->waiting_version()));
  EXPECT_EQ(2u, GetPurgeableResourceIds().size());
  EXPECT_TRUE(GetPurgingResources().empty());

  EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id1_, false));
  EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id2_, false));

  // Remove the controllee to allow the new version to become active, making the
  // old version redundant.
  base::RunLoop loop;
  storage_control()->SetPurgingCompleteCallbackForTest(loop.QuitClosure());
  scoped_refptr<ServiceWorkerVersion> old_version(
      registration_->active_version());
  old_version->RemoveControllee(container_host->client_uuid());
  registration_->ActivateWaitingVersionWhenReady();
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT, old_version->status());

  // Its resources should be purged.
  loop.Run();
  EXPECT_TRUE(GetPurgeableResourceIds().empty());
  EXPECT_FALSE(VerifyBasicResponse(storage_control(), resource_id1_, false));
  EXPECT_FALSE(VerifyBasicResponse(storage_control(), resource_id2_, false));
}

TEST_F(ServiceWorkerRegistryResourceTest, UpdateRegistration_NoLiveVersion) {
  // Promote the worker to active worker and add a controllee.
  registration_->SetActiveVersion(registration_->waiting_version());
  registry()->UpdateToActiveState(registration_->id(), registration_->key(),
                                  base::DoNothing());

  // Make an updated registration.
  scoped_refptr<ServiceWorkerVersion> live_version =
      CreateNewServiceWorkerVersion(registry(), registration_.get(), script_,
                                    blink::mojom::ScriptType::kClassic);
  live_version->SetStatus(ServiceWorkerVersion::NEW);
  registration_->SetWaitingVersion(live_version);
  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> records;
  records.push_back(CreateResourceRecord(10, live_version->script_url(), 100));
  live_version->script_cache_map()->SetResources(records);
  live_version->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);

  // Writing the registration should purge the old version's resources,
  // since it's not live.
  base::RunLoop loop;
  storage_control()->SetPurgingCompleteCallbackForTest(loop.QuitClosure());
  EXPECT_EQ(
      blink::ServiceWorkerStatusCode::kOk,
      StoreRegistration(registration_.get(), registration_->waiting_version()));
  EXPECT_EQ(2u, GetPurgeableResourceIds().size());

  // Destroy the active version.
  registration_->UnsetVersion(registration_->active_version());

  // The resources should be purged.
  loop.Run();
  EXPECT_TRUE(GetPurgeableResourceIds().empty());
  EXPECT_FALSE(VerifyBasicResponse(storage_control(), resource_id1_, false));
  EXPECT_FALSE(VerifyBasicResponse(storage_control(), resource_id2_, false));
}

TEST_F(ServiceWorkerRegistryResourceTest, CleanupOnRestart) {
  // Promote the worker to active and add a controllee.
  registration_->SetActiveVersion(registration_->waiting_version());
  registration_->active_version()->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetWaitingVersion(nullptr);
  registry()->UpdateToActiveState(registration_->id(), registration_->key(),
                                  base::DoNothing());
  ServiceWorkerRemoteContainerEndpoint remote_endpoint;
  base::WeakPtr<ServiceWorkerContainerHost> container_host =
      CreateContainerHostForWindow(
          GlobalRenderFrameHostId(/*mock process_id=*/33,
                                  /*mock frame_routing_id=*/1),
          /*is_parent_frame_secure=*/true, context()->AsWeakPtr(),
          &remote_endpoint);
  registration_->active_version()->AddControllee(container_host.get());

  // Deleting the registration should move the resources to the purgeable list
  // but keep them available.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            DeleteRegistration(registration_,
                               blink::StorageKey(url::Origin::Create(scope_))));
  std::vector<int64_t> verify_ids = GetPurgeableResourceIds();
  EXPECT_EQ(2u, verify_ids.size());

  EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id1_, true));
  EXPECT_TRUE(VerifyBasicResponse(storage_control(), resource_id2_, true));

  // Also add an uncommitted resource.
  int64_t kStaleUncommittedResourceId = GetNewResourceIdSync(storage_control());
  registry()->StoreUncommittedResourceId(kStaleUncommittedResourceId,
                                         registration_->key());
  EnsureRemoteCallsAreExecuted();
  verify_ids = GetUncommittedResourceIds();
  EXPECT_EQ(1u, verify_ids.size());
  WriteBasicResponse(storage_control(), kStaleUncommittedResourceId);
  EXPECT_TRUE(VerifyBasicResponse(storage_control(),
                                  kStaleUncommittedResourceId, true));

  // Simulate browser shutdown. The purgeable and uncommitted resources are now
  // stale.
  SimulateRestart();

  // Store a new uncommitted resource. This triggers stale resource cleanup.
  base::RunLoop loop;
  storage_control()->SetPurgingCompleteCallbackForTest(loop.QuitClosure());
  int64_t kNewResourceId = GetNewResourceIdSync(storage_control());
  WriteBasicResponse(storage_control(), kNewResourceId);
  registry()->StoreUncommittedResourceId(kNewResourceId, registration_->key());
  loop.Run();

  // The stale resources should be purged, but the new resource should persist.
  verify_ids = GetUncommittedResourceIds();
  ASSERT_EQ(1u, verify_ids.size());
  EXPECT_EQ(kNewResourceId, *verify_ids.begin());

  verify_ids = GetPurgeableResourceIds();
  EXPECT_TRUE(verify_ids.empty());
  EXPECT_FALSE(VerifyBasicResponse(storage_control(), resource_id1_, false));
  EXPECT_FALSE(VerifyBasicResponse(storage_control(), resource_id2_, false));
  EXPECT_FALSE(VerifyBasicResponse(storage_control(),
                                   kStaleUncommittedResourceId, false));
  EXPECT_TRUE(VerifyBasicResponse(storage_control(), kNewResourceId, true));
}

// Tests resource purging with storage service restarts.
TEST_F(ServiceWorkerRegistryResourceTest, Restart_LiveVersion) {
  // Precondition: One registration with two resources is stored. There is a
  // waiting version associated with the registration.

  // Restarting should not schedule resource purging.
  helper()->SimulateStorageRestartForTesting();
  storage_control().FlushForTesting();

  ASSERT_EQ(GetPurgeableResourceIds().size(), 0u);
  ASSERT_EQ(GetPurgingResources().size(), 0u);

  // Delete the registration. The resources should be on the purgeable list but
  // should not be purged yet.
  ASSERT_EQ(DeleteRegistration(registration_,
                               blink::StorageKey(url::Origin::Create(scope_))),
            blink::ServiceWorkerStatusCode::kOk);

  EXPECT_THAT(GetPurgeableResourceIds(), testing::UnorderedElementsAreArray(
                                             {resource_id1_, resource_id2_}));
  EXPECT_TRUE(GetPurgingResources().empty());
  ASSERT_TRUE(VerifyBasicResponse(storage_control(), resource_id1_, true));
  ASSERT_TRUE(VerifyBasicResponse(storage_control(), resource_id2_, true));

  // Restarting should not change the situation.
  helper()->SimulateStorageRestartForTesting();
  storage_control().FlushForTesting();

  EXPECT_THAT(GetPurgeableResourceIds(), testing::UnorderedElementsAreArray(
                                             {resource_id1_, resource_id2_}));
  EXPECT_TRUE(GetPurgingResources().empty());
  ASSERT_TRUE(VerifyBasicResponse(storage_control(), resource_id1_, true));
  ASSERT_TRUE(VerifyBasicResponse(storage_control(), resource_id2_, true));

  // Doom the version. The resources should be purged.
  base::RunLoop loop;
  storage_control()->SetPurgingCompleteCallbackForTest(loop.QuitClosure());
  registration_->waiting_version()->Doom();
  loop.Run();

  ASSERT_EQ(GetPurgeableResourceIds().size(), 0u);
  ASSERT_EQ(GetPurgingResources().size(), 0u);
  ASSERT_FALSE(VerifyBasicResponse(storage_control(), resource_id1_, false));
  ASSERT_FALSE(VerifyBasicResponse(storage_control(), resource_id2_, false));
}

// Tests that StoreUncommittedResourceId() and DoomUncommittedResource() are
// automatically retried after storage restarts.
TEST_F(ServiceWorkerRegistryResourceTest, RetryInflightCalls_Resources) {
  const int64_t kResourceId = GetNewResourceIdSync(storage_control());

  registry()->StoreUncommittedResourceId(kResourceId, registration_->key());
  EXPECT_EQ(inflight_call_count(), 1U);

  helper()->SimulateStorageRestartForTesting();
  EnsureRemoteCallsAreExecuted();

  EXPECT_EQ(inflight_call_count(), 0U);
  EXPECT_THAT(GetUncommittedResourceIds(),
              testing::UnorderedElementsAreArray({kResourceId}));

  registry()->DoomUncommittedResource(kResourceId);
  EXPECT_EQ(inflight_call_count(), 1U);

  helper()->SimulateStorageRestartForTesting();
  EnsureRemoteCallsAreExecuted();

  EXPECT_EQ(inflight_call_count(), 0U);
  EXPECT_EQ(GetUncommittedResourceIds().size(), 0U);
}

}  // namespace content
