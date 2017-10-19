// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/ThreadableLoader.h"

#include <memory>
#include "core/dom/TaskRunnerHelper.h"
#include "core/loader/DocumentThreadableLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "core/loader/ThreadableLoadingContext.h"
#include "core/loader/WorkerThreadableLoader.h"
#include "core/testing/DummyPageHolder.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerThreadTestHelper.h"
#include "platform/WaitableEvent.h"
#include "platform/geometry/IntSize.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/loader/fetch/ResourceError.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/loader/fetch/ResourceTimingInfo.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoadTiming.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLResponse.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

using ::testing::_;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::StrEq;
using ::testing::Truly;
using Checkpoint = ::testing::StrictMock<::testing::MockFunction<void(int)>>;

constexpr char kFileName[] = "fox-null-terminated.html";

class MockThreadableLoaderClient : public ThreadableLoaderClient {
 public:
  static std::unique_ptr<MockThreadableLoaderClient> Create() {
    return WTF::WrapUnique(
        new ::testing::StrictMock<MockThreadableLoaderClient>);
  }
  MOCK_METHOD2(DidSendData, void(unsigned long long, unsigned long long));
  MOCK_METHOD3(DidReceiveResponseMock,
               void(unsigned long,
                    const ResourceResponse&,
                    WebDataConsumerHandle*));
  void DidReceiveResponse(unsigned long identifier,
                          const ResourceResponse& response,
                          std::unique_ptr<WebDataConsumerHandle> handle) {
    DidReceiveResponseMock(identifier, response, handle.get());
  }
  MOCK_METHOD2(DidReceiveData, void(const char*, unsigned));
  MOCK_METHOD2(DidReceiveCachedMetadata, void(const char*, int));
  MOCK_METHOD2(DidFinishLoading, void(unsigned long, double));
  MOCK_METHOD1(DidFail, void(const ResourceError&));
  MOCK_METHOD0(DidFailRedirectCheck, void());
  MOCK_METHOD1(DidReceiveResourceTiming, void(const ResourceTimingInfo&));
  MOCK_METHOD1(DidDownloadData, void(int));

 protected:
  MockThreadableLoaderClient() = default;
};

bool IsCancellation(const ResourceError& error) {
  return error.IsCancellation();
}

bool IsNotCancellation(const ResourceError& error) {
  return !error.IsCancellation();
}

KURL SuccessURL() {
  return KURL(NullURL(), "http://example.com/success");
}
KURL ErrorURL() {
  return KURL(NullURL(), "http://example.com/error");
}
KURL RedirectURL() {
  return KURL(NullURL(), "http://example.com/redirect");
}
KURL RedirectLoopURL() {
  return KURL(NullURL(), "http://example.com/loop");
}

enum ThreadableLoaderToTest {
  kDocumentThreadableLoaderTest,
  kWorkerThreadableLoaderTest,
};

class ThreadableLoaderTestHelper {
 public:
  virtual ~ThreadableLoaderTestHelper() {}

  virtual void CreateLoader(ThreadableLoaderClient*) = 0;
  virtual void StartLoader(const ResourceRequest&) = 0;
  virtual void CancelLoader() = 0;
  virtual void CancelAndClearLoader() = 0;
  virtual void ClearLoader() = 0;
  virtual Checkpoint& GetCheckpoint() = 0;
  virtual void CallCheckpoint(int) = 0;
  virtual void OnSetUp() = 0;
  virtual void OnServeRequests() = 0;
  virtual void OnTearDown() = 0;
};

class DocumentThreadableLoaderTestHelper : public ThreadableLoaderTestHelper {
 public:
  DocumentThreadableLoaderTestHelper()
      : dummy_page_holder_(DummyPageHolder::Create(IntSize(1, 1))) {}

  void CreateLoader(ThreadableLoaderClient* client) override {
    ThreadableLoaderOptions options;
    ResourceLoaderOptions resource_loader_options;
    loader_ = DocumentThreadableLoader::Create(
        *ThreadableLoadingContext::Create(GetDocument()), client, options,
        resource_loader_options);
  }

  void StartLoader(const ResourceRequest& request) override {
    loader_->Start(request);
  }

