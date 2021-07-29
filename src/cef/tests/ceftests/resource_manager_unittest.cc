// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <memory>
#include <vector>

#include "include/base/cef_callback.h"
#include "include/base/cef_lock.h"
#include "include/cef_file_util.h"
#include "include/cef_waitable_event.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_resource_manager.h"
#include "include/wrapper/cef_scoped_temp_dir.h"
#include "include/wrapper/cef_stream_resource_handler.h"
#include "tests/ceftests/routing_test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/file_util.h"

namespace {

std::string CreateMessage(const std::string& name, const std::string& value) {
  return name + ":" + value;
}

// The returned contents execute a JavaScript callback containing |message|.
std::string CreateContents(const std::string& message) {
  return "<html><body>" + message +
         "<script>"
         "window.testQuery({request:'" +
         message +
         "'});"
         "</script></html></body>";
}

void WriteFile(const std::string& path, const std::string& contents) {
  int contents_size = static_cast<int>(contents.size());
  int write_ct =
      client::file_util::WriteFile(path, contents.data(), contents_size);
  EXPECT_EQ(contents_size, write_ct);
}

CefRefPtr<CefResourceHandler> CreateContentsResourceHandler(
    const std::string& message) {
  const std::string& contents = CreateContents(message);
  CefRefPtr<CefStreamReader> stream = CefStreamReader::CreateForData(
      static_cast<void*>(const_cast<char*>(contents.data())),
      contents.length());
  return new CefStreamResourceHandler("text/html", stream);
}

const char kDoneMsg[] = "ResourceManagerTestHandler.Done";
const char kNotHandled[] = "NotHandled";

// Browser side.
class ResourceManagerTestHandler : public RoutingTestHandler {
 public:
  struct State {
    State() : manager_(new CefResourceManager()), expected_message_ct_(0) {}

    CefRefPtr<CefResourceManager> manager_;

    // Set of URLs that will be loaded.
    std::vector<std::string> urls_;

    // Set of messages that were received.
    std::vector<std::string> messages_;

    // If non-zero the test will not complete until the expected number of
    // messages have been received.
    size_t expected_message_ct_;
  };

  explicit ResourceManagerTestHandler(State* state)
      : state_(state), current_url_(0) {
    EXPECT_TRUE(state_);
    EXPECT_TRUE(state_->manager_.get());
    EXPECT_TRUE(!state_->urls_.empty());
    EXPECT_TRUE(state_->messages_.empty());
  }

  void RunTest() override {
    // Create the browser.
    CreateBrowser(GetNextURL());

    SetTestTimeout(IsChromeRuntimeEnabled() ? 10000 : 5000);
  }

  cef_return_value_t OnBeforeResourceLoad(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefRequestCallback> callback) override {
    if (IsChromeRuntimeEnabled() && request->GetResourceType() == RT_FAVICON) {
      // Ignore favicon requests.
      return RV_CANCEL;
    }

    return state_->manager_->OnBeforeResourceLoad(browser, frame, request,
                                                  callback);
  }

  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
    // The ProviderDoNothing test destroys the manager before the handler.
    if (state_->manager_) {
      CefRefPtr<CefResourceHandler> handler =
          state_->manager_->GetResourceHandler(browser, frame, request);
      if (handler.get())
        return handler;
    }

    return CreateContentsResourceHandler(CreateMessage(kDoneMsg, kNotHandled));
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64 query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    state_->messages_.push_back(request);
    callback->Success("");

    CefPostTask(TID_UI, base::BindOnce(&ResourceManagerTestHandler::Continue,
                                       this, browser));

    return true;
  }

  // Wait a bit before destroying the test. Used with ProviderDoNothing.
  void DelayedDestroyTest() {
    CefPostDelayedTask(
        TID_UI, base::BindOnce(&ResourceManagerTestHandler::DestroyTest, this),
        100);
  }

 private:
  void Continue(CefRefPtr<CefBrowser> browser) {
    if (state_->expected_message_ct_ == 0) {
      // Load each URL sequentially.
      const std::string& next_url = GetNextURL();
      if (next_url.empty())
        DestroyTest();
      else
        browser->GetMainFrame()->LoadURL(next_url);
    } else if (state_->messages_.size() == state_->expected_message_ct_) {
      DestroyTest();
    }
  }

  std::string GetNextURL() {
    if (current_url_ >= state_->urls_.size())
      return std::string();
    return state_->urls_[current_url_++];
  }

  State* state_;
  size_t current_url_;

  IMPLEMENT_REFCOUNTING(ResourceManagerTestHandler);
};

}  // namespace

// Test with no providers.
TEST(ResourceManagerTest, NoProviders) {
  const char kUrl[] = "http://test.com/ResourceManagerTest";

  ResourceManagerTestHandler::State state;
  state.urls_.push_back(kUrl);

  CefRefPtr<ResourceManagerTestHandler> handler =
      new ResourceManagerTestHandler(&state);
  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);

  state.manager_ = nullptr;

  // Should get the message returned from GetResourceHandler when the request is
  // not handled.
  EXPECT_EQ(state.messages_.size(), 1U);
  EXPECT_EQ(CreateMessage(kDoneMsg, kNotHandled), state.messages_[0]);
}

namespace {

// Content provider that tracks execution state.
class TestProvider : public CefResourceManager::Provider {
 public:
  struct State {
    TrackCallback got_on_request_;
    TrackCallback got_on_request_canceled_;
    TrackCallback got_destruct_;
    base::OnceClosure destruct_callback_;
    std::string request_url_;
  };

  explicit TestProvider(State* state) : state_(state) { EXPECT_TRUE(state_); }

  ~TestProvider() {
    CEF_REQUIRE_IO_THREAD();
    state_->got_destruct_.yes();
    if (!state_->destruct_callback_.is_null())
      std::move(state_->destruct_callback_).Run();
  }

  bool OnRequest(scoped_refptr<CefResourceManager::Request> request) override {
    CEF_REQUIRE_IO_THREAD();
    EXPECT_FALSE(state_->got_on_request_);
    EXPECT_FALSE(state_->got_on_request_canceled_);

    state_->got_on_request_.yes();
    state_->request_url_ = request->url();

    return false;
  }

  void OnRequestCanceled(
      scoped_refptr<CefResourceManager::Request> request) override {
    CEF_REQUIRE_IO_THREAD();
    EXPECT_TRUE(state_->got_on_request_);
    EXPECT_FALSE(state_->got_on_request_canceled_);

    state_->got_on_request_canceled_.yes();
    EXPECT_STREQ(state_->request_url_.c_str(), request->url().c_str());
  }

 private:
  State* state_;

  DISALLOW_COPY_AND_ASSIGN(TestProvider);
};

// Helper that blocks on destruction of 1 or more TestProviders.
class ProviderDestructHelper {
 public:
  explicit ProviderDestructHelper(int expected_count)
      : expected_count_(expected_count),
        event_(CefWaitableEvent::CreateWaitableEvent(true, false)) {
    CHECK_GT(expected_count_, 0);
  }

  base::OnceClosure callback() {
    return base::BindOnce(
        [](ProviderDestructHelper* self) { self->DestructCallback(); },
        base::Unretained(this));
  }

  void Wait() { event_->Wait(); }

 private:
  void DestructCallback() {
    bool signal = false;

    {
      base::AutoLock lock_scope(lock_);
      CHECK_LT(current_count_, expected_count_);
      if (++current_count_ == expected_count_)
        signal = true;
    }

    if (signal) {
      // Don't access any members after calling Signal(), which causes |this| to
      // be deleted on a different thread.
      auto event = event_;
      event->Signal();
    }
  }

  const int expected_count_;
  int current_count_ = 0;
  CefRefPtr<CefWaitableEvent> event_;
  base::Lock lock_;
};

// Test that that the URL retrieved via Request::url() is parsed as expected.
// Fragment or query components in any order should be removed.
void TestUrlParsing(const char* kUrl) {
  const char kRequestUrl[] = "http://test.com/ResourceManagerTest";

  ResourceManagerTestHandler::State state;
  state.urls_.push_back(kUrl);

  TestProvider::State provider_state;

  ProviderDestructHelper destruct_helper(1);
  provider_state.destruct_callback_ = destruct_helper.callback();

  state.manager_->AddProvider(new TestProvider(&provider_state), 0,
                              std::string());

  CefRefPtr<ResourceManagerTestHandler> handler =
      new ResourceManagerTestHandler(&state);
  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);

  state.manager_ = nullptr;

  // Wait for the manager to be deleted.
  destruct_helper.Wait();

