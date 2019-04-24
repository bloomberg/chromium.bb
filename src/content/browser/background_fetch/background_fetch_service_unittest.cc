// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/background_fetch/background_fetch_context.h"
#include "content/browser/background_fetch/background_fetch_data_manager_observer.h"
#include "content/browser/background_fetch/background_fetch_job_controller.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_service_impl.h"
#include "content/browser/background_fetch/background_fetch_test_base.h"
#include "content/browser/background_fetch/background_fetch_test_data_manager.h"
#include "content/browser/background_fetch/background_fetch_test_service_worker.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "content/common/service_worker/service_worker_types.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "ui/gfx/geometry/size.h"

namespace content {
namespace {

using testing::_;

const char kExampleUniqueId[] = "7e57ab1e-c0de-a150-ca75-1e75f005ba11";
const char kExampleDeveloperId[] = "my-background-fetch";
const char kAlternativeDeveloperId[] = "my-alternative-fetch";

blink::Manifest::ImageResource CreateIcon(const std::string& src,
                                          std::vector<gfx::Size> sizes,
                                          const std::string& type) {
  blink::Manifest::ImageResource icon;
  icon.src = GURL(src);
  icon.sizes = std::move(sizes);
  icon.type = base::ASCIIToUTF16(type);

  return icon;
}

bool ContainsHeader(const base::flat_map<std::string, std::string>& headers,
                    const std::string& target) {
  return headers.cend() !=
         std::find_if(headers.cbegin(), headers.cend(),
                      [target](const auto& pair) -> bool {
                        return base::EqualsCaseInsensitiveASCII(pair.first,
                                                                target);
                      });
}

class FakeMojoMessageDispatchContext {
 public:
  FakeMojoMessageDispatchContext()
      : dummy_message_(0, 0, 0, 0, nullptr), context_(&dummy_message_) {}

 private:
  mojo::Message dummy_message_;
  mojo::internal::MessageDispatchContext context_;