  void CancelLoader() override { loader_->Cancel(); }
  void CancelAndClearLoader() override {
    loader_->Cancel();
    loader_ = nullptr;
  }
  void ClearLoader() override { loader_ = nullptr; }
  Checkpoint& GetCheckpoint() override { return checkpoint_; }
  void CallCheckpoint(int n) override { checkpoint_.Call(n); }

  void OnSetUp() override {}

  void OnServeRequests() override {}

  void OnTearDown() override {
    if (loader_) {
      loader_->Cancel();
      loader_ = nullptr;
    }
  }

 private:
  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  Checkpoint checkpoint_;
  Persistent<DocumentThreadableLoader> loader_;
};

class WorkerThreadableLoaderTestHelper : public ThreadableLoaderTestHelper {
 public:
  WorkerThreadableLoaderTestHelper()
      : dummy_page_holder_(DummyPageHolder::Create(IntSize(1, 1))) {}

  void CreateLoader(ThreadableLoaderClient* client) override {
    std::unique_ptr<WaitableEvent> completion_event =
        WTF::MakeUnique<WaitableEvent>();
    worker_loading_task_runner_->PostTask(
        BLINK_FROM_HERE,
        CrossThreadBind(&WorkerThreadableLoaderTestHelper::WorkerCreateLoader,
                        CrossThreadUnretained(this),
                        CrossThreadUnretained(client),
                        CrossThreadUnretained(completion_event.get())));
    completion_event->Wait();
  }

  void StartLoader(const ResourceRequest& request) override {
    std::unique_ptr<WaitableEvent> completion_event =
        WTF::MakeUnique<WaitableEvent>();
    worker_loading_task_runner_->PostTask(
        BLINK_FROM_HERE,
        CrossThreadBind(&WorkerThreadableLoaderTestHelper::WorkerStartLoader,
                        CrossThreadUnretained(this),
                        CrossThreadUnretained(completion_event.get()),
                        request));
    completion_event->Wait();
  }

  // Must be called on the worker thread.
  void CancelLoader() override {
    DCHECK(worker_thread_);
    DCHECK(worker_thread_->IsCurrentThread());
    loader_->Cancel();
  }

  void CancelAndClearLoader() override {
    DCHECK(worker_thread_);
    DCHECK(worker_thread_->IsCurrentThread());
    loader_->Cancel();
    loader_ = nullptr;
  }

  // Must be called on the worker thread.
  void ClearLoader() override {
    DCHECK(worker_thread_);
    DCHECK(worker_thread_->IsCurrentThread());
    loader_ = nullptr;
  }

  Checkpoint& GetCheckpoint() override { return checkpoint_; }

  void CallCheckpoint(int n) override {
    testing::RunPendingTasks();

    std::unique_ptr<WaitableEvent> completion_event =
        WTF::MakeUnique<WaitableEvent>();
    worker_loading_task_runner_->PostTask(
        BLINK_FROM_HERE,
        CrossThreadBind(&WorkerThreadableLoaderTestHelper::WorkerCallCheckpoint,
                        CrossThreadUnretained(this),
                        CrossThreadUnretained(completion_event.get()), n));
    completion_event->Wait();
  }

  void OnSetUp() override {
    reporting_proxy_ = WTF::MakeUnique<WorkerReportingProxy>();
    security_origin_ = GetDocument().GetSecurityOrigin();
    parent_frame_task_runners_ =
        ParentFrameTaskRunners::Create(dummy_page_holder_->GetFrame());
    worker_thread_ = WTF::MakeUnique<WorkerThreadForTest>(
        ThreadableLoadingContext::Create(GetDocument()), *reporting_proxy_);

    worker_thread_->StartWithSourceCode(security_origin_.get(),
                                        "//fake source code",
                                        parent_frame_task_runners_.Get());
    worker_thread_->WaitForInit();
    worker_loading_task_runner_ =
        TaskRunnerHelper::Get(TaskType::kUnspecedLoading, worker_thread_.get());
  }

  void OnServeRequests() override { testing::RunPendingTasks(); }