  // The provider is called.
  EXPECT_TRUE(provider_state.got_on_request_);
  EXPECT_FALSE(provider_state.got_on_request_canceled_);
  EXPECT_TRUE(provider_state.got_destruct_);

  // The expected URL is received.
  EXPECT_STREQ(kRequestUrl, provider_state.request_url_.c_str());

  // The request is not handled.
  EXPECT_EQ(state.messages_.size(), 1U);
  EXPECT_EQ(CreateMessage(kDoneMsg, kNotHandled), state.messages_[0]);
}

}  // namespace

TEST(ResourceManagerTest, UrlParsingNoQueryOrFragment) {
  TestUrlParsing("http://test.com/ResourceManagerTest");
}

TEST(ResourceManagerTest, UrlParsingWithQuery) {
  TestUrlParsing("http://test.com/ResourceManagerTest?foo=bar&choo=too");
}

TEST(ResourceManagerTest, UrlParsingWithFragment) {
  TestUrlParsing("http://test.com/ResourceManagerTest#some/fragment");
}

TEST(ResourceManagerTest, UrlParsingWithQueryAndFragment) {
  TestUrlParsing("http://test.com/ResourceManagerTest?foo=bar#some/fragment");
}

TEST(ResourceManagerTest, UrlParsingWithFragmentAndQuery) {
  TestUrlParsing("http://test.com/ResourceManagerTest#some/fragment?foo=bar");
}

namespace {

const char kProviderId[] = "provider";

// Content provider that performs simple tests.
class SimpleTestProvider : public TestProvider {
 public:
  enum Mode {
    NOT_HANDLED,
    CONTINUE,
    STOP,
    REMOVE,
    REMOVE_ALL,
    DO_NOTHING,
  };

  SimpleTestProvider(State* state, Mode mode, CefResourceManager* manager)
      : TestProvider(state), mode_(mode), manager_(manager) {}

  SimpleTestProvider(State* state,
                     Mode mode,
                     CefResourceManager* manager,
                     base::OnceClosure do_nothing_callback)
      : TestProvider(state),
        mode_(mode),
        manager_(manager),
        do_nothing_callback_(std::move(do_nothing_callback)) {}

  bool OnRequest(scoped_refptr<CefResourceManager::Request> request) override {
    TestProvider::OnRequest(request);

    if (mode_ == NOT_HANDLED)
      return false;
    else if (mode_ == CONTINUE)
      request->Continue(nullptr);
    else if (mode_ == STOP)
      request->Stop();
    else if (mode_ == REMOVE)
      manager_->RemoveProviders(kProviderId);
    else if (mode_ == REMOVE_ALL)
      manager_->RemoveAllProviders();
    else if (mode_ == DO_NOTHING) {
      EXPECT_FALSE(do_nothing_callback_.is_null());
      std::move(do_nothing_callback_).Run();
    }

    return true;
  }

 private:
  Mode mode_;
  CefResourceManager* manager_;  // Weak reference.
  base::OnceClosure do_nothing_callback_;

  DISALLOW_COPY_AND_ASSIGN(SimpleTestProvider);
};

}  // namespace

// Test with multiple providers that do not handle the request.
TEST(ResourceManagerTest, ProviderNotHandled) {
  const char kUrl[] = "http://test.com/ResourceManagerTest";

  ResourceManagerTestHandler::State state;
  state.urls_.push_back(kUrl);

  TestProvider::State provider_state1;
  TestProvider::State provider_state2;

  ProviderDestructHelper destruct_helper(2);
  provider_state1.destruct_callback_ = destruct_helper.callback();
  provider_state2.destruct_callback_ = destruct_helper.callback();

  state.manager_->AddProvider(
      new SimpleTestProvider(&provider_state1, SimpleTestProvider::NOT_HANDLED,
                             nullptr),
      0, std::string());
  state.manager_->AddProvider(
      new SimpleTestProvider(&provider_state2, SimpleTestProvider::NOT_HANDLED,
                             nullptr),
      0, std::string());

  CefRefPtr<ResourceManagerTestHandler> handler =
      new ResourceManagerTestHandler(&state);
  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);

  state.manager_ = nullptr;

  // Wait for the manager to be deleted.
  destruct_helper.Wait();

  // All providers are called.
  EXPECT_TRUE(provider_state1.got_on_request_);
  EXPECT_FALSE(provider_state1.got_on_request_canceled_);
  EXPECT_TRUE(provider_state1.got_destruct_);

  EXPECT_TRUE(provider_state2.got_on_request_);
  EXPECT_FALSE(provider_state2.got_on_request_canceled_);
  EXPECT_TRUE(provider_state2.got_destruct_);

  EXPECT_EQ(state.messages_.size(), 1U);
  EXPECT_EQ(CreateMessage(kDoneMsg, kNotHandled), state.messages_[0]);
}

// Test with multiple providers that all continue.
TEST(ResourceManagerTest, ProviderContinue) {
  const char kUrl[] = "http://test.com/ResourceManagerTest";

  ResourceManagerTestHandler::State state;
  state.urls_.push_back(kUrl);

  TestProvider::State provider_state1;
  TestProvider::State provider_state2;

  ProviderDestructHelper destruct_helper(2);
  provider_state1.destruct_callback_ = destruct_helper.callback();
  provider_state2.destruct_callback_ = destruct_helper.callback();

  state.manager_->AddProvider(
      new SimpleTestProvider(&provider_state1, SimpleTestProvider::CONTINUE,
                             nullptr),
      0, std::string());
  state.manager_->AddProvider(
      new SimpleTestProvider(&provider_state2, SimpleTestProvider::CONTINUE,
                             nullptr),
      0, std::string());

  CefRefPtr<ResourceManagerTestHandler> handler =
      new ResourceManagerTestHandler(&state);
  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);

  state.manager_ = nullptr;

  // Wait for the manager to be deleted.
  destruct_helper.Wait();

  // All providers are called.
  EXPECT_TRUE(provider_state1.got_on_request_);
  EXPECT_FALSE(provider_state1.got_on_request_canceled_);
  EXPECT_TRUE(provider_state1.got_destruct_);

  EXPECT_TRUE(provider_state2.got_on_request_);
  EXPECT_FALSE(provider_state2.got_on_request_canceled_);
  EXPECT_TRUE(provider_state2.got_destruct_);

  EXPECT_EQ(state.messages_.size(), 1U);
  EXPECT_EQ(CreateMessage(kDoneMsg, kNotHandled), state.messages_[0]);
}

// Test with multiple providers where the first one stops.
TEST(ResourceManagerTest, ProviderStop) {
  const char kUrl[] = "http://test.com/ResourceManagerTest";

  ResourceManagerTestHandler::State state;
  state.urls_.push_back(kUrl);

  TestProvider::State provider_state1;
  TestProvider::State provider_state2;

  ProviderDestructHelper destruct_helper(2);
  provider_state1.destruct_callback_ = destruct_helper.callback();
  provider_state2.destruct_callback_ = destruct_helper.callback();

  state.manager_->AddProvider(
      new SimpleTestProvider(&provider_state1, SimpleTestProvider::STOP,
                             nullptr),
      0, std::string());
  state.manager_->AddProvider(
      new SimpleTestProvider(&provider_state2, SimpleTestProvider::CONTINUE,
                             nullptr),
      0, std::string());

  CefRefPtr<ResourceManagerTestHandler> handler =
      new ResourceManagerTestHandler(&state);
  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);

  state.manager_ = nullptr;

  // Wait for the manager to be deleted.
  destruct_helper.Wait();

  // 1st provider is called.
  EXPECT_TRUE(provider_state1.got_on_request_);
  EXPECT_FALSE(provider_state1.got_on_request_canceled_);
  EXPECT_TRUE(provider_state1.got_destruct_);

  // 2nd provider is not called because the 1st provider stopped.
  EXPECT_FALSE(provider_state2.got_on_request_);
  EXPECT_FALSE(provider_state2.got_on_request_canceled_);
  EXPECT_TRUE(provider_state2.got_destruct_);

  EXPECT_EQ(state.messages_.size(), 1U);
  EXPECT_EQ(CreateMessage(kDoneMsg, kNotHandled), state.messages_[0]);
}