  DISALLOW_COPY_AND_ASSIGN(FakeMojoMessageDispatchContext);
};

std::vector<blink::mojom::FetchAPIRequestPtr> CloneRequestVector(
    const std::vector<blink::mojom::FetchAPIRequestPtr>& requests) {
  std::vector<blink::mojom::FetchAPIRequestPtr> request_cp;
  for (const auto& request : requests) {
    request_cp.push_back(BackgroundFetchSettledFetch::CloneRequest(request));
  }
  return request_cp;
}

}  // namespace

class BackgroundFetchServiceTest : public BackgroundFetchTestBase,
                                   public BackgroundFetchDataManagerObserver,
                                   public ServiceWorkerContextCoreObserver {
 public:
  BackgroundFetchServiceTest() = default;
  ~BackgroundFetchServiceTest() override = default;

  class ScopedCustomBackgroundFetchService {
   public:
    ScopedCustomBackgroundFetchService(BackgroundFetchServiceTest* test,
                                       const url::Origin& origin)
        : scoped_service_(&test->service_,
                          std::make_unique<BackgroundFetchServiceImpl>(
                              test->context_,
                              origin,
                              /* render_frame_tree_node_id= */ 0,
                              /* wc_getter= */ base::NullCallback())) {}

   private:
    base::AutoReset<std::unique_ptr<BackgroundFetchServiceImpl>>
        scoped_service_;

    DISALLOW_COPY_AND_ASSIGN(ScopedCustomBackgroundFetchService);
  };

  // Synchronous wrapper for BackgroundFetchServiceImpl::Fetch().
  BackgroundFetchRegistrationId Fetch(
      int64_t service_worker_registration_id,
      const std::string& developer_id,
      std::vector<blink::mojom::FetchAPIRequestPtr> requests,
      blink::mojom::BackgroundFetchOptionsPtr options,
      const SkBitmap& icon,
      blink::mojom::BackgroundFetchError* out_error,
      blink::mojom::BackgroundFetchRegistration* out_registration) {
    DCHECK(out_error);
    DCHECK(out_registration);

    base::RunLoop run_loop;

    service_->Fetch(
        service_worker_registration_id, developer_id, std::move(requests),
        std::move(options), icon, blink::mojom::BackgroundFetchUkmData::New(),
        base::BindOnce(&BackgroundFetchServiceTest::DidGetRegistration,
                       base::Unretained(this), run_loop.QuitClosure(),
                       out_error, out_registration));

    run_loop.Run();

    if (*out_error != blink::mojom::BackgroundFetchError::NONE)
      return BackgroundFetchRegistrationId();

    return BackgroundFetchRegistrationId(service_worker_registration_id,
                                         origin(), developer_id,
                                         out_registration->unique_id);
  }

  // Starts the Fetch without completing it. Only creates a registration.
  void StartFetch(int64_t service_worker_registration_id,
                  const std::string& developer_id,
                  std::vector<blink::mojom::FetchAPIRequestPtr> requests,
                  blink::mojom::BackgroundFetchOptionsPtr options,
                  const SkBitmap& icon) {
    BackgroundFetchRegistrationId registration_id(
        service_worker_registration_id, origin(), developer_id,
        kExampleUniqueId);

    base::RunLoop run_loop;
    context_->data_manager_->CreateRegistration(
        registration_id, std::move(requests), std::move(options), icon,
        /* start_paused= */ false,
        base::BindOnce(&BackgroundFetchServiceTest::DidStartFetch,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Calls BackgroundFetchServiceImpl::Fetch() and unregisters the service
  // worker before Fetch has completed but after the controller has been
  // initialized.
  void FetchAndUnregisterServiceWorker(
      int64_t service_worker_registration_id,
      const std::string& developer_id,
      std::vector<blink::mojom::FetchAPIRequestPtr> requests,
      blink::mojom::BackgroundFetchOptionsPtr options,
      const SkBitmap& icon,
      blink::mojom::BackgroundFetchError* out_error,
      blink::mojom::BackgroundFetchRegistration* out_registration) {
    DCHECK(out_error);
    DCHECK(out_registration);

    base::RunLoop run_loop;

    service_->Fetch(
        service_worker_registration_id, developer_id, std::move(requests),
        std::move(options), icon, blink::mojom::BackgroundFetchUkmData::New(),
        base::BindOnce(&BackgroundFetchServiceTest::DidGetRegistration,
                       base::Unretained(this), run_loop.QuitClosure(),
                       out_error, out_registration));
    UnregisterServiceWorker(service_worker_registration_id);
    run_loop.Run();
  }

  // Calls BackgroundFetchServiceImpl::MatchRequests() to retrieve all settled
  // fetches.
  void MatchAllRequests(
      int64_t service_worker_registration_id,
      const std::string& developer_id,
      const std::string& unique_id,
      std::vector<blink::mojom::BackgroundFetchSettledFetchPtr>* out_fetches) {
    DCHECK(out_fetches);
    base::RunLoop run_loop;
    service_->MatchRequests(
        service_worker_registration_id, developer_id, unique_id,
        /* request_to_match= */ nullptr,
        /* cache_query_options= */ nullptr, /* match_all= */ true,
        base::BindOnce(&BackgroundFetchServiceTest::DidMatchAllRequests,
                       base::Unretained(this), run_loop.QuitClosure(),
                       out_fetches));
    run_loop.Run();
  }

  // Synchronous wrapper for BackgroundFetchServiceImpl::UpdateUI().
  void UpdateUI(int64_t service_worker_registration_id,
                const std::string& developer_id,
                const std::string& unique_id,
                const std::string& title,
                blink::mojom::BackgroundFetchError* out_error) {
    DCHECK(out_error);

    base::RunLoop run_loop;
    service_->UpdateUI(service_worker_registration_id, developer_id, unique_id,
                       title, SkBitmap(),
                       base::BindOnce(&BackgroundFetchServiceTest::DidGetError,
                                      base::Unretained(this),
                                      run_loop.QuitClosure(), out_error));
    run_loop.Run();
  }

  // Synchronous wrapper for BackgroundFetchServiceImpl::Abort().
  void Abort(int64_t service_worker_registration_id,
             const std::string& developer_id,
             const std::string& unique_id,
             blink::mojom::BackgroundFetchError* out_error) {
    DCHECK(out_error);

    base::RunLoop run_loop;
    service_->Abort(service_worker_registration_id, developer_id, unique_id,
                    base::BindOnce(&BackgroundFetchServiceTest::DidGetError,
                                   base::Unretained(this),
                                   run_loop.QuitClosure(), out_error));

    run_loop.Run();
  }

  // Synchronous wrapper for BackgroundFetchServiceImpl::GetRegistration().
  void GetRegistration(
      int64_t service_worker_registration_id,
      const std::string& developer_id,
      blink::mojom::BackgroundFetchError* out_error,
      blink::mojom::BackgroundFetchRegistration* out_registration) {
    DCHECK(out_error);
    DCHECK(out_registration);

    base::RunLoop run_loop;
    service_->GetRegistration(
        service_worker_registration_id, developer_id,
        base::BindOnce(&BackgroundFetchServiceTest::DidGetRegistration,
                       base::Unretained(this), run_loop.QuitClosure(),
                       out_error, out_registration));

    run_loop.Run();
  }

  // Synchronous wrapper for BackgroundFetchServiceImpl::GetDeveloperIds().
  void GetDeveloperIds(int64_t service_worker_registration_id,
                       blink::mojom::BackgroundFetchError* out_error,
                       std::vector<std::string>* out_developer_ids) {
    DCHECK(out_error);
    DCHECK(out_developer_ids);

    base::RunLoop run_loop;
    service_->GetDeveloperIds(
        service_worker_registration_id,
        base::BindOnce(&BackgroundFetchServiceTest::DidGetDeveloperIds,
                       base::Unretained(this), run_loop.QuitClosure(),
                       out_error, out_developer_ids));

    run_loop.Run();
  }

  // BackgroundFetchTestBase overrides:
  void SetUp() override {
    BackgroundFetchTestBase::SetUp();

    context_ = base::MakeRefCounted<BackgroundFetchContext>(
        browser_context(),
        base::WrapRefCounted(embedded_worker_test_helper()->context_wrapper()),
        /* cache_storage_context= */ nullptr,
        /* quota_manager_proxy= */ nullptr);
    context_->SetDataManagerForTesting(
        std::make_unique<BackgroundFetchTestDataManager>(
            browser_context(), storage_partition(),
            embedded_worker_test_helper()->context_wrapper()));
    context_->data_manager_->AddObserver(this);
    embedded_worker_test_helper()->context_wrapper()->AddObserver(this);

    context_->InitializeOnIOThread();
    service_ = std::make_unique<BackgroundFetchServiceImpl>(
        context_, origin(),
        /* render_frame_tree_node_id= */ 0,
        /* wc_getter= */ base::NullCallback());
  }

  void TearDown() override {
    BackgroundFetchTestBase::TearDown();

    service_.reset();

    embedded_worker_test_helper()->context_wrapper()->RemoveObserver(this);
    context_->data_manager_->RemoveObserver(this);
    context_ = nullptr;

    // Give pending shutdown operations a chance to finish.
    base::RunLoop().RunUntilIdle();
  }

  // BackgroundFetchDataManagerObserver implementation & mocks:
  MOCK_METHOD6(
      OnRegistrationCreated,
      void(const BackgroundFetchRegistrationId& registration_id,
           const blink::mojom::BackgroundFetchRegistration& registration,
           blink::mojom::BackgroundFetchOptionsPtr options,
           const SkBitmap& icon,
           int num_requests,
           bool start_paused));
  MOCK_METHOD7(
      OnRegistrationLoadedAtStartup,
      void(const BackgroundFetchRegistrationId& registration_id,
           const blink::mojom::BackgroundFetchRegistration& registration,
           blink::mojom::BackgroundFetchOptionsPtr options,
           const SkBitmap& icon,
           int num_completed_requests,
           int num_requests,
           std::vector<scoped_refptr<BackgroundFetchRequestInfo>>
               active_fetch_requests));
  MOCK_METHOD1(OnRegistrationQueried,
               void(blink::mojom::BackgroundFetchRegistration* registration));
  MOCK_METHOD1(OnServiceWorkerDatabaseCorrupted,
               void(int64_t service_worker_registration_id));
  MOCK_METHOD3(OnRequestCompleted,
               void(const std::string& unique_id,
                    blink::mojom::FetchAPIRequestPtr request,
                    blink::mojom::FetchAPIResponsePtr response));

  // ServiceWorkerContextCoreObserver implementation.
  MOCK_METHOD2(OnRegistrationDeleted,
               void(int64_t registration_id, const GURL& pattern));
  MOCK_METHOD0(OnStorageWiped, void());

 protected:
  blink::mojom::FetchAPIRequestPtr CreateDefaultRequest() {
    return CreateRequestWithProvidedResponse(
        "GET", GURL("https://example.com/funny_cat.txt"),
        TestResponseBuilder(200).MakeIndefinitelyPending().Build());
  }

  scoped_refptr<BackgroundFetchContext> context_;

 private:
  void DidGetRegistration(
      base::OnceClosure quit_closure,
      blink::mojom::BackgroundFetchError* out_error,
      blink::mojom::BackgroundFetchRegistration* out_registration,
      blink::mojom::BackgroundFetchError error,
      blink::mojom::BackgroundFetchRegistrationPtr registration) {
    *out_error = error;
    *out_registration = registration
                            ? *registration
                            : blink::mojom::BackgroundFetchRegistration();

    std::move(quit_closure).Run();
  }

  void DidStartFetch(
      base::OnceClosure quit_closure,
      blink::mojom::BackgroundFetchError error,
      blink::mojom::BackgroundFetchRegistrationPtr registration) {
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
    std::move(quit_closure).Run();
  }

  void DidGetError(base::OnceClosure quit_closure,
                   blink::mojom::BackgroundFetchError* out_error,
                   blink::mojom::BackgroundFetchError error) {
    *out_error = error;

    std::move(quit_closure).Run();
  }

  void DidGetDeveloperIds(base::OnceClosure quit_closure,
                          blink::mojom::BackgroundFetchError* out_error,
                          std::vector<std::string>* out_developer_ids,
                          blink::mojom::BackgroundFetchError error,
                          const std::vector<std::string>& developer_ids) {
    *out_error = error;
    *out_developer_ids = developer_ids;

    std::move(quit_closure).Run();
  }

  void DidMatchAllRequests(
      base::OnceClosure quit_closure,
      std::vector<blink::mojom::BackgroundFetchSettledFetchPtr>* out_fetches,
      std::vector<blink::mojom::BackgroundFetchSettledFetchPtr> fetches) {
    for (const auto& in_fetch : fetches) {
      auto out_fetch = blink::mojom::BackgroundFetchSettledFetch::New();
      out_fetch->request =
          BackgroundFetchSettledFetch::CloneRequest(in_fetch->request);
      out_fetch->response =
          BackgroundFetchSettledFetch::CloneResponse(in_fetch->response);
      out_fetches->push_back(std::move(out_fetch));
    }
    std::move(quit_closure).Run();
  }

  std::unique_ptr<BackgroundFetchServiceImpl> service_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchServiceTest);
};

TEST_F(BackgroundFetchServiceTest, FetchInvalidArguments) {
  // This test verifies that the Fetch() function will kill the renderer and
  // return INVALID_ARGUMENT when invalid data is send over the Mojo channel.

  auto options = blink::mojom::BackgroundFetchOptions::New();

  // The |developer_id| must be a non-empty string.
  {
    FakeMojoMessageDispatchContext fake_dispatch_context;
    mojo::test::BadMessageObserver bad_message_observer;
    std::vector<blink::mojom::FetchAPIRequestPtr> requests;
    requests.push_back(CreateDefaultRequest());

    blink::mojom::BackgroundFetchError error;
    blink::mojom::BackgroundFetchRegistration registration;

    Fetch(/* service_worker_registration_id= */ 42, /* developer_id= */ "",
          std::move(requests), options.Clone(), SkBitmap(), &error,
          &registration);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_ARGUMENT);
    EXPECT_EQ("Invalid developer_id", bad_message_observer.WaitForBadMessage());
  }

  // At least a single blink::mojom::FetchAPIRequestPtr must be given.
  {
    FakeMojoMessageDispatchContext fake_dispatch_context;
    mojo::test::BadMessageObserver bad_message_observer;
    std::vector<blink::mojom::FetchAPIRequestPtr> requests;
    // |requests| has deliberately been left empty.

    blink::mojom::BackgroundFetchError error;
    blink::mojom::BackgroundFetchRegistration registration;

    Fetch(/* service_worker_registration_id= */ 42, kExampleDeveloperId,
          std::move(requests), std::move(options), SkBitmap(), &error,
          &registration);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_ARGUMENT);
    EXPECT_EQ("Invalid requests", bad_message_observer.WaitForBadMessage());
  }
}

TEST_F(BackgroundFetchServiceTest, FetchRegistrationProperties) {
  // This test starts a new Background Fetch and verifies that the returned
  // blink::mojom::BackgroundFetchRegistration object matches the given options.
  // Then gets the active Background Fetch with the same |developer_id|, and
  // verifies that.

  int64_t service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            service_worker_registration_id);