  void OnTearDown() override {
    worker_loading_task_runner_->PostTask(
        BLINK_FROM_HERE,
        CrossThreadBind(&WorkerThreadableLoaderTestHelper::ClearLoader,
                        CrossThreadUnretained(this)));
    WaitableEvent event;
    worker_loading_task_runner_->PostTask(
        BLINK_FROM_HERE,
        CrossThreadBind(&WaitableEvent::Signal, CrossThreadUnretained(&event)));
    event.Wait();
    worker_thread_->Terminate();
    worker_thread_->WaitForShutdownForTesting();

    // Needed to clean up the things on the main thread side and
    // avoid Resource leaks.
    testing::RunPendingTasks();
  }

 private:
  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

  void WorkerCreateLoader(ThreadableLoaderClient* client,
                          WaitableEvent* event) {
    DCHECK(worker_thread_);
    DCHECK(worker_thread_->IsCurrentThread());

    ThreadableLoaderOptions options;
    ResourceLoaderOptions resource_loader_options;

    // Ensure that WorkerThreadableLoader is created.
    // ThreadableLoader::create() determines whether it should create
    // a DocumentThreadableLoader or WorkerThreadableLoader based on
    // isWorkerGlobalScope().
    DCHECK(worker_thread_->GlobalScope()->IsWorkerGlobalScope());

    loader_ = ThreadableLoader::Create(*worker_thread_->GlobalScope(), client,
                                       options, resource_loader_options);
    DCHECK(loader_);
    event->Signal();
  }

  void WorkerStartLoader(
      WaitableEvent* event,
      std::unique_ptr<CrossThreadResourceRequestData> request_data) {
    DCHECK(worker_thread_);
    DCHECK(worker_thread_->IsCurrentThread());

    ResourceRequest request(request_data.get());
    request.SetFetchCredentialsMode(WebURLRequest::kFetchCredentialsModeOmit);
    loader_->Start(request);
    event->Signal();
  }

  void WorkerCallCheckpoint(WaitableEvent* event, int n) {
    DCHECK(worker_thread_);
    DCHECK(worker_thread_->IsCurrentThread());
    checkpoint_.Call(n);
    event->Signal();
  }

  scoped_refptr<SecurityOrigin> security_origin_;
  std::unique_ptr<WorkerReportingProxy> reporting_proxy_;
  std::unique_ptr<WorkerThreadForTest> worker_thread_;

  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  // Accessed cross-thread when worker thread posts tasks to the parent.
  CrossThreadPersistent<ParentFrameTaskRunners> parent_frame_task_runners_;
  scoped_refptr<WebTaskRunner> worker_loading_task_runner_;
  Checkpoint checkpoint_;
  // |m_loader| must be touched only from the worker thread only.
  CrossThreadPersistent<ThreadableLoader> loader_;
};