// Test with multiple providers where the first one removes multiple providers
// including itself.
TEST(ResourceManagerTest, ProviderRemove) {
  const char kUrl[] = "http://test.com/ResourceManagerTest";

  ResourceManagerTestHandler::State state;
  state.urls_.push_back(kUrl);

  TestProvider::State provider_state1;
  TestProvider::State provider_state2;
  TestProvider::State provider_state3;

  ProviderDestructHelper destruct_helper(3);
  provider_state1.destruct_callback_ = destruct_helper.callback();
  provider_state2.destruct_callback_ = destruct_helper.callback();
  provider_state3.destruct_callback_ = destruct_helper.callback();

  state.manager_->AddProvider(
      new SimpleTestProvider(&provider_state1, SimpleTestProvider::REMOVE,
                             state.manager_.get()),
      0, kProviderId);
  state.manager_->AddProvider(
      new SimpleTestProvider(&provider_state2, SimpleTestProvider::CONTINUE,
                             nullptr),
      0, kProviderId);
  state.manager_->AddProvider(
      new SimpleTestProvider(&provider_state3, SimpleTestProvider::CONTINUE,
                             nullptr),
      1, std::string());

  CefRefPtr<ResourceManagerTestHandler> handler =
      new ResourceManagerTestHandler(&state);
  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);

  state.manager_ = nullptr;

  // Wait for the manager to be deleted.
  destruct_helper.Wait();

  // 1st provider is called and canceled.
  EXPECT_TRUE(provider_state1.got_on_request_);
  EXPECT_TRUE(provider_state1.got_on_request_canceled_);
  EXPECT_TRUE(provider_state1.got_destruct_);

  // 2nd provider is removed with the 1st provider due to sharing the same
  // identifier.
  EXPECT_FALSE(provider_state2.got_on_request_);
  EXPECT_FALSE(provider_state2.got_on_request_canceled_);
  EXPECT_TRUE(provider_state2.got_destruct_);

  // 3rd provider is called.
  EXPECT_TRUE(provider_state3.got_on_request_);
  EXPECT_FALSE(provider_state3.got_on_request_canceled_);
  EXPECT_TRUE(provider_state3.got_destruct_);

  EXPECT_EQ(state.messages_.size(), 1U);
  EXPECT_EQ(CreateMessage(kDoneMsg, kNotHandled), state.messages_[0]);
}

// Test with multiple providers where the first provider removes all.
TEST(ResourceManagerTest, ProviderRemoveAll) {
  const char kUrl[] = "http://test.com/ResourceManagerTest";

  ResourceManagerTestHandler::State state;
  state.urls_.push_back(kUrl);

  TestProvider::State provider_state1;
  TestProvider::State provider_state2;
  TestProvider::State provider_state3;

  ProviderDestructHelper destruct_helper(3);
  provider_state1.destruct_callback_ = destruct_helper.callback();
  provider_state2.destruct_callback_ = destruct_helper.callback();
  provider_state3.destruct_callback_ = destruct_helper.callback();

  state.manager_->AddProvider(
      new SimpleTestProvider(&provider_state1, SimpleTestProvider::REMOVE_ALL,
                             state.manager_.get()),
      0, std::string());
  state.manager_->AddProvider(
      new SimpleTestProvider(&provider_state2, SimpleTestProvider::CONTINUE,
                             nullptr),
      0, std::string());
  state.manager_->AddProvider(
      new SimpleTestProvider(&provider_state3, SimpleTestProvider::CONTINUE,
                             nullptr),
      1, std::string());

  CefRefPtr<ResourceManagerTestHandler> handler =
      new ResourceManagerTestHandler(&state);
  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);

  state.manager_ = nullptr;

  // Wait for the manager to be deleted.
  destruct_helper.Wait();

  // 1st provider is called and canceled.
  EXPECT_TRUE(provider_state1.got_on_request_);
  EXPECT_TRUE(provider_state1.got_on_request_canceled_);
  EXPECT_TRUE(provider_state1.got_destruct_);

  // 2nd and 3rd providers are not called due to removal.
  EXPECT_FALSE(provider_state2.got_on_request_);
  EXPECT_FALSE(provider_state2.got_on_request_canceled_);
  EXPECT_TRUE(provider_state2.got_destruct_);

  EXPECT_FALSE(provider_state3.got_on_request_);
  EXPECT_FALSE(provider_state3.got_on_request_canceled_);
  EXPECT_TRUE(provider_state3.got_destruct_);

  EXPECT_EQ(state.messages_.size(), 1U);
  EXPECT_EQ(CreateMessage(kDoneMsg, kNotHandled), state.messages_[0]);
}

// Test with multiple providers that do not continue and will be destroyed when
// the manager is destroyed.
TEST(ResourceManagerTest, ProviderDoNothing) {
  const char kUrl[] = "http://test.com/ResourceManagerTest";

  ResourceManagerTestHandler::State state;
  state.urls_.push_back(kUrl);

  TestProvider::State provider_state1;
  TestProvider::State provider_state2;

  ProviderDestructHelper destruct_helper(2);
  provider_state1.destruct_callback_ = destruct_helper.callback();
  provider_state2.destruct_callback_ = destruct_helper.callback();

  CefRefPtr<ResourceManagerTestHandler> handler =
      new ResourceManagerTestHandler(&state);

  // DelayedDestroyTest will be executed from SimpleTestHandler::OnRequest.
  state.manager_->AddProvider(
      new SimpleTestProvider(
          &provider_state1, SimpleTestProvider::DO_NOTHING, nullptr,
          base::BindOnce(&ResourceManagerTestHandler::DelayedDestroyTest,
                         handler)),
      0, std::string());
  state.manager_->AddProvider(
      new SimpleTestProvider(&provider_state2, SimpleTestProvider::DO_NOTHING,
                             nullptr),
      0, std::string());

  handler->ExecuteTest();

  // Destroy the resource manager before the handler so that pending requests
  // are canceled. ResourceManagerTestHandler::GetResourceHandler will be called
  // after the manager is destroyed.
  state.manager_ = nullptr;

  // Wait for the manager to be deleted.
  destruct_helper.Wait();

  ReleaseAndWaitForDestructor(handler);

  // Only the first provider is called. It will be canceled when the manager is
  // destroyed.
  EXPECT_TRUE(provider_state1.got_on_request_);
  EXPECT_TRUE(provider_state1.got_on_request_canceled_);
  EXPECT_TRUE(provider_state1.got_destruct_);

  EXPECT_FALSE(provider_state2.got_on_request_);
  EXPECT_FALSE(provider_state2.got_on_request_canceled_);
  EXPECT_TRUE(provider_state2.got_destruct_);

  // Nothing completed.
  EXPECT_EQ(state.messages_.size(), 0U);
}

// Test AddContentProvider.
TEST(ResourceManagerTest, ContentProvider) {
  const char kUrl1[] = "http://test.com/ResourceManagerTest1";
  const char kUrl2[] = "http://test.com/ResourceManagerTest2";
  const char kUrl3[] = "http://test.com/ResourceManagerTest3";

  const std::string& success1_message = CreateMessage(kDoneMsg, "Success1");
  const std::string& success2_message = CreateMessage(kDoneMsg, "Success2");
  const std::string& not_handled_message = CreateMessage(kDoneMsg, kNotHandled);

  ResourceManagerTestHandler::State state;
  state.urls_.push_back(kUrl1);
  state.urls_.push_back(kUrl2);
  state.urls_.push_back(kUrl3);

  // Only the first 2 URLs will be handled.
  state.manager_->AddContentProvider(kUrl1, CreateContents(success1_message),
                                     "text/html", 0, std::string());
  state.manager_->AddContentProvider(kUrl2, CreateContents(success2_message),
                                     "text/html", 0, std::string());

  CefRefPtr<ResourceManagerTestHandler> handler =
      new ResourceManagerTestHandler(&state);
  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);

  state.manager_ = nullptr;

  // Wait for the manager to be deleted.
  WaitForIOThread();

  // Both providers are called and return the expected results.
  EXPECT_EQ(state.messages_.size(), 3U);
  EXPECT_EQ(success1_message, state.messages_[0]);
  EXPECT_EQ(success2_message, state.messages_[1]);
  EXPECT_EQ(not_handled_message, state.messages_[2]);
}