  std::vector<blink::mojom::FetchAPIRequestPtr> requests;
  requests.push_back(CreateDefaultRequest());

  auto options = blink::mojom::BackgroundFetchOptions::New();
  options->icons.push_back(
      CreateIcon("funny_cat.png", {{256, 256}}, "image/png"));
  options->icons.push_back(
      CreateIcon("silly_cat.gif", {{512, 512}}, "image/gif"));
  options->title = "My Background Fetch!";
  options->download_total = 9001;

  blink::mojom::BackgroundFetchError error;
  blink::mojom::BackgroundFetchRegistration registration;

  Fetch(service_worker_registration_id, kExampleDeveloperId,
        std::move(requests), options.Clone(), SkBitmap(), &error,
        &registration);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  // The |registration| should reflect the options given in |options|.
  EXPECT_EQ(registration.developer_id, kExampleDeveloperId);

  EXPECT_EQ(registration.download_total, options->download_total);

  blink::mojom::BackgroundFetchError second_error;
  blink::mojom::BackgroundFetchRegistration second_registration;

  GetRegistration(service_worker_registration_id, kExampleDeveloperId,
                  &second_error, &second_registration);
  ASSERT_EQ(second_error, blink::mojom::BackgroundFetchError::NONE);

  // The |second_registration| should reflect the options given in |options|.
  EXPECT_EQ(second_registration.developer_id, kExampleDeveloperId);