class ThreadableLoaderTest
    : public ::testing::TestWithParam<ThreadableLoaderToTest> {
 public:
  ThreadableLoaderTest() {
    switch (GetParam()) {
      case kDocumentThreadableLoaderTest:
        helper_ = WTF::WrapUnique(new DocumentThreadableLoaderTestHelper);
        break;
      case kWorkerThreadableLoaderTest:
        helper_ = WTF::WrapUnique(new WorkerThreadableLoaderTestHelper());
        break;
    }
  }

  void StartLoader(const KURL& url,
                   WebURLRequest::FetchRequestMode fetch_request_mode =
                       WebURLRequest::kFetchRequestModeNoCORS) {
    ResourceRequest request(url);
    request.SetRequestContext(WebURLRequest::kRequestContextObject);
    request.SetFetchRequestMode(fetch_request_mode);
    request.SetFetchCredentialsMode(WebURLRequest::kFetchCredentialsModeOmit);
    helper_->StartLoader(request);
  }

  void CancelLoader() { helper_->CancelLoader(); }
  void CancelAndClearLoader() { helper_->CancelAndClearLoader(); }
  void ClearLoader() { helper_->ClearLoader(); }
  Checkpoint& GetCheckpoint() { return helper_->GetCheckpoint(); }
  void CallCheckpoint(int n) { helper_->CallCheckpoint(n); }

  void ServeRequests() {
    helper_->OnServeRequests();
    Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  }

  void CreateLoader() { helper_->CreateLoader(Client()); }

  MockThreadableLoaderClient* Client() const { return client_.get(); }

 private:
  void SetUp() override {
    SetUpSuccessURL();
    SetUpErrorURL();
    SetUpRedirectURL();
    SetUpRedirectLoopURL();

    client_ = MockThreadableLoaderClient::Create();
    helper_->OnSetUp();
  }

  void TearDown() override {
    helper_->OnTearDown();
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
    client_.reset();
  }

  void SetUpSuccessURL() {
    URLTestHelpers::RegisterMockedURLLoad(
        SuccessURL(), testing::CoreTestDataPath(kFileName), "text/html");
  }

  void SetUpErrorURL() {
    URLTestHelpers::RegisterMockedErrorURLLoad(ErrorURL());
  }

  void SetUpRedirectURL() {
    KURL url = RedirectURL();

    WebURLLoadTiming timing;
    timing.Initialize();

    WebURLResponse response;
    response.SetURL(url);
    response.SetHTTPStatusCode(301);
    response.SetLoadTiming(timing);
    response.AddHTTPHeaderField("Location", SuccessURL().GetString());
    response.AddHTTPHeaderField("Access-Control-Allow-Origin", "null");

    URLTestHelpers::RegisterMockedURLLoadWithCustomResponse(
        url, testing::CoreTestDataPath(kFileName), response);
  }

  void SetUpRedirectLoopURL() {
    KURL url = RedirectLoopURL();

    WebURLLoadTiming timing;
    timing.Initialize();

    WebURLResponse response;
    response.SetURL(url);
    response.SetHTTPStatusCode(301);
    response.SetLoadTiming(timing);
    response.AddHTTPHeaderField("Location", RedirectLoopURL().GetString());
    response.AddHTTPHeaderField("Access-Control-Allow-Origin", "null");

    URLTestHelpers::RegisterMockedURLLoadWithCustomResponse(
        url, testing::CoreTestDataPath(kFileName), response);
  }

  std::unique_ptr<MockThreadableLoaderClient> client_;
  std::unique_ptr<ThreadableLoaderTestHelper> helper_;
};

INSTANTIATE_TEST_CASE_P(Document,
                        ThreadableLoaderTest,
                        ::testing::Values(kDocumentThreadableLoaderTest));

INSTANTIATE_TEST_CASE_P(Worker,
                        ThreadableLoaderTest,
                        ::testing::Values(kWorkerThreadableLoaderTest));

TEST_P(ThreadableLoaderTest, StartAndStop) {}

TEST_P(ThreadableLoaderTest, CancelAfterStart) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2))
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::CancelLoader));
  EXPECT_CALL(*Client(), DidFail(Truly(IsCancellation)));
  EXPECT_CALL(GetCheckpoint(), Call(3));

  StartLoader(SuccessURL());
  CallCheckpoint(2);
  CallCheckpoint(3);
  ServeRequests();
}

TEST_P(ThreadableLoaderTest, CancelAndClearAfterStart) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2))
      .WillOnce(
          InvokeWithoutArgs(this, &ThreadableLoaderTest::CancelAndClearLoader));
  EXPECT_CALL(*Client(), DidFail(Truly(IsCancellation)));
  EXPECT_CALL(GetCheckpoint(), Call(3));

  StartLoader(SuccessURL());
  CallCheckpoint(2);
  CallCheckpoint(3);
  ServeRequests();
}

TEST_P(ThreadableLoaderTest, CancelInDidReceiveResponse) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidReceiveResponseMock(_, _, _))
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::CancelLoader));
  EXPECT_CALL(*Client(), DidFail(Truly(IsCancellation)));

  StartLoader(SuccessURL());
  CallCheckpoint(2);
  ServeRequests();
}

TEST_P(ThreadableLoaderTest, CancelAndClearInDidReceiveResponse) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidReceiveResponseMock(_, _, _))
      .WillOnce(
          InvokeWithoutArgs(this, &ThreadableLoaderTest::CancelAndClearLoader));
  EXPECT_CALL(*Client(), DidFail(Truly(IsCancellation)));

  StartLoader(SuccessURL());
  CallCheckpoint(2);
  ServeRequests();
}