// Test AddDirectoryProvider.
TEST(ResourceManagerTest, DirectoryProvider) {
  const char kUrlBase[] = "http://test.com/ResourceManager";
  const char kFile1[] = "File1.html";
  const char kFile2[] = "File2.html";
  const char kFile3[] = "File3.html";
  const char kFile4[] = "File4.html";

  const std::string& success1_message = CreateMessage(kDoneMsg, "Success1");
  const std::string& success2_message = CreateMessage(kDoneMsg, "Success2");
  const std::string& success3_message = CreateMessage(kDoneMsg, "Success3");
  const std::string& not_handled_message = CreateMessage(kDoneMsg, kNotHandled);

  ResourceManagerTestHandler::State state;
  state.urls_.push_back(kUrlBase + std::string("/") + kFile1);
  state.urls_.push_back(kUrlBase + std::string("/") + kFile2);
  state.urls_.push_back(kUrlBase + std::string("/sub/") + kFile3);
  state.urls_.push_back(kUrlBase + std::string("/") + kFile4);

  CefScopedTempDir scoped_dir;
  EXPECT_TRUE(scoped_dir.CreateUniqueTempDir());

  // Write the files to disk.
  const std::string& temp_dir = scoped_dir.GetPath();
  WriteFile(client::file_util::JoinPath(temp_dir, kFile1),
            CreateContents(success1_message));
  WriteFile(client::file_util::JoinPath(temp_dir, kFile2),
            CreateContents(success2_message));

  // Also include a subdirectory.
  const std::string& sub_dir = client::file_util::JoinPath(temp_dir, "sub");
  EXPECT_TRUE(CefCreateDirectory(sub_dir));
  WriteFile(client::file_util::JoinPath(sub_dir, kFile3),
            CreateContents(success3_message));

  state.manager_->AddDirectoryProvider(kUrlBase, temp_dir, 0, std::string());

  CefRefPtr<ResourceManagerTestHandler> handler =
      new ResourceManagerTestHandler(&state);
  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);

  state.manager_ = nullptr;

  // Wait for the manager to be deleted.
  WaitForIOThread();

  EXPECT_TRUE(scoped_dir.Delete());

  // The first 3 URLs are handled.
  EXPECT_EQ(state.messages_.size(), 4U);
  EXPECT_EQ(success1_message, state.messages_[0]);
  EXPECT_EQ(success2_message, state.messages_[1]);
  EXPECT_EQ(success3_message, state.messages_[2]);
  EXPECT_EQ(not_handled_message, state.messages_[3]);
}

// Test AddArchiveProvider.
TEST(ResourceManagerTest, ArchiveProvider) {
  const char kUrlBase[] = "http://test.com/ResourceManager";
  const char kFile1[] = "File1.html";
  const char kFile2[] = "File2.html";
  const char kFile3[] = "File3.html";
  const char kFile4[] = "File4.html";

  const std::string& success1_message = CreateMessage(kDoneMsg, "Success1");
  const std::string& success2_message = CreateMessage(kDoneMsg, "Success2");
  const std::string& success3_message = CreateMessage(kDoneMsg, "Success3");
  const std::string& not_handled_message = CreateMessage(kDoneMsg, kNotHandled);

  ResourceManagerTestHandler::State state;
  state.urls_.push_back(kUrlBase + std::string("/") + kFile1);
  state.urls_.push_back(kUrlBase + std::string("/") + kFile2);
  state.urls_.push_back(kUrlBase + std::string("/sub/") + kFile3);
  state.urls_.push_back(kUrlBase + std::string("/") + kFile4);

  // Only the first 2 URLs will be handled.
  CefScopedTempDir scoped_dir;
  EXPECT_TRUE(scoped_dir.CreateUniqueTempDir());

  const std::string& temp_dir = scoped_dir.GetPath();

  // Write the files to disk.
  const std::string& file_dir = client::file_util::JoinPath(temp_dir, "files");
  EXPECT_TRUE(CefCreateDirectory(file_dir));
  WriteFile(client::file_util::JoinPath(file_dir, kFile1),
            CreateContents(success1_message));
  WriteFile(client::file_util::JoinPath(file_dir, kFile2),
            CreateContents(success2_message));

  // Also include a subdirectory.
  const std::string& sub_dir = client::file_util::JoinPath(file_dir, "sub");
  EXPECT_TRUE(CefCreateDirectory(sub_dir));
  WriteFile(client::file_util::JoinPath(sub_dir, kFile3),
            CreateContents(success3_message));

  const std::string& archive_path =
      client::file_util::JoinPath(temp_dir, "archive.zip");

  // Create the archive file.
  EXPECT_TRUE(CefZipDirectory(file_dir, archive_path, false));

  state.manager_->AddArchiveProvider(kUrlBase, archive_path, std::string(), 0,
                                     std::string());

  CefRefPtr<ResourceManagerTestHandler> handler =
      new ResourceManagerTestHandler(&state);
  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);

  state.manager_ = nullptr;

  // Wait for the manager to be deleted.
  WaitForIOThread();

  EXPECT_TRUE(scoped_dir.Delete());

  // The first 3 URLs are handled.
  EXPECT_EQ(state.messages_.size(), 4U);
  EXPECT_EQ(success1_message, state.messages_[0]);
  EXPECT_EQ(success2_message, state.messages_[1]);
  EXPECT_EQ(success3_message, state.messages_[2]);
  EXPECT_EQ(not_handled_message, state.messages_[3]);
}

namespace {

// Content provider that only handles a single request.
class OneShotProvider : public CefResourceManager::Provider {
 public:
  OneShotProvider(const std::string& content,
                  base::OnceClosure destruct_callback)
      : done_(false),
        content_(content),
        destruct_callback_(std::move(destruct_callback)) {
    EXPECT_FALSE(content.empty());
  }

  ~OneShotProvider() {
    CEF_REQUIRE_IO_THREAD();
    std::move(destruct_callback_).Run();
  }

  bool OnRequest(scoped_refptr<CefResourceManager::Request> request) override {
    CEF_REQUIRE_IO_THREAD();

    if (done_) {
      // Provider only handles a single request.
      return false;
    }

    done_ = true;

    CefRefPtr<CefStreamReader> stream = CefStreamReader::CreateForData(
        static_cast<void*>(const_cast<char*>(content_.data())),
        content_.length());

    request->Continue(new CefStreamResourceHandler("text/html", stream));
    return true;
  }

 private:
  bool done_;
  std::string content_;
  base::OnceClosure destruct_callback_;

  DISALLOW_COPY_AND_ASSIGN(OneShotProvider);
};

}  // namespace

// Test that providers are called in the expected order and return expected
// results.
TEST(ResourceManagerTest, ProviderOrder) {
  const char kUrl1[] = "http://test.com/ResourceManagerTest1";
  const char kUrl2[] = "http://test.com/ResourceManagerTest2";
  const char kUrl3[] = "http://test.com/ResourceManagerTest3";
  const char kUrl4[] = "http://test.com/ResourceManagerTest4";
  const char kUrl5[] = "http://test.com/ResourceManagerTest5";

  const std::string& success1_message = CreateMessage(kDoneMsg, "Success1");
  const std::string& success2_message = CreateMessage(kDoneMsg, "Success2");
  const std::string& success3_message = CreateMessage(kDoneMsg, "Success3");
  const std::string& success4_message = CreateMessage(kDoneMsg, "Success4");
  const std::string& not_handled_message = CreateMessage(kDoneMsg, kNotHandled);

  ResourceManagerTestHandler::State state;
  state.urls_.push_back(kUrl1);
  state.urls_.push_back(kUrl2);
  state.urls_.push_back(kUrl3);
  state.urls_.push_back(kUrl4);
  state.urls_.push_back(kUrl5);

  ProviderDestructHelper destruct_helper(4);

  // Resulting order should be sequential; success1 .. success4.
  state.manager_->AddProvider(
      new OneShotProvider(CreateContents(success2_message),
                          destruct_helper.callback()),
      0, std::string());
  state.manager_->AddProvider(
      new OneShotProvider(CreateContents(success1_message),
                          destruct_helper.callback()),
      -100, std::string());
  state.manager_->AddProvider(
      new OneShotProvider(CreateContents(success4_message),
                          destruct_helper.callback()),
      100, std::string());
  state.manager_->AddProvider(
      new OneShotProvider(CreateContents(success3_message),
                          destruct_helper.callback()),
      0, std::string());

  CefRefPtr<ResourceManagerTestHandler> handler =
      new ResourceManagerTestHandler(&state);
  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);

  state.manager_ = nullptr;

  // Wait for the manager to be deleted.
  destruct_helper.Wait();

  EXPECT_EQ(state.messages_.size(), 5U);
  EXPECT_EQ(success1_message, state.messages_[0]);
  EXPECT_EQ(success2_message, state.messages_[1]);
  EXPECT_EQ(success3_message, state.messages_[2]);
  EXPECT_EQ(success4_message, state.messages_[3]);
  EXPECT_EQ(not_handled_message, state.messages_[4]);
}

namespace {

// Content provider that returns the path component as the result.
class EchoProvider : public CefResourceManager::Provider {
 public:
  EchoProvider(const std::string& base_url) : base_url_(base_url) {
    EXPECT_TRUE(!base_url_.empty());
  }