  EXPECT_EQ(second_registration.download_total, options->download_total);
}

TEST_F(BackgroundFetchServiceTest, FetchDuplicatedRegistrationFailure) {
  // This tests starts a new Background Fetch, verifies that a registration was
  // successfully created, and then tries to start a second fetch for the same
  // registration. This should fail with a DUPLICATED_DEVELOPER_ID error.

  int64_t service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            service_worker_registration_id);

  std::vector<blink::mojom::FetchAPIRequestPtr> requests;
  requests.push_back(CreateDefaultRequest());

  auto options = blink::mojom::BackgroundFetchOptions::New();

  blink::mojom::BackgroundFetchError error;
  blink::mojom::BackgroundFetchRegistration registration;

  // Create the first registration. This must succeed.
  Fetch(service_worker_registration_id, kExampleDeveloperId,
        CloneRequestVector(requests), options.Clone(), SkBitmap(), &error,
        &registration);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  blink::mojom::BackgroundFetchError second_error;
  blink::mojom::BackgroundFetchRegistration second_registration;

  // Create the second registration with the same data. This must fail.
  Fetch(service_worker_registration_id, kExampleDeveloperId,
        std::move(requests), std::move(options), SkBitmap(), &second_error,
        &second_registration);
  ASSERT_EQ(second_error,
            blink::mojom::BackgroundFetchError::DUPLICATED_DEVELOPER_ID);
}

TEST_F(BackgroundFetchServiceTest, FetchSuccessEventDispatch) {
  // This test starts a new Background Fetch, completes the registration, then
  // fetches all files to complete the job, and then verifies that the
  // `backgroundfetchsuccess` event will be dispatched with the expected
  // contents.

  auto* worker =
      embedded_worker_test_helper()
          ->AddNewPendingServiceWorker<BackgroundFetchTestServiceWorker>(
              embedded_worker_test_helper());
  int64_t service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            service_worker_registration_id);

  // base::RunLoop that we'll run until the event has been dispatched. If this
  // test times out, it means that the event could not be dispatched.
  base::RunLoop event_dispatched_loop;
  worker->set_fetched_event_closure(event_dispatched_loop.QuitClosure());

  std::vector<blink::mojom::FetchAPIRequestPtr> requests;

  constexpr int kFirstResponseCode = 200;
  constexpr int kSecondResponseCode = 201;
  constexpr int kThirdResponseCode = 200;

  requests.push_back(CreateRequestWithProvidedResponse(
      "GET", GURL("https://example.com/funny_cat.txt"),
      TestResponseBuilder(kFirstResponseCode)
          .SetResponseData(
              "This text describes a scenario involving a funny cat.")
          .AddResponseHeader("Content-Type", "text/plain")
          .AddResponseHeader("X-Cat", "yes")
          .Build()));

  requests.push_back(CreateRequestWithProvidedResponse(
      "GET", GURL("https://example.com/crazy_cat.txt"),
      TestResponseBuilder(kSecondResponseCode)
          .SetResponseData(
              "This text describes another scenario that involves a crazy cat.")
          .AddResponseHeader("Content-Type", "text/plain")
          .Build()));

  requests.push_back(CreateRequestWithProvidedResponse(
      "GET", GURL("https://chrome.com/accessible_cross_origin_cat.txt"),
      TestResponseBuilder(kThirdResponseCode)
          .SetResponseData("This cat originates from another origin.")
          .AddResponseHeader("Access-Control-Allow-Origin", "*")
          .AddResponseHeader("Content-Type", "text/plain")
          .Build()));

  // Create the registration with the given |requests|.
  blink::mojom::BackgroundFetchRegistration registration;
  {
    auto options = blink::mojom::BackgroundFetchOptions::New();
    blink::mojom::BackgroundFetchError error;

    // Create the first registration. This must succeed.
    Fetch(service_worker_registration_id, kExampleDeveloperId,
          CloneRequestVector(requests), std::move(options), SkBitmap(), &error,
          &registration);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  // Spin the |event_dispatched_loop| to wait for the dispatched event.
  event_dispatched_loop.Run();

  ASSERT_TRUE(worker->last_registration());
  EXPECT_EQ(kExampleDeveloperId, registration.developer_id);

  // Get all the settled fetches and test properties.
  std::vector<blink::mojom::BackgroundFetchSettledFetchPtr> fetches;
  MatchAllRequests(service_worker_registration_id, registration.developer_id,
                   registration.unique_id, &fetches);
  ASSERT_EQ(fetches.size(), requests.size());
  for (size_t i = 0; i < fetches.size(); ++i) {
    ASSERT_EQ(fetches[i]->request->url, requests[i]->url);
    EXPECT_EQ(fetches[i]->request->method, requests[i]->method);

    EXPECT_EQ(fetches[i]->response->url_list[0], fetches[i]->request->url);
    EXPECT_EQ(fetches[i]->response->response_type,
              network::mojom::FetchResponseType::kDefault);

    switch (i) {
      case 0:
        EXPECT_EQ(fetches[i]->response->status_code, kFirstResponseCode);
        EXPECT_TRUE(
            ContainsHeader(fetches[i]->response->headers, "Content-Type"));
        EXPECT_TRUE(ContainsHeader(fetches[i]->response->headers, "X-Cat"));
        break;
      case 1:
        EXPECT_EQ(fetches[i]->response->status_code, kSecondResponseCode);
        EXPECT_TRUE(
            ContainsHeader(fetches[i]->response->headers, "Content-Type"));
        EXPECT_FALSE(ContainsHeader(fetches[i]->response->headers, "X-Cat"));
        break;
      case 2:
        EXPECT_EQ(fetches[i]->response->status_code, kThirdResponseCode);
        EXPECT_TRUE(
            ContainsHeader(fetches[i]->response->headers, "Content-Type"));
        EXPECT_FALSE(ContainsHeader(fetches[i]->response->headers, "X-Cat"));
        break;
      default:
        NOTREACHED();
    }

    // TODO(peter): change-detector tests for unsupported properties.
    EXPECT_EQ(fetches[i]->response->error,
              blink::mojom::ServiceWorkerResponseError::kUnknown);

    // Verify that all properties have a sensible value.
    EXPECT_FALSE(fetches[i]->response->response_time.is_null());

    // Verify that the response blobs have been populated. We cannot consume
    // their data here since the handles have already been released.
    ASSERT_TRUE(fetches[i]->response->blob);
    ASSERT_FALSE(fetches[i]->response->blob->uuid.empty());
    ASSERT_GT(fetches[i]->response->blob->size, 0u);
  }

  auto* delegate = static_cast<MockBackgroundFetchDelegate*>(
      browser_context()->GetBackgroundFetchDelegate());
  EXPECT_TRUE(delegate->completed_jobs().count(registration.unique_id));
}