TEST_P(ThreadableLoaderTest, CancelInDidReceiveData) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidReceiveResponseMock(_, _, _));
  EXPECT_CALL(*Client(), DidReceiveData(_, _))
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::CancelLoader));
  EXPECT_CALL(*Client(), DidFail(Truly(IsCancellation)));

  StartLoader(SuccessURL());
  CallCheckpoint(2);
  ServeRequests();
}

TEST_P(ThreadableLoaderTest, CancelAndClearInDidReceiveData) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidReceiveResponseMock(_, _, _));
  EXPECT_CALL(*Client(), DidReceiveData(_, _))
      .WillOnce(
          InvokeWithoutArgs(this, &ThreadableLoaderTest::CancelAndClearLoader));
  EXPECT_CALL(*Client(), DidFail(Truly(IsCancellation)));

  StartLoader(SuccessURL());
  CallCheckpoint(2);
  ServeRequests();
}

TEST_P(ThreadableLoaderTest, DidFinishLoading) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidReceiveResponseMock(_, _, _));
  EXPECT_CALL(*Client(), DidReceiveData(StrEq("fox"), 4));
  // We expect didReceiveResourceTiming() calls in DocumentThreadableLoader;
  // it's used to connect DocumentThreadableLoader to WorkerThreadableLoader,
  // not to ThreadableLoaderClient.
  EXPECT_CALL(*Client(), DidReceiveResourceTiming(_));
  EXPECT_CALL(*Client(), DidFinishLoading(_, _));

  StartLoader(SuccessURL());
  CallCheckpoint(2);
  ServeRequests();
}

TEST_P(ThreadableLoaderTest, CancelInDidFinishLoading) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidReceiveResponseMock(_, _, _));
  EXPECT_CALL(*Client(), DidReceiveData(_, _));
  EXPECT_CALL(*Client(), DidReceiveResourceTiming(_));
  EXPECT_CALL(*Client(), DidFinishLoading(_, _))
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::CancelLoader));

  StartLoader(SuccessURL());
  CallCheckpoint(2);
  ServeRequests();
}

TEST_P(ThreadableLoaderTest, ClearInDidFinishLoading) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidReceiveResponseMock(_, _, _));
  EXPECT_CALL(*Client(), DidReceiveData(_, _));
  EXPECT_CALL(*Client(), DidReceiveResourceTiming(_));
  EXPECT_CALL(*Client(), DidFinishLoading(_, _))
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::ClearLoader));

  StartLoader(SuccessURL());
  CallCheckpoint(2);
  ServeRequests();
}

TEST_P(ThreadableLoaderTest, DidFail) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidReceiveResponseMock(_, _, _));
  EXPECT_CALL(*Client(), DidFail(Truly(IsNotCancellation)));

  StartLoader(ErrorURL());
  CallCheckpoint(2);
  ServeRequests();
}

TEST_P(ThreadableLoaderTest, CancelInDidFail) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidReceiveResponseMock(_, _, _));
  EXPECT_CALL(*Client(), DidFail(_))
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::CancelLoader));

  StartLoader(ErrorURL());
  CallCheckpoint(2);
  ServeRequests();
}

TEST_P(ThreadableLoaderTest, ClearInDidFail) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidReceiveResponseMock(_, _, _));
  EXPECT_CALL(*Client(), DidFail(_))
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::ClearLoader));

  StartLoader(ErrorURL());
  CallCheckpoint(2);
  ServeRequests();
}

TEST_P(ThreadableLoaderTest, DidFailInStart) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(*Client(), DidFail(ResourceError::CancelledDueToAccessCheckError(
                             ErrorURL(), ResourceRequestBlockedReason::kOther,
                             "Cross origin requests are not supported.")));
  EXPECT_CALL(GetCheckpoint(), Call(2));

  StartLoader(ErrorURL(), WebURLRequest::kFetchRequestModeSameOrigin);
  CallCheckpoint(2);
  ServeRequests();
}

TEST_P(ThreadableLoaderTest, CancelInDidFailInStart) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(*Client(), DidFail(_))
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::CancelLoader));
  EXPECT_CALL(GetCheckpoint(), Call(2));

  StartLoader(ErrorURL(), WebURLRequest::kFetchRequestModeSameOrigin);
  CallCheckpoint(2);
  ServeRequests();
}