  bool OnRequest(scoped_refptr<CefResourceManager::Request> request) override {
    CEF_REQUIRE_IO_THREAD();

    const std::string& url = request->url();
    if (url.find(base_url_) != 0U) {
      // Not handled by this provider.
      return false;
    }

    const std::string& content = CreateContents(url);

    // Parse the URL to identify the delay.
    int delay = atoi(url.substr(base_url_.size()).data());
    EXPECT_GE(delay, 0);

    CefRefPtr<CefStreamReader> stream = CefStreamReader::CreateForData(
        static_cast<void*>(const_cast<char*>(content.data())),
        content.length());

    CefRefPtr<CefStreamResourceHandler> handler(
        new CefStreamResourceHandler("text/html", stream));
    CefPostDelayedTask(TID_IO,
                       base::BindOnce(&CefResourceManager::Request::Continue,
                                      request, handler),
                       delay);

    return true;
  }

 private:
  std::string base_url_;

  DISALLOW_COPY_AND_ASSIGN(EchoProvider);
};

}  // namespace

// Test that many requests pending at the same time complete in the expected
// order and return correct results.
TEST(ResourceManagerTest, ManyRequests) {
  const char kUrl[] = "http://test.com/ResourceManagerTest";
  const char kBaseUrl[] = "http://test.com/ResourceManagerSubTest/";

  ResourceManagerTestHandler::State state;
  state.urls_.push_back(kUrl);

  state.expected_message_ct_ = 50U;

  // Build a page with lots of iframes.
  std::stringstream ss;
  ss << "<html><body>";
  for (size_t i = 0U; i < state.expected_message_ct_; ++i) {
    ss << "<iframe src=\"" << kBaseUrl << (i * 10) << "\"></iframe>";
  }
  ss << "</body></html>";

  state.manager_->AddContentProvider(kUrl, ss.str(), "text/html", 0,
                                     std::string());
  state.manager_->AddProvider(new EchoProvider(kBaseUrl), 0, std::string());

  CefRefPtr<ResourceManagerTestHandler> handler =
      new ResourceManagerTestHandler(&state);
  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);

  state.manager_ = nullptr;

  // Wait for the manager to be deleted.
  WaitForIOThread();

  EXPECT_EQ(state.messages_.size(), state.expected_message_ct_);

  // Requests should complete in order due to the delay.
  for (size_t i = 0; i < state.messages_.size(); ++i) {
    ss.str("");
    ss << kBaseUrl << (i * 10);
    EXPECT_EQ(ss.str(), state.messages_[i]);
  }
}

namespace {

// Content provider that only handles a single request and then removes itself
// from the manager.
class OneShotRemovalProvider : public TestProvider {
 public:
  OneShotRemovalProvider(State* state,
                         const std::string& content,
                         CefResourceManager* manager,
                         const std::string& identifier,
                         bool remove_before_continue)
      : TestProvider(state),
        content_(content),
        manager_(manager),
        identifier_(identifier),
        remove_before_continue_(remove_before_continue) {
    EXPECT_FALSE(content.empty());
  }

  bool OnRequest(scoped_refptr<CefResourceManager::Request> request) override {
    TestProvider::OnRequest(request);

    CefRefPtr<CefStreamReader> stream = CefStreamReader::CreateForData(
        static_cast<void*>(const_cast<char*>(content_.data())),
        content_.length());

    if (remove_before_continue_) {
      // Removing the provider before continuing should trigger a call to
      // OnRequestCanceled.
      if (identifier_.empty())
        manager_->RemoveAllProviders();
      else
        manager_->RemoveProviders(identifier_);
    }

    request->Continue(new CefStreamResourceHandler("text/html", stream));

    if (!remove_before_continue_) {
      // The request has already completed so OnRequestCanceled is not called.
      if (identifier_.empty())
        manager_->RemoveAllProviders();
      else
        manager_->RemoveProviders(identifier_);
    }

    return true;
  }

 private:
  std::string content_;
  CefResourceManager* manager_;  // Weak reference.
  std::string identifier_;
  bool remove_before_continue_;

  DISALLOW_COPY_AND_ASSIGN(OneShotRemovalProvider);
};

}  // namespace

// Test that removal of the current provider after continue has the expected
// results.
TEST(ResourceManagerTest, RemoveProviderAfterContinue) {
  const char kUrl1[] = "http://test.com/ResourceManagerTest1";
  const char kUrl2[] = "http://test.com/ResourceManagerTest2";
  const char kUrl3[] = "http://test.com/ResourceManagerTest3";
  const char kUrl4[] = "http://test.com/ResourceManagerTest4";

  const std::string& success1_message = CreateMessage(kDoneMsg, "Success1");
  const std::string& success2_message = CreateMessage(kDoneMsg, "Success2");
  const std::string& success3_message = CreateMessage(kDoneMsg, "Success3");
  const std::string& success4_message = CreateMessage(kDoneMsg, "Success4");
  const std::string& not_handled_message = CreateMessage(kDoneMsg, kNotHandled);

  ResourceManagerTestHandler::State state;
  state.urls_.push_back(kUrl1);
  state.urls_.push_back(kUrl2);
  state.urls_.push_back(kUrl3);
  state.urls_.push_back(kUrl4);

  TestProvider::State provider_state1;
  TestProvider::State provider_state2;
  TestProvider::State provider_state3;
  TestProvider::State provider_state4;

  ProviderDestructHelper destruct_helper(4);
  provider_state1.destruct_callback_ = destruct_helper.callback();
  provider_state2.destruct_callback_ = destruct_helper.callback();
  provider_state3.destruct_callback_ = destruct_helper.callback();
  provider_state4.destruct_callback_ = destruct_helper.callback();

  const char kIdentifier1[] = "id1";
  const char kIdentifier2[] = "id2";
  const char kIdentifier4[] = "id4";

  // Resulting order should be sequential; success1 .. success4.
  state.manager_->AddProvider(
      new OneShotRemovalProvider(&provider_state2,
                                 CreateContents(success2_message),
                                 state.manager_.get(), kIdentifier2, false),
      0, kIdentifier2);
  state.manager_->AddProvider(
      new OneShotRemovalProvider(&provider_state1,
                                 CreateContents(success1_message),
                                 state.manager_.get(), kIdentifier1, false),
      -100, kIdentifier1);
  state.manager_->AddProvider(
      new OneShotRemovalProvider(&provider_state4,
                                 CreateContents(success4_message),
                                 state.manager_.get(), kIdentifier4, false),
      100, kIdentifier4);
  // Will be removed at the same time as #2 and therefore never called.
  state.manager_->AddProvider(
      new OneShotRemovalProvider(&provider_state3,
                                 CreateContents(success3_message),
                                 state.manager_.get(), kIdentifier2, false),
      0, kIdentifier2);

  CefRefPtr<ResourceManagerTestHandler> handler =
      new ResourceManagerTestHandler(&state);
  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);

  state.manager_ = nullptr;

  // Wait for the manager to be deleted.
  destruct_helper.Wait();

  // All providers except for 3 (which is removed at the same time as 2 and
  // therefore never executed) should complete.
  EXPECT_EQ(state.messages_.size(), 4U);
  EXPECT_EQ(success1_message, state.messages_[0]);
  EXPECT_EQ(success2_message, state.messages_[1]);
  EXPECT_EQ(success4_message, state.messages_[2]);
  EXPECT_EQ(not_handled_message, state.messages_[3]);

  EXPECT_TRUE(provider_state1.got_on_request_);
  EXPECT_FALSE(provider_state1.got_on_request_canceled_);
  EXPECT_TRUE(provider_state1.got_destruct_);

  EXPECT_TRUE(provider_state2.got_on_request_);
  EXPECT_FALSE(provider_state2.got_on_request_canceled_);
  EXPECT_TRUE(provider_state2.got_destruct_);

  // Removed at the same time as 2 and therefore not executed.
  EXPECT_FALSE(provider_state3.got_on_request_);
  EXPECT_FALSE(provider_state3.got_on_request_canceled_);
  EXPECT_TRUE(provider_state3.got_destruct_);

  EXPECT_TRUE(provider_state4.got_on_request_);
  EXPECT_FALSE(provider_state4.got_on_request_canceled_);
  EXPECT_TRUE(provider_state4.got_destruct_);
}