TEST_F(BackgroundFetchServiceTest, FetchFailEventDispatch) {
  // This test verifies that the fail event will be fired when a response either
  // has a non-OK status code, or the response cannot be accessed due to CORS.

  auto* worker =
      embedded_worker_test_helper()
          ->AddNewPendingServiceWorker<BackgroundFetchTestServiceWorker>(
              embedded_worker_test_helper());
  int64_t service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            service_worker_registration_id);

  // base::RunLoop that we'll run until the event has been dispatched. If this
  // test times out, it means that the event could not be dispatched.
  base::RunLoop event_dispatched_loop;
  worker->set_fetch_fail_event_closure(event_dispatched_loop.QuitClosure());

  std::vector<blink::mojom::FetchAPIRequestPtr> requests;

  constexpr int kFirstResponseCode = 404;
  constexpr int kSecondResponseCode = 200;

  requests.push_back(CreateRequestWithProvidedResponse(
      "GET", GURL("https://example.com/not_existing_cat.txt"),
      TestResponseBuilder(kFirstResponseCode).Build()));

  requests.push_back(CreateRequestWithProvidedResponse(
      "GET", GURL("https://chrome.com/inaccessible_cross_origin_cat.txt"),
      TestResponseBuilder(kSecondResponseCode)
          .SetResponseData(
              "This is a cross-origin response not accessible to the reader.")
          .AddResponseHeader("Content-Type", "text/plain")
          .Build()));

  // Create the registration with the given |requests|.
  blink::mojom::BackgroundFetchRegistration registration;

  {
    auto options = blink::mojom::BackgroundFetchOptions::New();

    blink::mojom::BackgroundFetchError error;

    // Create the first registration. This must succeed.
    Fetch(service_worker_registration_id, kExampleDeveloperId,
          CloneRequestVector(requests), std::move(options), SkBitmap(), &error,
          &registration);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  // Spin the |event_dispatched_loop| to wait for the dispatched event.
  event_dispatched_loop.Run();

  ASSERT_TRUE(worker->last_registration());
  EXPECT_EQ(kExampleDeveloperId, registration.developer_id);

  // Get all the settled fetches and test properties.
  std::vector<blink::mojom::BackgroundFetchSettledFetchPtr> fetches;
  MatchAllRequests(service_worker_registration_id, registration.developer_id,
                   registration.unique_id, &fetches);
  ASSERT_EQ(fetches.size(), 2u);

  // Make sure the 404 request is first, which has a response.
  if (!fetches[0]->response)
    std::swap(fetches[0], fetches[1]);

  for (size_t i = 0; i < fetches.size(); ++i) {
    ASSERT_EQ(fetches[i]->request->url, requests[i]->url);
    EXPECT_EQ(fetches[i]->request->method, requests[i]->method);

    switch (i) {
      case 0:
        EXPECT_EQ(fetches[i]->response->status_code, 404);
        EXPECT_EQ(fetches[i]->response->url_list[0], fetches[i]->request->url);
        EXPECT_EQ(fetches[i]->response->response_type,
                  network::mojom::FetchResponseType::kDefault);
        break;
      case 1:
        EXPECT_FALSE(fetches[i]->response);
        continue;
      default:
        NOTREACHED();
    }

    EXPECT_TRUE(fetches[i]->response->headers.empty());
    EXPECT_FALSE(fetches[i]->response->blob);
    EXPECT_FALSE(fetches[i]->response->response_time.is_null());

    // TODO(peter): change-detector tests for unsupported properties.
    EXPECT_EQ(fetches[i]->response->error,
              blink::mojom::ServiceWorkerResponseError::kUnknown);
    EXPECT_TRUE(fetches[i]->response->cors_exposed_header_names.empty());
  }

  auto* delegate = static_cast<MockBackgroundFetchDelegate*>(
      browser_context()->GetBackgroundFetchDelegate());
  EXPECT_TRUE(delegate->completed_jobs().count(registration.unique_id));
}

TEST_F(BackgroundFetchServiceTest, UpdateUI) {
  // This test starts a new Background Fetch, completes the registration, and
  // checks that updates to the title using UpdateUI are successfully reflected
  // back when calling GetRegistration.
  // TODO(crbug.com/766156): Add tests that UpdateUI() updates the UI of any
  // existing notifications, rather than merely updating the stored title.

  int64_t service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            service_worker_registration_id);

  std::vector<blink::mojom::FetchAPIRequestPtr> requests;
  requests.push_back(CreateDefaultRequest());

  auto options = blink::mojom::BackgroundFetchOptions::New();
  options->title = "1st title";

  blink::mojom::BackgroundFetchError error;
  blink::mojom::BackgroundFetchRegistration registration;

  // Create the registration.
  BackgroundFetchRegistrationId registration_id = Fetch(
      service_worker_registration_id, kExampleDeveloperId, std::move(requests),
      std::move(options), SkBitmap(), &error, &registration);
  ASSERT_EQ(blink::mojom::BackgroundFetchError::NONE, error);

  std::string second_title = "2nd title";

  // Immediately update the title. This should succeed.
  UpdateUI(registration_id.service_worker_registration_id(),
           registration_id.developer_id(), registration_id.unique_id(),
           second_title, &error);
  EXPECT_EQ(blink::mojom::BackgroundFetchError::NONE, error);

  blink::mojom::BackgroundFetchRegistration second_registration;

  // GetRegistration should now resolve with the updated title.
  GetRegistration(service_worker_registration_id, kExampleDeveloperId, &error,
                  &second_registration);
  ASSERT_EQ(blink::mojom::BackgroundFetchError::NONE, error);
}