TEST_P(ThreadableLoaderTest, ClearInDidFailInStart) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(*Client(), DidFail(_))
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::ClearLoader));
  EXPECT_CALL(GetCheckpoint(), Call(2));

  StartLoader(ErrorURL(), WebURLRequest::kFetchRequestModeSameOrigin);
  CallCheckpoint(2);
  ServeRequests();
}

TEST_P(ThreadableLoaderTest, DidFailAccessControlCheck) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(
      *Client(),
      DidFail(ResourceError::CancelledDueToAccessCheckError(
          SuccessURL(), ResourceRequestBlockedReason::kOther,
          "No 'Access-Control-Allow-Origin' header is present on the requested "
          "resource. Origin 'null' is therefore not allowed access.")));

  StartLoader(SuccessURL(), WebURLRequest::kFetchRequestModeCORS);
  CallCheckpoint(2);
  ServeRequests();
}

TEST_P(ThreadableLoaderTest, RedirectDidFinishLoading) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidReceiveResponseMock(_, _, _));
  EXPECT_CALL(*Client(), DidReceiveData(StrEq("fox"), 4));
  EXPECT_CALL(*Client(), DidReceiveResourceTiming(_));
  EXPECT_CALL(*Client(), DidFinishLoading(_, _));

  StartLoader(RedirectURL());
  CallCheckpoint(2);
  ServeRequests();
}

TEST_P(ThreadableLoaderTest, CancelInRedirectDidFinishLoading) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidReceiveResponseMock(_, _, _));
  EXPECT_CALL(*Client(), DidReceiveData(StrEq("fox"), 4));
  EXPECT_CALL(*Client(), DidReceiveResourceTiming(_));
  EXPECT_CALL(*Client(), DidFinishLoading(_, _))
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::CancelLoader));

  StartLoader(RedirectURL());
  CallCheckpoint(2);
  ServeRequests();
}

TEST_P(ThreadableLoaderTest, ClearInRedirectDidFinishLoading) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidReceiveResponseMock(_, _, _));
  EXPECT_CALL(*Client(), DidReceiveData(StrEq("fox"), 4));
  EXPECT_CALL(*Client(), DidReceiveResourceTiming(_));
  EXPECT_CALL(*Client(), DidFinishLoading(_, _))
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::ClearLoader));

  StartLoader(RedirectURL());
  CallCheckpoint(2);
  ServeRequests();
}

TEST_P(ThreadableLoaderTest, DidFailRedirectCheck) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidFailRedirectCheck());

  StartLoader(RedirectLoopURL(), WebURLRequest::kFetchRequestModeCORS);
  CallCheckpoint(2);
  ServeRequests();
}

TEST_P(ThreadableLoaderTest, CancelInDidFailRedirectCheck) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidFailRedirectCheck())
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::CancelLoader));

  StartLoader(RedirectLoopURL(), WebURLRequest::kFetchRequestModeCORS);
  CallCheckpoint(2);
  ServeRequests();
}

TEST_P(ThreadableLoaderTest, ClearInDidFailRedirectCheck) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(GetCheckpoint(), Call(2));
  EXPECT_CALL(*Client(), DidFailRedirectCheck())
      .WillOnce(InvokeWithoutArgs(this, &ThreadableLoaderTest::ClearLoader));

  StartLoader(RedirectLoopURL(), WebURLRequest::kFetchRequestModeCORS);
  CallCheckpoint(2);
  ServeRequests();
}

// This test case checks blink doesn't crash even when the response arrives
// synchronously.
TEST_P(ThreadableLoaderTest, GetResponseSynchronously) {
  InSequence s;
  EXPECT_CALL(GetCheckpoint(), Call(1));
  CreateLoader();
  CallCheckpoint(1);

  EXPECT_CALL(*Client(), DidFail(_));
  EXPECT_CALL(GetCheckpoint(), Call(2));

  // Currently didFailAccessControlCheck is dispatched synchronously. This
  // test is not saying that didFailAccessControlCheck should be dispatched
  // synchronously, but is saying that even when a response is served
  // synchronously it should not lead to a crash.
  StartLoader(KURL(NullURL(), "about:blank"),
              WebURLRequest::kFetchRequestModeCORS);
  CallCheckpoint(2);
}

}  // namespace

}  // namespace blink