// Test that removal of the current provider before continue has the expected
// results.
TEST(ResourceManagerTest, RemoveProviderBeforeContinue) {
  const char kUrl1[] = "http://test.com/ResourceManagerTest1";
  const char kUrl2[] = "http://test.com/ResourceManagerTest2";
  const char kUrl3[] = "http://test.com/ResourceManagerTest3";
  const char kUrl4[] = "http://test.com/ResourceManagerTest4";

  const std::string& success1_message = CreateMessage(kDoneMsg, "Success1");
  const std::string& success2_message = CreateMessage(kDoneMsg, "Success2");
  const std::string& success3_message = CreateMessage(kDoneMsg, "Success3");
  const std::string& success4_message = CreateMessage(kDoneMsg, "Success4");
  const std::string& not_handled_message = CreateMessage(kDoneMsg, kNotHandled);

  ResourceManagerTestHandler::State state;
  state.urls_.push_back(kUrl1);
  state.urls_.push_back(kUrl2);
  state.urls_.push_back(kUrl3);
  state.urls_.push_back(kUrl4);

  TestProvider::State provider_state1;
  TestProvider::State provider_state2;
  TestProvider::State provider_state3;
  TestProvider::State provider_state4;

  ProviderDestructHelper destruct_helper(4);
  provider_state1.destruct_callback_ = destruct_helper.callback();
  provider_state2.destruct_callback_ = destruct_helper.callback();
  provider_state3.destruct_callback_ = destruct_helper.callback();
  provider_state4.destruct_callback_ = destruct_helper.callback();

  const char kIdentifier1[] = "id1";
  const char kIdentifier2[] = "id2";
  const char kIdentifier4[] = "id4";

  // Resulting order should be sequential; success1 .. success4.
  state.manager_->AddProvider(
      new OneShotRemovalProvider(&provider_state2,
                                 CreateContents(success2_message),
                                 state.manager_.get(), kIdentifier2, true),
      0, kIdentifier2);
  state.manager_->AddProvider(
      new OneShotRemovalProvider(&provider_state1,
                                 CreateContents(success1_message),
                                 state.manager_.get(), kIdentifier1, true),
      -100, kIdentifier1);
  state.manager_->AddProvider(
      new OneShotRemovalProvider(&provider_state4,
                                 CreateContents(success4_message),
                                 state.manager_.get(), kIdentifier4, true),
      100, kIdentifier4);
  state.manager_->AddProvider(
      new OneShotRemovalProvider(&provider_state3,
                                 CreateContents(success3_message),
                                 state.manager_.get(), kIdentifier2, true),
      0, kIdentifier2);

  CefRefPtr<ResourceManagerTestHandler> handler =
      new ResourceManagerTestHandler(&state);
  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);

  state.manager_ = nullptr;

  // Wait for the manager to be deleted.
  destruct_helper.Wait();

  // No providers should complete.
  EXPECT_EQ(state.messages_.size(), 4U);
  EXPECT_EQ(not_handled_message, state.messages_[0]);
  EXPECT_EQ(not_handled_message, state.messages_[1]);
  EXPECT_EQ(not_handled_message, state.messages_[2]);
  EXPECT_EQ(not_handled_message, state.messages_[3]);

  // All providers except 3 should be called and then canceled.
  EXPECT_TRUE(provider_state1.got_on_request_);
  EXPECT_TRUE(provider_state1.got_on_request_canceled_);
  EXPECT_TRUE(provider_state1.got_destruct_);

  EXPECT_TRUE(provider_state2.got_on_request_);
  EXPECT_TRUE(provider_state2.got_on_request_canceled_);
  EXPECT_TRUE(provider_state2.got_destruct_);

  // Removed at the same time as 2 and therefore not executed.
  EXPECT_FALSE(provider_state3.got_on_request_);
  EXPECT_FALSE(provider_state3.got_on_request_canceled_);
  EXPECT_TRUE(provider_state3.got_destruct_);

  EXPECT_TRUE(provider_state4.got_on_request_);
  EXPECT_TRUE(provider_state4.got_on_request_canceled_);
  EXPECT_TRUE(provider_state4.got_destruct_);
}

// Test that removal of all providers after continue has the expected results.
TEST(ResourceManagerTest, RemoveAllProvidersAfterContinue) {
  const char kUrl1[] = "http://test.com/ResourceManagerTest1";
  const char kUrl2[] = "http://test.com/ResourceManagerTest2";
  const char kUrl3[] = "http://test.com/ResourceManagerTest3";
  const char kUrl4[] = "http://test.com/ResourceManagerTest4";

  const std::string& success1_message = CreateMessage(kDoneMsg, "Success1");
  const std::string& success2_message = CreateMessage(kDoneMsg, "Success2");
  const std::string& success3_message = CreateMessage(kDoneMsg, "Success3");
  const std::string& success4_message = CreateMessage(kDoneMsg, "Success4");
  const std::string& not_handled_message = CreateMessage(kDoneMsg, kNotHandled);

  ResourceManagerTestHandler::State state;
  state.urls_.push_back(kUrl1);
  state.urls_.push_back(kUrl2);
  state.urls_.push_back(kUrl3);
  state.urls_.push_back(kUrl4);

  TestProvider::State provider_state1;
  TestProvider::State provider_state2;
  TestProvider::State provider_state3;
  TestProvider::State provider_state4;

  ProviderDestructHelper destruct_helper(4);
  provider_state1.destruct_callback_ = destruct_helper.callback();
  provider_state2.destruct_callback_ = destruct_helper.callback();
  provider_state3.destruct_callback_ = destruct_helper.callback();
  provider_state4.destruct_callback_ = destruct_helper.callback();

  // Resulting order should be sequential; success1 .. success4.
  state.manager_->AddProvider(
      new OneShotRemovalProvider(&provider_state2,
                                 CreateContents(success2_message),
                                 state.manager_.get(), std::string(), false),
      0, std::string());
  state.manager_->AddProvider(
      new OneShotRemovalProvider(&provider_state1,
                                 CreateContents(success1_message),
                                 state.manager_.get(), std::string(), false),
      -100, std::string());
  state.manager_->AddProvider(
      new OneShotRemovalProvider(&provider_state4,
                                 CreateContents(success4_message),
                                 state.manager_.get(), std::string(), false),
      100, std::string());
  // Will be removed at the same time as #2 and therefore never called.
  state.manager_->AddProvider(
      new OneShotRemovalProvider(&provider_state3,
                                 CreateContents(success3_message),
                                 state.manager_.get(), std::string(), false),
      0, std::string());

  CefRefPtr<ResourceManagerTestHandler> handler =
      new ResourceManagerTestHandler(&state);
  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);

  state.manager_ = nullptr;

  // Wait for the manager to be deleted.
  destruct_helper.Wait();

  // Only the 1st provider should complete
  EXPECT_EQ(state.messages_.size(), 4U);
  EXPECT_EQ(success1_message, state.messages_[0]);
  EXPECT_EQ(not_handled_message, state.messages_[1]);
  EXPECT_EQ(not_handled_message, state.messages_[2]);
  EXPECT_EQ(not_handled_message, state.messages_[3]);

  EXPECT_TRUE(provider_state1.got_on_request_);
  EXPECT_FALSE(provider_state1.got_on_request_canceled_);
  EXPECT_TRUE(provider_state1.got_destruct_);

  EXPECT_FALSE(provider_state2.got_on_request_);
  EXPECT_FALSE(provider_state2.got_on_request_canceled_);
  EXPECT_TRUE(provider_state2.got_destruct_);

  EXPECT_FALSE(provider_state3.got_on_request_);
  EXPECT_FALSE(provider_state3.got_on_request_canceled_);
  EXPECT_TRUE(provider_state3.got_destruct_);

  EXPECT_FALSE(provider_state4.got_on_request_);
  EXPECT_FALSE(provider_state4.got_on_request_canceled_);
  EXPECT_TRUE(provider_state4.got_destruct_);
}

