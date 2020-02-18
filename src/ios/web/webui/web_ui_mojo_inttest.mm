// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ios/web/grit/ios_web_resources.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/web_state.h"
#include "ios/web/public/webui/web_ui_ios_controller.h"
#include "ios/web/public/webui/web_ui_ios_controller_factory.h"
#include "ios/web/public/webui/web_ui_ios_data_source.h"
#include "ios/web/test/grit/test_resources.h"
#include "ios/web/test/mojo_test.mojom.h"
#include "ios/web/test/test_url_constants.h"
#import "ios/web/test/web_int_test.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {

// Hostname for test WebUI page.
const char kTestWebUIURLHost[] = "testwebui";

// Timeout in seconds to wait for a successful message exchange between native
// code and a web page using Mojo.
const NSTimeInterval kMessageTimeout = 5.0;

// UI handler class which communicates with test WebUI page as follows:
// - page sends "syn" message to |TestUIHandler|
// - |TestUIHandler| replies with "ack" message
// - page replies back with "fin"
//
// Once "fin" is received |IsFinReceived()| call will return true, indicating
// that communication was successful. See test WebUI page code here:
// ios/web/test/data/mojo_test.js
class TestUIHandler : public TestUIHandlerMojo {
 public:
  TestUIHandler() {}
  ~TestUIHandler() override {}

  // Returns true if "fin" has been received.
  bool IsFinReceived() { return fin_received_; }

  // TestUIHandlerMojo overrides.
  void SetClientPage(TestPagePtr page) override { page_ = std::move(page); }
  void HandleJsMessage(const std::string& message) override {
    if (message == "syn") {
      // Received "syn" message from WebUI page, send "ack" as reply.
      DCHECK(!syn_received_);
      DCHECK(!fin_received_);
      syn_received_ = true;
      NativeMessageResultMojoPtr result(NativeMessageResultMojo::New());
      result->message = "ack";
      page_->HandleNativeMessage(std::move(result));
    } else if (message == "fin") {
      // Received "fin" from the WebUI page in response to "ack".
      DCHECK(syn_received_);
      DCHECK(!fin_received_);
      fin_received_ = true;
    } else {
      NOTREACHED();
    }
  }

  void BindTestHandler(mojo::PendingReceiver<TestUIHandlerMojo> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

 private:
  mojo::ReceiverSet<TestUIHandlerMojo> receivers_;
  TestPagePtr page_ = nullptr;
  // |true| if "syn" has been received.
  bool syn_received_ = false;
  // |true| if "fin" has been received.
  bool fin_received_ = false;
};

// Controller for test WebUI.
class TestUI : public WebUIIOSController {
 public:
  // Constructs controller from |web_ui| and |ui_handler| which will communicate
  // with test WebUI page.
  TestUI(WebUIIOS* web_ui, TestUIHandler* ui_handler)
      : WebUIIOSController(web_ui) {
    web::WebUIIOSDataSource* source =
        web::WebUIIOSDataSource::Create(kTestWebUIURLHost);

    source->AddResourcePath("mojo_test.js", IDR_MOJO_TEST_JS);
    source->AddResourcePath("mojo_bindings.js", IDR_IOS_MOJO_BINDINGS_JS);
    source->AddResourcePath("mojo_test.mojom.js", IDR_MOJO_TEST_MOJO_JS);
    source->SetDefaultResource(IDR_MOJO_TEST_HTML);

    web::WebState* web_state = web_ui->GetWebState();
    web::WebUIIOSDataSource::Add(web_state->GetBrowserState(), source);

    web_state->GetInterfaceBinderForMainFrame()->AddInterface(
        base::BindRepeating(&TestUIHandler::BindTestHandler,
                            base::Unretained(ui_handler)));
  }

  ~TestUI() override = default;
};

// Factory that creates TestUI controller.
class TestWebUIControllerFactory : public WebUIIOSControllerFactory {
 public:
  // Constructs a controller factory which will eventually create |ui_handler|.
  explicit TestWebUIControllerFactory(TestUIHandler* ui_handler)
      : ui_handler_(ui_handler) {}