TEST_F(BackgroundFetchServiceTest, Abort) {
  // This test starts a new Background Fetch, completes the registration, and
  // then aborts the Background Fetch mid-process.

  int64_t service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            service_worker_registration_id);

  std::vector<blink::mojom::FetchAPIRequestPtr> requests;
  requests.push_back(CreateDefaultRequest());

  auto options = blink::mojom::BackgroundFetchOptions::New();

  blink::mojom::BackgroundFetchError error;
  blink::mojom::BackgroundFetchRegistration registration;

  // Create the registration. This must succeed.
  BackgroundFetchRegistrationId registration_id = Fetch(
      service_worker_registration_id, kExampleDeveloperId, std::move(requests),
      std::move(options), SkBitmap(), &error, &registration);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  blink::mojom::BackgroundFetchError abort_error;

  // Immediately abort the registration. This also is expected to succeed.
  Abort(service_worker_registration_id, kExampleDeveloperId,
        registration_id.unique_id(), &abort_error);
  ASSERT_EQ(abort_error, blink::mojom::BackgroundFetchError::NONE);
  // Wait for the response of the Mojo IPC to dispatch
  // BackgroundFetchAbortEvent.
  base::RunLoop().RunUntilIdle();

  blink::mojom::BackgroundFetchError second_error;
  blink::mojom::BackgroundFetchRegistration second_registration;

  // Now try to get the created registration, which is expected to fail.
  GetRegistration(service_worker_registration_id, kExampleDeveloperId,
                  &second_error, &second_registration);
  ASSERT_EQ(second_error, blink::mojom::BackgroundFetchError::INVALID_ID);

  auto* delegate = static_cast<MockBackgroundFetchDelegate*>(
      browser_context()->GetBackgroundFetchDelegate());
  EXPECT_TRUE(delegate->completed_jobs().count(registration_id.unique_id()));
}

TEST_F(BackgroundFetchServiceTest, AbortInvalidDeveloperIdArgument) {
  // This test verifies that the Abort() function will kill the renderer and
  // return INVALID_ARGUMENT when an invalid |developer_id| is sent over the
  // Mojo channel.

  FakeMojoMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;
  blink::mojom::BackgroundFetchError error;
  Abort(/* service_worker_registration_id= */ 42, /* developer_id= */ "",
        kExampleUniqueId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_ARGUMENT);
  EXPECT_EQ("Invalid developer_id", bad_message_observer.WaitForBadMessage());
}

TEST_F(BackgroundFetchServiceTest, AbortInvalidUniqueIdArgument) {
  // This test verifies that the Abort() function will kill the renderer and
  // return INVALID_ARGUMENT when an invalid |unique_id| is sent over the Mojo
  // channel.

  FakeMojoMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;
  blink::mojom::BackgroundFetchError error;
  Abort(/* service_worker_registration_id= */ 42, kExampleDeveloperId,
        /* unique_id= */ "not a GUID", &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_ARGUMENT);
  EXPECT_EQ("Invalid unique_id", bad_message_observer.WaitForBadMessage());
}

TEST_F(BackgroundFetchServiceTest, AbortUnknownUniqueId) {
  // This test verifies that aborting a Background Fetch registration with a
  // |unique_id| that does not correspond to an active fetch kindly tells us so.

  blink::mojom::BackgroundFetchError error;
  Abort(/* service_worker_registration_id= */ 42, kExampleDeveloperId,
        kExampleUniqueId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_ID);
}