// Test that removal of all providers before continue has the expected results.
TEST(ResourceManagerTest, RemoveAllProvidersBeforeContinue) {
  const char kUrl1[] = "http://test.com/ResourceManagerTest1";
  const char kUrl2[] = "http://test.com/ResourceManagerTest2";
  const char kUrl3[] = "http://test.com/ResourceManagerTest3";
  const char kUrl4[] = "http://test.com/ResourceManagerTest4";

  const std::string& success1_message = CreateMessage(kDoneMsg, "Success1");
  const std::string& success2_message = CreateMessage(kDoneMsg, "Success2");
  const std::string& success3_message = CreateMessage(kDoneMsg, "Success3");
  const std::string& success4_message = CreateMessage(kDoneMsg, "Success4");
  const std::string& not_handled_message = CreateMessage(kDoneMsg, kNotHandled);

  ResourceManagerTestHandler::State state;
  state.urls_.push_back(kUrl1);
  state.urls_.push_back(kUrl2);
  state.urls_.push_back(kUrl3);
  state.urls_.push_back(kUrl4);

  TestProvider::State provider_state1;
  TestProvider::State provider_state2;
  TestProvider::State provider_state3;
  TestProvider::State provider_state4;

  ProviderDestructHelper destruct_helper(4);
  provider_state1.destruct_callback_ = destruct_helper.callback();
  provider_state2.destruct_callback_ = destruct_helper.callback();
  provider_state3.destruct_callback_ = destruct_helper.callback();
  provider_state4.destruct_callback_ = destruct_helper.callback();

  // Resulting order should be sequential; success1 .. success4.
  state.manager_->AddProvider(
      new OneShotRemovalProvider(&provider_state2,
                                 CreateContents(success2_message),
                                 state.manager_.get(), std::string(), true),
      0, std::string());
  state.manager_->AddProvider(
      new OneShotRemovalProvider(&provider_state1,
                                 CreateContents(success1_message),
                                 state.manager_.get(), std::string(), true),
      -100, std::string());
  state.manager_->AddProvider(
      new OneShotRemovalProvider(&provider_state4,
                                 CreateContents(success4_message),
                                 state.manager_.get(), std::string(), true),
      100, std::string());
  // Will be removed at the same time as #2 and therefore never called.
  state.manager_->AddProvider(
      new OneShotRemovalProvider(&provider_state3,
                                 CreateContents(success3_message),
                                 state.manager_.get(), std::string(), true),
      0, std::string());

  CefRefPtr<ResourceManagerTestHandler> handler =
      new ResourceManagerTestHandler(&state);
  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);

  state.manager_ = nullptr;

  // Wait for the manager to be deleted.
  destruct_helper.Wait();

  // No providers should complete.
  EXPECT_EQ(state.messages_.size(), 4U);
  EXPECT_EQ(not_handled_message, state.messages_[0]);
  EXPECT_EQ(not_handled_message, state.messages_[1]);
  EXPECT_EQ(not_handled_message, state.messages_[2]);
  EXPECT_EQ(not_handled_message, state.messages_[3]);

  // 1st provider should also be canceled.
  EXPECT_TRUE(provider_state1.got_on_request_);
  EXPECT_TRUE(provider_state1.got_on_request_canceled_);
  EXPECT_TRUE(provider_state1.got_destruct_);

  EXPECT_FALSE(provider_state2.got_on_request_);
  EXPECT_FALSE(provider_state2.got_on_request_canceled_);
  EXPECT_TRUE(provider_state2.got_destruct_);

  EXPECT_FALSE(provider_state3.got_on_request_);
  EXPECT_FALSE(provider_state3.got_on_request_canceled_);
  EXPECT_TRUE(provider_state3.got_destruct_);

  EXPECT_FALSE(provider_state4.got_on_request_);
  EXPECT_FALSE(provider_state4.got_on_request_canceled_);
  EXPECT_TRUE(provider_state4.got_destruct_);
}

namespace {

// Test the URL filter capability.
class UrlFilterTestProvider : public TestProvider {
 public:
  UrlFilterTestProvider(State* state,
                        const std::string& expected_url,
                        const std::string& expected_url_after_filter)
      : TestProvider(state),
        expected_url_(expected_url),
        expected_url_after_filter_(expected_url_after_filter) {}

  bool OnRequest(scoped_refptr<CefResourceManager::Request> request) override {
    TestProvider::OnRequest(request);

    // Request::url is already filtered.
    EXPECT_EQ(expected_url_, request->url());

    // Explicitly filter the URL again.
    const std::string& filtered_url =
        request->url_filter().Run(request->request()->GetURL());
    EXPECT_EQ(expected_url_after_filter_, filtered_url);

    request->Continue(nullptr);
    return true;
  }

 private:
  std::string expected_url_;
  std::string expected_url_after_filter_;
  DISALLOW_COPY_AND_ASSIGN(UrlFilterTestProvider);
};

std::string TestUrlFilter(const std::string& url) {
  return url + "Rewrite";
}

// Add to the URL but keep the query component.
std::string TestUrlFilterWithQuery(const std::string& url) {
  size_t pos = url.find('?');
  if (pos == std::string::npos)
    return url;
  return url.substr(0, pos) + "Rewrite" + url.substr(pos);
}

// Add to the URL but keep the fragment component.
std::string TestUrlFilterWithFragment(const std::string& url) {
  size_t pos = url.find('#');
  if (pos == std::string::npos)
    return url;
  return url.substr(0, pos) + "Rewrite" + url.substr(pos);
}

}  // namespace

// Test the URL filter capability.
TEST(ResourceManagerTest, UrlFilter) {
  const char kUrl[] = "http://test.com/ResourceManagerTest";
  const char kExpectedUrl[] = "http://test.com/ResourceManagerTestRewrite";

  ResourceManagerTestHandler::State state;
  state.urls_.push_back(kUrl);

  // Set the URL filter.
  state.manager_->SetUrlFilter(base::BindRepeating(TestUrlFilter));

  TestProvider::State provider_state1;
  TestProvider::State provider_state2;

  ProviderDestructHelper destruct_helper(2);
  provider_state1.destruct_callback_ = destruct_helper.callback();
  provider_state2.destruct_callback_ = destruct_helper.callback();

  state.manager_->AddProvider(
      new UrlFilterTestProvider(&provider_state1, kExpectedUrl, kExpectedUrl),
      0, std::string());
  state.manager_->AddProvider(
      new UrlFilterTestProvider(&provider_state2, kExpectedUrl, kExpectedUrl),
      0, std::string());

  CefRefPtr<ResourceManagerTestHandler> handler =
      new ResourceManagerTestHandler(&state);
  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);

  state.manager_ = nullptr;

  // Wait for the manager to be deleted.
  destruct_helper.Wait();

  // All providers are called.
  EXPECT_TRUE(provider_state1.got_on_request_);
  EXPECT_FALSE(provider_state1.got_on_request_canceled_);
  EXPECT_TRUE(provider_state1.got_destruct_);

  EXPECT_TRUE(provider_state2.got_on_request_);
  EXPECT_FALSE(provider_state2.got_on_request_canceled_);
  EXPECT_TRUE(provider_state2.got_destruct_);

  EXPECT_EQ(state.messages_.size(), 1U);
  EXPECT_EQ(CreateMessage(kDoneMsg, kNotHandled), state.messages_[0]);
}

// Test the URL filter capability with a query component.
TEST(ResourceManagerTest, UrlFilterWithQuery) {
  const char kUrl[] = "http://test.com/ResourceManagerTest?foo=bar";
  const char kExpectedUrl[] = "http://test.com/ResourceManagerTestRewrite";
  const char kExpectedUrlAfterFilter[] =
      "http://test.com/ResourceManagerTestRewrite?foo=bar";

  ResourceManagerTestHandler::State state;
  state.urls_.push_back(kUrl);

  // Set the URL filter.
  state.manager_->SetUrlFilter(base::BindRepeating(TestUrlFilterWithQuery));

  TestProvider::State provider_state1;
  TestProvider::State provider_state2;

  ProviderDestructHelper destruct_helper(2);
  provider_state1.destruct_callback_ = destruct_helper.callback();
  provider_state2.destruct_callback_ = destruct_helper.callback();

  state.manager_->AddProvider(
      new UrlFilterTestProvider(&provider_state1, kExpectedUrl,
                                kExpectedUrlAfterFilter),
      0, std::string());
  state.manager_->AddProvider(
      new UrlFilterTestProvider(&provider_state2, kExpectedUrl,
                                kExpectedUrlAfterFilter),
      0, std::string());

  CefRefPtr<ResourceManagerTestHandler> handler =
      new ResourceManagerTestHandler(&state);
  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);

  state.manager_ = nullptr;

  // Wait for the manager to be deleted.
  destruct_helper.Wait();

  // All providers are called.
  EXPECT_TRUE(provider_state1.got_on_request_);
  EXPECT_FALSE(provider_state1.got_on_request_canceled_);
  EXPECT_TRUE(provider_state1.got_destruct_);

  EXPECT_TRUE(provider_state2.got_on_request_);
  EXPECT_FALSE(provider_state2.got_on_request_canceled_);
  EXPECT_TRUE(provider_state2.got_destruct_);

  EXPECT_EQ(state.messages_.size(), 1U);
  EXPECT_EQ(CreateMessage(kDoneMsg, kNotHandled), state.messages_[0]);
}