  // WebUIIOSControllerFactory overrides.
  std::unique_ptr<WebUIIOSController> CreateWebUIIOSControllerForURL(
      WebUIIOS* web_ui,
      const GURL& url) const override {
    if (!url.SchemeIs(kTestWebUIScheme))
      return nullptr;
    DCHECK_EQ(url.host(), kTestWebUIURLHost);
    return std::make_unique<TestUI>(web_ui, ui_handler_);
  }

  NSInteger GetErrorCodeForWebUIURL(const GURL& url) const override {
    if (url.SchemeIs(kTestWebUIScheme))
      return 0;
    return NSURLErrorUnsupportedURL;
  }

 private:
  // UI handler class which communicates with test WebUI page.
  TestUIHandler* ui_handler_;
};
}  // namespace

// A test fixture for verifying mojo communication for WebUI.
class WebUIMojoTest : public WebIntTest {
 protected:
  void SetUp() override {
    WebIntTest::SetUp();
    @autoreleasepool {
      ui_handler_ = std::make_unique<TestUIHandler>();
      WebState::CreateParams params(GetBrowserState());
      web_state_ = WebState::Create(params);
      factory_ =
          std::make_unique<TestWebUIControllerFactory>(ui_handler_.get());
      WebUIIOSControllerFactory::RegisterFactory(factory_.get());
    }
  }

  void TearDown() override {
    @autoreleasepool {
      // WebState owns CRWWebUIManager. When WebState is destroyed,
      // CRWWebUIManager is autoreleased and will be destroyed upon autorelease
      // pool purge. However in this test, WebTest destructor is called before
      // PlatformTest, thus CRWWebUIManager outlives the WebTaskenvironment.
      // However, CRWWebUIManager owns a URLFetcherImpl, which DCHECKs that its
      // destructor is called on UI web thread. Hence, URLFetcherImpl has to
      // outlive the WebTaskenvironment, since [NSThread mainThread] will not be
      // WebThread::UI once WebTaskenvironment is destroyed.
      web_state_.reset();
      ui_handler_.reset();
      WebUIIOSControllerFactory::DeregisterFactory(factory_.get());
    }

    WebIntTest::TearDown();
  }

  // Returns WebState which loads test WebUI page.
  WebState* web_state() { return web_state_.get(); }
  // Returns UI handler which communicates with WebUI page.
  TestUIHandler* test_ui_handler() { return ui_handler_.get(); }

 private:
  std::unique_ptr<WebState> web_state_;
  std::unique_ptr<TestUIHandler> ui_handler_;
  std::unique_ptr<TestWebUIControllerFactory> factory_;
};

// Tests that JS can send messages to the native code and vice versa.
// TestUIHandler is used for communication and test succeeds only when
// |TestUIHandler| successfully receives "ack" message from WebUI page.
TEST_F(WebUIMojoTest, MessageExchange) {
  @autoreleasepool {
    url::SchemeHostPort tuple(kTestWebUIScheme, kTestWebUIURLHost, 0);
    GURL url(tuple.Serialize());
    test::LoadUrl(web_state(), url);
    // LoadIfNecessary is needed because the view is not created (but needed)
    // when loading the page. TODO(crbug.com/705819): Remove this call.
    web_state()->GetNavigationManager()->LoadIfNecessary();

    // Wait until |TestUIHandler| receives "fin" message from WebUI page.
    bool fin_received =
        base::test::ios::WaitUntilConditionOrTimeout(kMessageTimeout, ^{
          // Flush any pending tasks. Don't RunUntilIdle() because
          // RunUntilIdle() is incompatible with mojo::SimpleWatcher's
          // automatic arming behavior, which Mojo JS still depends upon.
          //
          // TODO(crbug.com/701875): Introduce the full watcher API to JS and
          // get rid of this hack.
          base::RunLoop loop;
          base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                        loop.QuitClosure());
          loop.Run();
          return test_ui_handler()->IsFinReceived();
        });

    ASSERT_TRUE(fin_received);
    EXPECT_FALSE(web_state()->IsLoading());
    EXPECT_EQ(url, web_state()->GetLastCommittedURL());
  }
}

}  // namespace web