TEST_F(BackgroundFetchServiceTest, AbortEventDispatch) {
  // Tests that the `backgroundfetchabort` event will be fired when a Background
  // Fetch registration has been aborted by either the user or developer.

  auto* worker =
      embedded_worker_test_helper()
          ->AddNewPendingServiceWorker<BackgroundFetchTestServiceWorker>(
              embedded_worker_test_helper());
  int64_t service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            service_worker_registration_id);

  // base::RunLoop that we'll run until the event has been dispatched. If this
  // test times out, it means that the event could not be dispatched.
  base::RunLoop event_dispatched_loop;
  worker->set_abort_event_closure(event_dispatched_loop.QuitClosure());

  constexpr int kResponseCode = 200;

  std::vector<blink::mojom::FetchAPIRequestPtr> requests;
  requests.push_back(CreateRequestWithProvidedResponse(
      "GET", GURL("https://example.com/funny_cat.txt"),
      TestResponseBuilder(kResponseCode)
          .SetResponseData("Random data about a funny cat.")
          .Build()));

  // Create the registration with the given |requests|.
  BackgroundFetchRegistrationId registration_id;
  {
    auto options = blink::mojom::BackgroundFetchOptions::New();

    blink::mojom::BackgroundFetchError error;
    blink::mojom::BackgroundFetchRegistration registration;

    // Create the registration. This must succeed.
    registration_id = Fetch(service_worker_registration_id, kExampleDeveloperId,
                            std::move(requests), std::move(options), SkBitmap(),
                            &error, &registration);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  // Immediately abort the request created for the |registration_id|. Then wait
  // for the `backgroundfetchabort` event to have been invoked.
  {
    blink::mojom::BackgroundFetchError error;

    Abort(service_worker_registration_id, kExampleDeveloperId,
          registration_id.unique_id(), &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  event_dispatched_loop.Run();

  ASSERT_TRUE(worker->last_registration());
  EXPECT_EQ(kExampleDeveloperId, worker->last_registration()->developer_id);
}

TEST_F(BackgroundFetchServiceTest, UniqueId) {
  // Tests that Abort() and UpdateUI() update the correct Background Fetch
  // registration, according to the registration's |unique_id|, rather than
  // keying off the |developer_id| provided by JavaScript, since multiple
  // registrations can share an |developer_id| if JavaScript holds a reference
  // to a blink::mojom::BackgroundFetchRegistration object after that
  // registration is completed/failed/aborted and then creates a new
  // registration with the same |developer_id|.

  int64_t service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            service_worker_registration_id);

  std::vector<blink::mojom::FetchAPIRequestPtr> requests;
  requests.push_back(CreateDefaultRequest());

  blink::mojom::BackgroundFetchError error;

  // Create the first registration, that we will soon abort.
  auto aborted_options = blink::mojom::BackgroundFetchOptions::New();
  aborted_options->title = "Aborted";
  blink::mojom::BackgroundFetchRegistration aborted_registration;
  BackgroundFetchRegistrationId aborted_registration_id =
      Fetch(service_worker_registration_id, kExampleDeveloperId,
            CloneRequestVector(requests), std::move(aborted_options),
            SkBitmap(), &error, &aborted_registration);
  ASSERT_EQ(blink::mojom::BackgroundFetchError::NONE, error);

  // Immediately abort the registration so it is no longer active (everything
  // that follows should behave the same if the registration had completed
  // instead of being aborted).
  Abort(service_worker_registration_id, kExampleDeveloperId,
        aborted_registration_id.unique_id(), &error);
  ASSERT_EQ(blink::mojom::BackgroundFetchError::NONE, error);
  // Wait for response of the Mojo IPC to dispatch BackgroundFetchAbortEvent.
  base::RunLoop().RunUntilIdle();

  // Create a second registration sharing the same |developer_id|. Should
  // succeed.
  auto second_options = blink::mojom::BackgroundFetchOptions::New();
  second_options->title = "Second";
  blink::mojom::BackgroundFetchRegistration second_registration;
  BackgroundFetchRegistrationId second_registration_id = Fetch(
      service_worker_registration_id, kExampleDeveloperId, std::move(requests),
      std::move(second_options), SkBitmap(), &error, &second_registration);
  EXPECT_EQ(blink::mojom::BackgroundFetchError::NONE, error);

  // Now try to get the registration using its |developer_id|. This should
  // return the second registration since that is the active one.
  blink::mojom::BackgroundFetchRegistration gotten_registration;
  GetRegistration(service_worker_registration_id, kExampleDeveloperId, &error,
                  &gotten_registration);
  EXPECT_EQ(blink::mojom::BackgroundFetchError::NONE, error);
  EXPECT_EQ(second_registration.unique_id, gotten_registration.unique_id);

  // Calling UpdateUI for the second registration should succeed, and update the
  // title of the second registration only.
  std::string updated_second_registration_title = "Foo";
  UpdateUI(second_registration_id.service_worker_registration_id(),
           second_registration_id.developer_id(),
           second_registration_id.unique_id(),
           updated_second_registration_title, &error);
  EXPECT_EQ(blink::mojom::BackgroundFetchError::NONE, error);

  // Verify that the second registration's title was indeed updated, and that it
  // wasn't affected by the subsequent call to UpdateUI for the aborted
  // registration, by getting the second registration again.
  GetRegistration(service_worker_registration_id, kExampleDeveloperId, &error,
                  &gotten_registration);
  EXPECT_EQ(blink::mojom::BackgroundFetchError::NONE, error);
  EXPECT_EQ(second_registration.unique_id, gotten_registration.unique_id);

  // Aborting the previously aborted registration should fail with INVALID_ID
  // since it is no longer active.
  Abort(service_worker_registration_id, kExampleDeveloperId,
        aborted_registration_id.unique_id(), &error);
  EXPECT_EQ(blink::mojom::BackgroundFetchError::INVALID_ID, error);
  // Wait for response of the Mojo IPC to dispatch BackgroundFetchAbortEvent.
  // (MockBackgroundFetchDelegate won't complete/fail second_registration in the
  // meantime, since this test deliberately doesn't register a response).
  base::RunLoop().RunUntilIdle();

  // Getting the second registration should still succeed.
  GetRegistration(service_worker_registration_id, kExampleDeveloperId, &error,
                  &gotten_registration);
  EXPECT_EQ(blink::mojom::BackgroundFetchError::NONE, error);
  EXPECT_EQ(second_registration.unique_id, gotten_registration.unique_id);

  // Aborting the second registration should succeed.
  Abort(service_worker_registration_id, kExampleDeveloperId,
        second_registration_id.unique_id(), &error);
  EXPECT_EQ(blink::mojom::BackgroundFetchError::NONE, error);
  // Wait for response of the Mojo IPC to dispatch BackgroundFetchAbortEvent.
  // (MockBackgroundFetchDelegate won't complete/fail second_registration in the
  // meantime, since this test deliberately doesn't register a response).
  base::RunLoop().RunUntilIdle();

  // Getting the second registration should now fail as it is no longer active.
  GetRegistration(service_worker_registration_id, kExampleDeveloperId, &error,
                  &gotten_registration);
  EXPECT_EQ(blink::mojom::BackgroundFetchError::INVALID_ID, error);
}

TEST_F(BackgroundFetchServiceTest, GetDeveloperIds) {
  // This test verifies that the list of active |developer_id|s can be retrieved
  // from the service for a given Service Worker, as extracted from a
  // registration.

  int64_t service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            service_worker_registration_id);

  std::vector<blink::mojom::FetchAPIRequestPtr> requests;
  requests.push_back(CreateDefaultRequest());

  auto options = blink::mojom::BackgroundFetchOptions::New();

  // Verify that there are no active |developer_id|s yet.
  {
    blink::mojom::BackgroundFetchError error;
    std::vector<std::string> developer_ids;

    GetDeveloperIds(service_worker_registration_id, &error, &developer_ids);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

    ASSERT_EQ(developer_ids.size(), 0u);
  }

  // Start the Background Fetch for the |registration_id|.
  {
    blink::mojom::BackgroundFetchError error;
    blink::mojom::BackgroundFetchRegistration registration;

    Fetch(service_worker_registration_id, kExampleDeveloperId,
          CloneRequestVector(requests), options.Clone(), SkBitmap(), &error,
          &registration);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  // Verify that there is a single active fetch (the one we just started).
  {
    blink::mojom::BackgroundFetchError error;
    std::vector<std::string> developer_ids;

    GetDeveloperIds(service_worker_registration_id, &error, &developer_ids);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

    ASSERT_EQ(developer_ids.size(), 1u);
    EXPECT_EQ(developer_ids[0], kExampleDeveloperId);
  }

  // Start the Background Fetch for the |second_registration_id|.
  {
    blink::mojom::BackgroundFetchError error;
    blink::mojom::BackgroundFetchRegistration registration;

    Fetch(service_worker_registration_id, kAlternativeDeveloperId,
          std::move(requests), std::move(options), SkBitmap(), &error,
          &registration);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  // Verify that there are two active fetches.
  {
    blink::mojom::BackgroundFetchError error;
    std::vector<std::string> developer_ids;

    GetDeveloperIds(service_worker_registration_id, &error, &developer_ids);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

    ASSERT_EQ(developer_ids.size(), 2u);

    // Both |developer_id|s should be present, in either order.
    EXPECT_TRUE(developer_ids[0] == kExampleDeveloperId ||
                developer_ids[1] == kExampleDeveloperId);
    EXPECT_TRUE(developer_ids[0] == kAlternativeDeveloperId ||
                developer_ids[1] == kAlternativeDeveloperId);
  }

  // Verify that using the wrong origin does not return developer ids even if
  // the service worker registration is correct.
  {
    ScopedCustomBackgroundFetchService scoped_bogus_url_service(
        this, url::Origin::Create(GURL("https://www.bogus-origin.com")));
    blink::mojom::BackgroundFetchError error;
    std::vector<std::string> developer_ids;

    GetDeveloperIds(service_worker_registration_id, &error, &developer_ids);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

    // TODO(crbug.com/850076): The Storage Worker Database access is not
    // checking the origin. In a non-test environment this won't happen since a
    // ServiceWorker registration ID is tied to the origin.
    ASSERT_EQ(developer_ids.size(), 2u);
  }

  // Verify that using the wrong service worker id does not return developer ids
  // even if the origin is correct.
  {
    blink::mojom::BackgroundFetchError error;
    std::vector<std::string> developer_ids;

    int64_t bogus_service_worker_registration_id =
        service_worker_registration_id + 1;

    GetDeveloperIds(bogus_service_worker_registration_id, &error,
                    &developer_ids);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

    ASSERT_EQ(developer_ids.size(), 0u);
  }
}

TEST_F(BackgroundFetchServiceTest, UnregisterServiceWorker) {
  // This test registers a service worker, and calls fetch, but unregisters the
  // service worker before fetch has finished. We then verify that the
  // appropriate error is returned from Fetch().

  int64_t service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            service_worker_registration_id);

  std::vector<blink::mojom::FetchAPIRequestPtr> requests;
  requests.push_back(CreateDefaultRequest());

  auto options = blink::mojom::BackgroundFetchOptions::New();
  options->icons.push_back(
      CreateIcon("funny_cat.png", {{256, 256}}, "image/png"));
  options->icons.push_back(
      CreateIcon("silly_cat.gif", {{512, 512}}, "image/gif"));
  options->title = "My Background Fetch!";
  options->download_total = 9001;

  blink::mojom::BackgroundFetchError error;
  blink::mojom::BackgroundFetchRegistration registration;

  {
    using blink::mojom::BackgroundFetchError;
    EXPECT_CALL(*this,
                OnRegistrationDeleted(service_worker_registration_id, _));
    FetchAndUnregisterServiceWorker(service_worker_registration_id,
                                    kExampleDeveloperId, std::move(requests),
                                    std::move(options), SkBitmap(), &error,
                                    &registration);
    // There's a race condition between aborting due to unregistering the
    // Service Worker, and a STORAGE_ERROR caused by the DatabaseTask as a
    // result.
    ASSERT_TRUE(error == BackgroundFetchError::STORAGE_ERROR ||
                error == BackgroundFetchError::SERVICE_WORKER_UNAVAILABLE);
  }
}

TEST_F(BackgroundFetchServiceTest, JobsInitializedOnBrowserRestart) {
  int64_t service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            service_worker_registration_id);

  std::vector<blink::mojom::FetchAPIRequestPtr> requests;
  requests.push_back(CreateDefaultRequest());
  auto options = blink::mojom::BackgroundFetchOptions::New();
  blink::mojom::BackgroundFetchError error;
  blink::mojom::BackgroundFetchRegistration registration;

  // Start the fetch. The request is indefinitley pending so this will never
  // finish.
  BackgroundFetchRegistrationId registration_id;
  {
    EXPECT_CALL(*this, OnRegistrationCreated(_, _, _, _, _, _));
    registration_id = Fetch(service_worker_registration_id, kExampleDeveloperId,
                            std::move(requests), std::move(options), SkBitmap(),
                            &error, &registration);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  // Check that the registration is in the DB.
  {
    std::vector<std::string> developer_ids;
    GetDeveloperIds(service_worker_registration_id, &error, &developer_ids);
    EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
    ASSERT_EQ(developer_ids.size(), 1u);
    EXPECT_EQ(developer_ids[0], kExampleDeveloperId);
  }

  auto context = context_->service_worker_context_;
  // Simulate browser restart by re-creating |context_| and |service_|.
  TearDown();
  SetUp();

  // Overwrite pending request with a completable version.
  CreateRequestWithProvidedResponse(
      "GET", GURL("https://example.com/funny_cat.txt"),
      TestResponseBuilder(200)
          .SetResponseData("This request is no longer indefinitely pending.")
          .Build());

  {
    // The same BackgroundFetchRegistrationId is loaded.
    EXPECT_CALL(*this, OnRegistrationLoadedAtStartup(registration_id, _, _, _,
                                                     _, _, _));
    // Allow restart process to go through.
    thread_bundle_.RunUntilIdle();
  }

  // Check that the registration is not in the DB, which means it completed.
  {
    std::vector<std::string> developer_ids;
    GetDeveloperIds(service_worker_registration_id, &error, &developer_ids);
    EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
    ASSERT_EQ(developer_ids.size(), 0u);
  }
}

}  // namespace content