// Test the URL filter capability with a fragment component.
TEST(ResourceManagerTest, UrlFilterWithFragment) {
  // Fragment components will not be passed with the request.
  const char kUrl[] = "http://test.com/ResourceManagerTest#fragment";
  const char kExpectedUrl[] = "http://test.com/ResourceManagerTestRewrite";
  const char kExpectedUrlAfterFilter[] =
      "http://test.com/ResourceManagerTestRewrite#fragment";

  ResourceManagerTestHandler::State state;
  state.urls_.push_back(kUrl);

  // Set the URL filter.
  state.manager_->SetUrlFilter(base::BindRepeating(TestUrlFilterWithFragment));

  TestProvider::State provider_state1;
  TestProvider::State provider_state2;

  ProviderDestructHelper destruct_helper(2);
  provider_state1.destruct_callback_ = destruct_helper.callback();
  provider_state2.destruct_callback_ = destruct_helper.callback();

  state.manager_->AddProvider(
      new UrlFilterTestProvider(&provider_state1, kExpectedUrl,
                                kExpectedUrlAfterFilter),
      0, std::string());
  state.manager_->AddProvider(
      new UrlFilterTestProvider(&provider_state2, kExpectedUrl,
                                kExpectedUrlAfterFilter),
      0, std::string());

  CefRefPtr<ResourceManagerTestHandler> handler =
      new ResourceManagerTestHandler(&state);
  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);

  state.manager_ = nullptr;

  // Wait for the manager to be deleted.
  destruct_helper.Wait();

  // All providers are called.
  EXPECT_TRUE(provider_state1.got_on_request_);
  EXPECT_FALSE(provider_state1.got_on_request_canceled_);
  EXPECT_TRUE(provider_state1.got_destruct_);

  EXPECT_TRUE(provider_state2.got_on_request_);
  EXPECT_FALSE(provider_state2.got_on_request_canceled_);
  EXPECT_TRUE(provider_state2.got_destruct_);

  EXPECT_EQ(state.messages_.size(), 1U);
  EXPECT_EQ(CreateMessage(kDoneMsg, kNotHandled), state.messages_[0]);
}

namespace {

// Test the mime type resolver capability.
class MimeTypeTestProvider : public TestProvider {
 public:
  MimeTypeTestProvider(State* state, const std::string& expected_mime_type)
      : TestProvider(state), expected_mime_type_(expected_mime_type) {}

  bool OnRequest(scoped_refptr<CefResourceManager::Request> request) override {
    TestProvider::OnRequest(request);

    const std::string& mime_type =
        request->mime_type_resolver().Run(request->url());
    EXPECT_EQ(expected_mime_type_, mime_type);

    request->Continue(nullptr);
    return true;
  }

 private:
  std::string expected_mime_type_;
  DISALLOW_COPY_AND_ASSIGN(MimeTypeTestProvider);
};

const char kExpectedMimeType[] = "foo/bar";

std::string TestMimeTypeResolver(const std::string& url) {
  return kExpectedMimeType;
}

}  // namespace

// Test the mime type resolver capability.
TEST(ResourceManagerTest, MimeTypeResolver) {
  const char kUrl[] = "http://test.com/ResourceManagerTest";

  ResourceManagerTestHandler::State state;
  state.urls_.push_back(kUrl);

  // Set the mime type resolver.
  state.manager_->SetMimeTypeResolver(
      base::BindRepeating(TestMimeTypeResolver));

  TestProvider::State provider_state1;
  TestProvider::State provider_state2;

  ProviderDestructHelper destruct_helper(2);
  provider_state1.destruct_callback_ = destruct_helper.callback();
  provider_state2.destruct_callback_ = destruct_helper.callback();

  state.manager_->AddProvider(
      new MimeTypeTestProvider(&provider_state1, kExpectedMimeType), 0,
      std::string());
  state.manager_->AddProvider(
      new MimeTypeTestProvider(&provider_state2, kExpectedMimeType), 0,
      std::string());

  CefRefPtr<ResourceManagerTestHandler> handler =
      new ResourceManagerTestHandler(&state);
  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);

  state.manager_ = nullptr;

  // Wait for the manager to be deleted.
  destruct_helper.Wait();

  // All providers are called.
  EXPECT_TRUE(provider_state1.got_on_request_);
  EXPECT_FALSE(provider_state1.got_on_request_canceled_);
  EXPECT_TRUE(provider_state1.got_destruct_);

  EXPECT_TRUE(provider_state2.got_on_request_);
  EXPECT_FALSE(provider_state2.got_on_request_canceled_);
  EXPECT_TRUE(provider_state2.got_destruct_);

  EXPECT_EQ(state.messages_.size(), 1U);
  EXPECT_EQ(CreateMessage(kDoneMsg, kNotHandled), state.messages_[0]);
}

namespace {

// Content provider that adds another provider before or after the current
// provider.
class AddingTestProvider : public TestProvider {
 public:
  AddingTestProvider(State* state,
                     State* new_state,
                     CefResourceManager* manager,
                     bool before)
      : TestProvider(state),
        new_state_(new_state),
        manager_(manager),
        before_(before) {}

  bool OnRequest(scoped_refptr<CefResourceManager::Request> request) override {
    TestProvider::OnRequest(request);

    manager_->AddProvider(
        new SimpleTestProvider(new_state_, SimpleTestProvider::CONTINUE,
                               nullptr),
        before_ ? -1 : 1, std::string());

    request->Continue(nullptr);
    return true;
  }

 private:
  State* new_state_;
  CefResourceManager* manager_;  // Weak reference.
  bool before_;

  DISALLOW_COPY_AND_ASSIGN(AddingTestProvider);
};

}  // namespace

// Test adding a new provider after the current provider.
TEST(ResourceManagerTest, AddProviderAfter) {
  const char kUrl[] = "http://test.com/ResourceManagerTest";

  ResourceManagerTestHandler::State state;
  state.urls_.push_back(kUrl);

  TestProvider::State provider_state1;
  TestProvider::State provider_state2;

  ProviderDestructHelper destruct_helper(2);
  provider_state1.destruct_callback_ = destruct_helper.callback();
  provider_state2.destruct_callback_ = destruct_helper.callback();

  state.manager_->AddProvider(
      new AddingTestProvider(&provider_state1, &provider_state2,
                             state.manager_.get(), false),
      0, std::string());

  CefRefPtr<ResourceManagerTestHandler> handler =
      new ResourceManagerTestHandler(&state);
  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);

  state.manager_ = nullptr;

  // Wait for the manager to be deleted.
  destruct_helper.Wait();

  // All providers are called.
  EXPECT_TRUE(provider_state1.got_on_request_);
  EXPECT_FALSE(provider_state1.got_on_request_canceled_);
  EXPECT_TRUE(provider_state1.got_destruct_);

  EXPECT_TRUE(provider_state2.got_on_request_);
  EXPECT_FALSE(provider_state2.got_on_request_canceled_);
  EXPECT_TRUE(provider_state2.got_destruct_);

  EXPECT_EQ(state.messages_.size(), 1U);
  EXPECT_EQ(CreateMessage(kDoneMsg, kNotHandled), state.messages_[0]);
}

// Test adding a new provider before the current provider.
TEST(ResourceManagerTest, AddProviderBefore) {
  const char kUrl[] = "http://test.com/ResourceManagerTest";

  ResourceManagerTestHandler::State state;
  state.urls_.push_back(kUrl);

  TestProvider::State provider_state1;
  TestProvider::State provider_state2;

  ProviderDestructHelper destruct_helper(2);
  provider_state1.destruct_callback_ = destruct_helper.callback();
  provider_state2.destruct_callback_ = destruct_helper.callback();

  state.manager_->AddProvider(
      new AddingTestProvider(&provider_state1, &provider_state2,
                             state.manager_.get(), true),
      0, std::string());

  CefRefPtr<ResourceManagerTestHandler> handler =
      new ResourceManagerTestHandler(&state);
  handler->ExecuteTest();

  ReleaseAndWaitForDestructor(handler);

  state.manager_ = nullptr;

  // Wait for the manager to be deleted.
  destruct_helper.Wait();

  // 1st provider is called.
  EXPECT_TRUE(provider_state1.got_on_request_);
  EXPECT_FALSE(provider_state1.got_on_request_canceled_);
  EXPECT_TRUE(provider_state1.got_destruct_);

  // 2nd provider is not called because it was added before the 1st provider.
  EXPECT_FALSE(provider_state2.got_on_request_);
  EXPECT_FALSE(provider_state2.got_on_request_canceled_);
  EXPECT_TRUE(provider_state2.got_destruct_);

  EXPECT_EQ(state.messages_.size(), 1U);
  EXPECT_EQ(CreateMessage(kDoneMsg, kNotHandled), state.messages_[0]);
}
