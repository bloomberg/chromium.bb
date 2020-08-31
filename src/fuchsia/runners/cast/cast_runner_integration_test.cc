// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/camera3/cpp/fidl.h>
#include <fuchsia/legacymetrics/cpp/fidl.h>
#include <fuchsia/legacymetrics/cpp/fidl_test_base.h>
#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/modular/cpp/fidl.h>
#include <lib/fdio/directory.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/ui/scenic/cpp/view_token_pair.h>
#include <lib/zx/channel.h>
#include <zircon/processargs.h>

#include "base/base_paths_fuchsia.h"
#include "base/callback_helpers.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/filtered_service_directory.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "fuchsia/base/agent_impl.h"
#include "fuchsia/base/context_provider_test_connector.h"
#include "fuchsia/base/fake_component_context.h"
#include "fuchsia/base/fit_adapter.h"
#include "fuchsia/base/frame_test_util.h"
#include "fuchsia/base/fuchsia_dir_scheme.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/base/release_channel.h"
#include "fuchsia/base/result_receiver.h"
#include "fuchsia/base/string_util.h"
#include "fuchsia/base/test_devtools_list_fetcher.h"
#include "fuchsia/base/test_navigation_listener.h"
#include "fuchsia/base/url_request_rewrite_test_util.h"
#include "fuchsia/runners/cast/cast_runner.h"
#include "fuchsia/runners/cast/fake_application_config_manager.h"
#include "fuchsia/runners/cast/test_api_bindings.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestAppId[] = "00000000";

constexpr char kBlankAppUrl[] = "/defaultresponse";
constexpr char kEchoHeaderPath[] = "/echoheader?Test";

constexpr char kTestServerRoot[] =
    FILE_PATH_LITERAL("fuchsia/runners/cast/testdata");

constexpr char kDummyAgentUrl[] =
    "fuchsia-pkg://fuchsia.com/dummy_agent#meta/dummy_agent.cmx";

// Helper used to ensure that cr_fuchsia::RegisterFuchsiaDirScheme() is called
// once per process to register fuchsia-dir scheme. In cast_runner this function
// is called in main.cc, but that code is not executed in
// cast_runner_integration_tests.
//
// TODO(crbug.com/1062351): Update the tests to start cast_runner component
// instead of creating CastRunner in process. Then remove this function.
void EnsureFuchsiaDirSchemeInitialized() {
  class SchemeInitializer {
   public:
    SchemeInitializer() { cr_fuchsia::RegisterFuchsiaDirScheme(); }
  };
  static SchemeInitializer initializer;
}

class FakeUrlRequestRewriteRulesProvider
    : public chromium::cast::UrlRequestRewriteRulesProvider {
 public:
  FakeUrlRequestRewriteRulesProvider() = default;
  ~FakeUrlRequestRewriteRulesProvider() override = default;
  FakeUrlRequestRewriteRulesProvider(
      const FakeUrlRequestRewriteRulesProvider&) = delete;
  FakeUrlRequestRewriteRulesProvider& operator=(
      const FakeUrlRequestRewriteRulesProvider&) = delete;

 private:
  void GetUrlRequestRewriteRules(
      GetUrlRequestRewriteRulesCallback callback) override {
    // Only send the rules once. They do not expire
    if (rules_sent_)
      return;
    rules_sent_ = true;

    std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
    rewrites.push_back(
        cr_fuchsia::CreateRewriteAddHeaders("Test", "TestHeaderValue"));
    fuchsia::web::UrlRequestRewriteRule rule;
    rule.set_rewrites(std::move(rewrites));
    std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
    rules.push_back(std::move(rule));
    callback(std::move(rules));
  }

  bool rules_sent_ = false;
};

class FakeApplicationContext : public chromium::cast::ApplicationContext {
 public:
  FakeApplicationContext() = default;
  ~FakeApplicationContext() final = default;

  FakeApplicationContext(const FakeApplicationContext&) = delete;
  FakeApplicationContext& operator=(const FakeApplicationContext&) = delete;

  chromium::cast::ApplicationController* controller() {
    if (!controller_)
      return nullptr;

    return controller_.get();
  }

 private:
  // chromium::cast::ApplicationContext implementation.
  void GetMediaSessionId(GetMediaSessionIdCallback callback) final {
    callback(0);
  }
  void SetApplicationController(
      fidl::InterfaceHandle<chromium::cast::ApplicationController> controller)
      final {
    controller_ = controller.Bind();
  }

  chromium::cast::ApplicationControllerPtr controller_;
};

class FakeComponentState : public cr_fuchsia::AgentImpl::ComponentStateBase {
 public:
  FakeComponentState(
      base::StringPiece component_url,
      chromium::cast::ApplicationConfigManager* app_config_manager,
      chromium::cast::ApiBindings* bindings_manager,
      chromium::cast::UrlRequestRewriteRulesProvider*
          url_request_rules_provider)
      : ComponentStateBase(component_url),
        app_config_binding_(outgoing_directory(), app_config_manager),
        bindings_manager_binding_(outgoing_directory(), bindings_manager),
        context_binding_(outgoing_directory(), &application_context_) {
    if (url_request_rules_provider) {
      url_request_rules_provider_binding_.emplace(outgoing_directory(),
                                                  url_request_rules_provider);
    }
  }

  ~FakeComponentState() override {
    if (on_delete_)
      std::move(on_delete_).Run();
  }
  FakeComponentState(const FakeComponentState&) = delete;
  FakeComponentState& operator=(const FakeComponentState&) = delete;

  // Make outgoing_directory() public.
  using ComponentStateBase::outgoing_directory;

  FakeApplicationContext* application_context() {
    return &application_context_;
  }

  void set_on_delete(base::OnceClosure on_delete) {
    on_delete_ = std::move(on_delete);
  }

  void Disconnect() { DisconnectClientsAndTeardown(); }

  bool api_bindings_has_clients() {
    return bindings_manager_binding_.has_clients();
  }

  bool url_request_rules_provider_has_clients() {
    if (url_request_rules_provider_binding_) {
      return url_request_rules_provider_binding_->has_clients();
    }
    return false;
  }

 protected:
  const base::fuchsia::ScopedServiceBinding<
      chromium::cast::ApplicationConfigManager>
      app_config_binding_;
  const base::fuchsia::ScopedServiceBinding<chromium::cast::ApiBindings>
      bindings_manager_binding_;
  base::Optional<base::fuchsia::ScopedServiceBinding<
      chromium::cast::UrlRequestRewriteRulesProvider>>
      url_request_rules_provider_binding_;
  FakeApplicationContext application_context_;
  const base::fuchsia::ScopedServiceBinding<chromium::cast::ApplicationContext>
      context_binding_;
  base::OnceClosure on_delete_;
};
}  // namespace

class CastRunnerIntegrationTest : public testing::Test {
 public:
  CastRunnerIntegrationTest()
      : CastRunnerIntegrationTest(/*enable_headless=*/false,
                                  /*enable_vulkan=*/false) {}
  CastRunnerIntegrationTest(const CastRunnerIntegrationTest&) = delete;
  CastRunnerIntegrationTest& operator=(const CastRunnerIntegrationTest&) =
      delete;

  void TearDown() override {
    if (component_controller_)
      ShutdownComponent();

    // Disconnect the CastRunner & let things tear-down.
    cast_runner_ptr_.Unbind();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  explicit CastRunnerIntegrationTest(bool enable_headless, bool enable_vulkan)
      : app_config_manager_binding_(&component_services_,
                                    &app_config_manager_) {
    EnsureFuchsiaDirSchemeInitialized();

    // Create the CastRunner, published into |outgoing_directory_|.
    cast_runner_ = std::make_unique<CastRunner>(enable_headless);
    if (!enable_vulkan)
      cast_runner_->set_disable_vulkan_for_test();
    cast_runner_binding_.emplace(&outgoing_directory_, cast_runner_.get());

    StartAndPublishWebEngine();

    // Connect to the CastRunner's fuchsia.sys.Runner interface.
    fidl::InterfaceHandle<fuchsia::io::Directory> directory;
    outgoing_directory_.GetOrCreateDirectory("svc")->Serve(
        fuchsia::io::OPEN_RIGHT_READABLE | fuchsia::io::OPEN_RIGHT_WRITABLE,
        directory.NewRequest().TakeChannel());
    sys::ServiceDirectory public_directory_client(std::move(directory));
    cast_runner_ptr_ = public_directory_client.Connect<fuchsia::sys::Runner>();
    cast_runner_ptr_.set_error_handler([](zx_status_t status) {
      ZX_LOG(ERROR, status) << "CastRunner closed channel.";
      ADD_FAILURE();
    });

    test_server_.ServeFilesFromSourceDirectory(kTestServerRoot);
    net::test_server::RegisterDefaultHandlers(&test_server_);
    EXPECT_TRUE(test_server_.Start());

    // Inject ApiBinding that used by ExecuteJavaScript().
    std::vector<chromium::cast::ApiBinding> binding_list;
    chromium::cast::ApiBinding eval_js_binding;
    eval_js_binding.set_before_load_script(cr_fuchsia::MemBufferFromString(
        "window.addEventListener('DOMContentLoaded', (event) => {"
        "  var port = cast.__platform__.PortConnector.bind('testport');"
        "  port.onmessage = (e) => {"
        "    var result = eval(e.data);"
        "    if (typeof(result) == \"undefined\") {"
        "      result = \"undefined\";"
        "    }"
        "    port.postMessage(result);"
        "  };"
        "});",
        "test"));
    binding_list.emplace_back(std::move(eval_js_binding));
    api_bindings_.set_bindings(std::move(binding_list));
  }

  std::unique_ptr<cr_fuchsia::AgentImpl::ComponentStateBase> OnComponentConnect(
      base::StringPiece component_url) {
    auto component_state = std::make_unique<FakeComponentState>(
        component_url, &app_config_manager_, &api_bindings_,
        url_request_rewrite_rules_provider_.get());
    component_state_ = component_state.get();

    if (component_state_created_callback_)
      std::move(component_state_created_callback_).Run();

    return component_state;
  }

  void StartAndPublishWebEngine() {
    fidl::InterfaceHandle<fuchsia::io::Directory> web_engine_outgoing_dir =
        cr_fuchsia::StartWebEngineForTests(web_engine_controller_.NewRequest());
    sys::ServiceDirectory web_engine_outgoing_services(
        std::move(web_engine_outgoing_dir));

    test_component_context_.additional_services()
        ->RemovePublicService<fuchsia::web::ContextProvider>();
    test_component_context_.additional_services()->AddPublicService(
        std::make_unique<vfs::Service>(
            [web_engine_outgoing_services =
                 std::move(web_engine_outgoing_services)](
                zx::channel channel, async_dispatcher_t* dispatcher) {
              web_engine_outgoing_services.Connect(
                  fuchsia::web::ContextProvider::Name_, std::move(channel));
            }),
        fuchsia::web::ContextProvider::Name_);
  }

  void RegisterAppWithTestData(GURL url) {
    fuchsia::web::ContentDirectoryProvider provider;
    provider.set_name("testdata");
    base::FilePath pkg_path;
    CHECK(base::PathService::Get(base::DIR_ASSETS, &pkg_path));
    provider.set_directory(base::fuchsia::OpenDirectory(
        pkg_path.AppendASCII("fuchsia/runners/cast/testdata")));
    std::vector<fuchsia::web::ContentDirectoryProvider> providers;
    providers.emplace_back(std::move(provider));

    auto app_config =
        FakeApplicationConfigManager::CreateConfig(kTestAppId, url);
    app_config.set_content_directories_for_isolated_application(
        std::move(providers));
    app_config_manager_.AddAppConfig(std::move(app_config));
  }

  void CreateComponentContextAndStartComponent() {
    auto component_url = base::StringPrintf("cast:%s", kTestAppId);
    CreateComponentContext(component_url);
    StartCastComponent(component_url);
    WaitComponentState();
    WaitTestPort();
  }

  void CreateComponentContext(const base::StringPiece& component_url) {
    url_request_rewrite_rules_provider_ =
        std::make_unique<FakeUrlRequestRewriteRulesProvider>();
    component_context_ = std::make_unique<cr_fuchsia::FakeComponentContext>(
        &component_services_, component_url);
    component_context_->RegisterCreateComponentStateCallback(
        FakeApplicationConfigManager::kFakeAgentUrl,
        base::BindRepeating(&CastRunnerIntegrationTest::OnComponentConnect,
                            base::Unretained(this)));
  }

  void StartCastComponent(base::StringPiece component_url) {
    // Configure the Runner, including a service directory channel to publish
    // services to.
    fidl::InterfaceHandle<fuchsia::io::Directory> directory;
    component_services_.GetOrCreateDirectory("svc")->Serve(
        fuchsia::io::OPEN_RIGHT_READABLE | fuchsia::io::OPEN_RIGHT_WRITABLE,
        directory.NewRequest().TakeChannel());
    fuchsia::sys::StartupInfo startup_info;
    startup_info.launch_info.url = component_url.as_string();

    fidl::InterfaceHandle<fuchsia::io::Directory> outgoing_directory;
    startup_info.launch_info.directory_request =
        outgoing_directory.NewRequest().TakeChannel();

    fidl::InterfaceHandle<fuchsia::io::Directory> svc_directory;
    CHECK_EQ(fdio_service_connect_at(
                 outgoing_directory.channel().get(), "svc",
                 svc_directory.NewRequest().TakeChannel().release()),
             ZX_OK);

    component_services_client_ =
        std::make_unique<sys::ServiceDirectory>(std::move(svc_directory));

    // Place the ServiceDirectory in the |flat_namespace|.
    startup_info.flat_namespace.paths.emplace_back(
        base::fuchsia::kServiceDirectoryPath);
    startup_info.flat_namespace.directories.emplace_back(
        directory.TakeChannel());

    fuchsia::sys::Package package;
    package.resolved_url = component_url.as_string();

    cast_runner_->StartComponent(std::move(package), std::move(startup_info),
                                 component_controller_.NewRequest());
    component_controller_.set_error_handler([](zx_status_t status) {
      ZX_LOG(ERROR, status) << "Component launch failed";
      ADD_FAILURE();
    });
  }

  // Executes |code| in the context of the test application and the returns
  // serialized as string.
  std::string ExecuteJavaScript(const std::string& code) {
    fuchsia::web::WebMessage message;
    message.set_data(cr_fuchsia::MemBufferFromString(code, "test-msg"));
    test_port_->PostMessage(
        std::move(message),
        [](fuchsia::web::MessagePort_PostMessage_Result result) {
          EXPECT_TRUE(result.is_response());
        });

    base::RunLoop response_loop;
    cr_fuchsia::ResultReceiver<fuchsia::web::WebMessage> response(
        response_loop.QuitClosure());
    test_port_->ReceiveMessage(
        cr_fuchsia::CallbackToFitFunction(response.GetReceiveCallback()));
    response_loop.Run();

    std::string response_string;
    EXPECT_TRUE(
        cr_fuchsia::StringFromMemBuffer(response->data(), &response_string));

    return response_string;
  }

  void WaitComponentState() {
    base::RunLoop run_loop;
    component_state_created_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WaitTestPort() {
    CHECK(!test_port_);
    test_port_ = api_bindings_.RunUntilMessagePortReceived("testport").Bind();
  }

  void CheckAppUrl(const GURL& app_url) {
    EXPECT_EQ(ExecuteJavaScript("window.location.href"), app_url.spec());
  }

  void ShutdownComponent() {
    DCHECK(component_controller_);

    base::RunLoop run_loop;
    component_state_->set_on_delete(run_loop.QuitClosure());
    component_controller_.Unbind();
    run_loop.Run();

    component_controller_ = nullptr;
  }

  void WaitForComponentDestroyed() {
    ASSERT_TRUE(component_state_);
    base::RunLoop state_loop;
    component_state_->set_on_delete(state_loop.QuitClosure());

    base::RunLoop controller_loop;
    if (component_controller_) {
      component_controller_.set_error_handler(
          [&controller_loop](zx_status_t status) {
            EXPECT_EQ(status, ZX_ERR_PEER_CLOSED);
            controller_loop.Quit();
          });
    }

    web_engine_controller_->Kill();

    if (component_controller_) {
      controller_loop.Run();
    }

    state_loop.Run();

    component_context_ = nullptr;
    component_services_client_ = nullptr;
    component_state_ = nullptr;
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  net::EmbeddedTestServer test_server_;

  fuchsia::sys::ComponentControllerPtr web_engine_controller_;

  FakeApplicationConfigManager app_config_manager_;
  TestApiBindings api_bindings_;
  std::unique_ptr<FakeUrlRequestRewriteRulesProvider>
      url_request_rewrite_rules_provider_;

  // Incoming service directory, ComponentContext and per-component state.
  sys::OutgoingDirectory component_services_;
  base::fuchsia::ScopedServiceBinding<chromium::cast::ApplicationConfigManager>
      app_config_manager_binding_;
  std::unique_ptr<cr_fuchsia::FakeComponentContext> component_context_;
  fuchsia::sys::ComponentControllerPtr component_controller_;
  std::unique_ptr<sys::ServiceDirectory> component_services_client_;
  FakeComponentState* component_state_ = nullptr;
  fuchsia::web::MessagePortPtr test_port_;

  base::OnceClosure component_state_created_callback_;

  // ServiceDirectory into which the CastRunner will publish itself.
  sys::OutgoingDirectory outgoing_directory_;

  std::unique_ptr<CastRunner> cast_runner_;
  base::Optional<base::fuchsia::ScopedServiceBinding<fuchsia::sys::Runner>>
      cast_runner_binding_;
  fuchsia::sys::RunnerPtr cast_runner_ptr_;
  base::TestComponentContextForProcess test_component_context_{
      base::TestComponentContextForProcess::InitialState::kCloneAll};
};

// A basic integration test ensuring a basic cast request launches the right
// URL in the Chromium service.
TEST_F(CastRunnerIntegrationTest, BasicRequest) {
  GURL app_url = test_server_.GetURL(kBlankAppUrl);
  app_config_manager_.AddApp(kTestAppId, app_url);

  CreateComponentContextAndStartComponent();

  CheckAppUrl(app_url);
}

// Verify that the Runner can continue to be used even after its Context has
// crashed. Regression test for https://crbug.com/1066826).
// TODO(https://crbug.com/1066833): Make this a WebRunner test.
TEST_F(CastRunnerIntegrationTest, CanRecreateContext) {
  // Execute two iterations of launching the component and verifying that it
  // reaches the expected URL.
  for (int i = 0; i < 2; ++i) {
    SCOPED_TRACE(testing::Message() << "Test iteration " << i);

    const GURL app_url = test_server_.GetURL(kBlankAppUrl);
    app_config_manager_.AddApp(kTestAppId, app_url);

    CreateComponentContextAndStartComponent();

    CheckAppUrl(app_url);

    // Setup a loop to wait for Context destruction after WebEngine is killed
    // below.
    base::RunLoop context_lost_loop;
    cast_runner_->SetOnMainContextLostCallbackForTest(
        context_lost_loop.QuitClosure());

    web_engine_controller_->Kill();

    // Wait for the component and the Context to be torn down.
    WaitForComponentDestroyed();
    context_lost_loop.Run();

    // Start a new WebEngine instance for the next iteration.
    if (i < 1)
      StartAndPublishWebEngine();
  }
}

TEST_F(CastRunnerIntegrationTest, ApiBindings) {
  app_config_manager_.AddApp(kTestAppId, test_server_.GetURL(kBlankAppUrl));

  CreateComponentContextAndStartComponent();

  // Verify that we can communicate with the binding added in
  // CastRunnerIntegrationTest().
  EXPECT_EQ(ExecuteJavaScript("1+2+\"\""), "3");
}

TEST_F(CastRunnerIntegrationTest, IncorrectCastAppId) {
  const char kIncorrectComponentUrl[] = "cast:99999999";

  CreateComponentContext(kIncorrectComponentUrl);
  StartCastComponent(kIncorrectComponentUrl);

  // Run the loop until the ComponentController is dropped, or a WebComponent is
  // created.
  base::RunLoop run_loop;
  component_controller_.set_error_handler([&run_loop](zx_status_t status) {
    EXPECT_EQ(status, ZX_ERR_PEER_CLOSED);
    run_loop.Quit();
  });
  run_loop.Run();

  EXPECT_TRUE(!component_state_);
}

TEST_F(CastRunnerIntegrationTest, UrlRequestRewriteRulesProvider) {
  GURL echo_app_url = test_server_.GetURL(kEchoHeaderPath);
  app_config_manager_.AddApp(kTestAppId, echo_app_url);

  CreateComponentContextAndStartComponent();

  CheckAppUrl(echo_app_url);

  EXPECT_EQ(ExecuteJavaScript("document.body.innerText"), "TestHeaderValue");
}

TEST_F(CastRunnerIntegrationTest, ApplicationControllerBound) {
  app_config_manager_.AddApp(kTestAppId, test_server_.GetURL(kBlankAppUrl));

  CreateComponentContextAndStartComponent();

  // Spin the message loop to handle creation of the component state.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(component_state_);
  EXPECT_TRUE(component_state_->application_context()->controller());
}

// Verify an App launched with remote debugging enabled is properly reachable.
TEST_F(CastRunnerIntegrationTest, RemoteDebugging) {
  GURL app_url = test_server_.GetURL(kBlankAppUrl);
  auto app_config =
      FakeApplicationConfigManager::CreateConfig(kTestAppId, app_url);
  app_config.set_enable_remote_debugging(true);
  app_config_manager_.AddAppConfig(std::move(app_config));

  CreateComponentContextAndStartComponent();

  // Connect to the debug service and ensure we get the proper response.
  base::Value devtools_list =
      cr_fuchsia::GetDevToolsListFromPort(CastRunner::kRemoteDebuggingPort);
  ASSERT_TRUE(devtools_list.is_list());
  EXPECT_EQ(devtools_list.GetList().size(), 1u);

  base::Value* devtools_url = devtools_list.GetList()[0].FindPath("url");
  ASSERT_TRUE(devtools_url->is_string());
  EXPECT_EQ(devtools_url->GetString(), app_url.spec());
}

TEST_F(CastRunnerIntegrationTest, IsolatedContext) {
  const GURL kContentDirectoryUrl("fuchsia-dir://testdata/empty.html");

  RegisterAppWithTestData(kContentDirectoryUrl);
  CreateComponentContextAndStartComponent();
  CheckAppUrl(kContentDirectoryUrl);
}

// Test the lack of CastAgent service does not cause a CastRunner crash.
TEST_F(CastRunnerIntegrationTest, NoCastAgent) {
  app_config_manager_.AddApp(kTestAppId, test_server_.GetURL(kEchoHeaderPath));

  StartCastComponent(base::StringPrintf("cast:%s", kTestAppId));

  base::RunLoop run_loop;
  component_controller_.set_error_handler([&run_loop](zx_status_t error) {
    EXPECT_EQ(error, ZX_ERR_PEER_CLOSED);
    run_loop.Quit();
  });
  run_loop.Run();
}

// Test the CastAgent disconnecting does not cause a CastRunner crash.
TEST_F(CastRunnerIntegrationTest, DisconnectedCastAgent) {
  app_config_manager_.AddApp(kTestAppId, test_server_.GetURL(kEchoHeaderPath));

  CreateComponentContextAndStartComponent();

  base::RunLoop run_loop;
  component_controller_.set_error_handler([&run_loop](zx_status_t error) {
    EXPECT_EQ(error, ZX_ERR_PEER_CLOSED);
    run_loop.Quit();
  });

  // Tear down the ComponentState, this should close the Agent connection and
  // shut down the CastComponent.
  component_state_->Disconnect();

  run_loop.Run();
}

// Test that the ApiBindings and RewriteRules are received from the secondary
// DummyAgent. This validates that the |agent_url| retrieved from
// AppConfigManager is the one used to retrieve the bindings and the rewrite
// rules.
TEST_F(CastRunnerIntegrationTest, ApplicationConfigAgentUrl) {
  // These are part of the secondary agent, and CastRunner will contact
  // the secondary agent for both of them.
  FakeUrlRequestRewriteRulesProvider dummy_url_request_rewrite_rules_provider;
  TestApiBindings dummy_agent_api_bindings;

  // Indicate that this app is to get bindings from a secondary agent.
  auto app_config = FakeApplicationConfigManager::CreateConfig(
      kTestAppId, test_server_.GetURL(kBlankAppUrl));
  app_config.set_agent_url(kDummyAgentUrl);
  app_config_manager_.AddAppConfig(std::move(app_config));

  // Instantiate the bindings that are returned in the multi-agent scenario. The
  // bindings returned for the single-agent scenario are not initialized.
  std::vector<chromium::cast::ApiBinding> binding_list;
  chromium::cast::ApiBinding echo_binding;
  echo_binding.set_before_load_script(cr_fuchsia::MemBufferFromString(
      "window.echo = cast.__platform__.PortConnector.bind('dummyService');",
      "test"));
  binding_list.emplace_back(std::move(echo_binding));
  // Assign the bindings to the multi-agent binding.
  dummy_agent_api_bindings.set_bindings(std::move(binding_list));

  auto component_url = base::StringPrintf("cast:%s", kTestAppId);
  CreateComponentContext(component_url);
  EXPECT_NE(component_context_, nullptr);

  base::RunLoop run_loop;
  FakeComponentState* dummy_component_state = nullptr;
  component_context_->RegisterCreateComponentStateCallback(
      kDummyAgentUrl,
      base::BindLambdaForTesting(
          [&](base::StringPiece component_url)
              -> std::unique_ptr<cr_fuchsia::AgentImpl::ComponentStateBase> {
            run_loop.Quit();
            auto result = std::make_unique<FakeComponentState>(
                component_url, &app_config_manager_, &dummy_agent_api_bindings,
                &dummy_url_request_rewrite_rules_provider);
            dummy_component_state = result.get();
            return result;
          }));

  StartCastComponent(component_url);

  // Wait for the component state to be created.
  run_loop.Run();

  // Validate that the component state in the default agent wasn't crated.
  EXPECT_FALSE(component_state_);

  // Shutdown component before destroying dummy_agent_api_bindings.
  base::RunLoop shutdown_run_loop;
  dummy_component_state->set_on_delete(shutdown_run_loop.QuitClosure());
  component_controller_.Unbind();
  shutdown_run_loop.Run();
}

// Test that when RewriteRules are not provided, a WebComponent is still
// created. Further validate that the primary agent does not provide ApiBindings
// or RewriteRules.
TEST_F(CastRunnerIntegrationTest, ApplicationConfigAgentUrlRewriteOptional) {
  TestApiBindings dummy_agent_api_bindings;
  // Indicate that this app is to get bindings from a secondary agent.
  auto app_config = FakeApplicationConfigManager::CreateConfig(
      kTestAppId, test_server_.GetURL(kBlankAppUrl));
  app_config.set_agent_url(kDummyAgentUrl);
  app_config_manager_.AddAppConfig(std::move(app_config));

  // Instantiate the bindings that are returned in the multi-agent scenario. The
  // bindings returned for the single-agent scenario are not initialized.
  std::vector<chromium::cast::ApiBinding> binding_list;
  chromium::cast::ApiBinding echo_binding;
  echo_binding.set_before_load_script(cr_fuchsia::MemBufferFromString(
      "window.echo = cast.__platform__.PortConnector.bind('dummyService');",
      "test"));
  binding_list.emplace_back(std::move(echo_binding));
  // Assign the bindings to the multi-agent binding.
  dummy_agent_api_bindings.set_bindings(std::move(binding_list));

  auto component_url = base::StringPrintf("cast:%s", kTestAppId);
  CreateComponentContext(component_url);
  base::RunLoop run_loop;
  FakeComponentState* dummy_component_state = nullptr;
  component_context_->RegisterCreateComponentStateCallback(
      kDummyAgentUrl,
      base::BindLambdaForTesting(
          [&](base::StringPiece component_url)
              -> std::unique_ptr<cr_fuchsia::AgentImpl::ComponentStateBase> {
            run_loop.Quit();
            auto result = std::make_unique<FakeComponentState>(
                component_url, &app_config_manager_, &dummy_agent_api_bindings,
                nullptr);
            dummy_component_state = result.get();
            return result;
          }));

  StartCastComponent(component_url);

  // Wait for the component state to be created.
  run_loop.Run();

  // Validate that the component state in the default agent wasn't crated.
  EXPECT_FALSE(component_state_);

  // Shutdown component before destroying dummy_agent_api_bindings.
  base::RunLoop shutdown_run_loop;
  dummy_component_state->set_on_delete(shutdown_run_loop.QuitClosure());
  component_controller_.Unbind();
  shutdown_run_loop.Run();
}

TEST_F(CastRunnerIntegrationTest, MicrophoneRedirect) {
  GURL app_url = test_server_.GetURL("/microphone.html");
  auto app_config =
      FakeApplicationConfigManager::CreateConfig(kTestAppId, app_url);

  fuchsia::web::PermissionDescriptor mic_permission;
  mic_permission.set_type(fuchsia::web::PermissionType::MICROPHONE);
  app_config.mutable_permissions()->push_back(std::move(mic_permission));
  app_config_manager_.AddAppConfig(std::move(app_config));

  CreateComponentContextAndStartComponent();

  // Expect fuchsia.media.Audio connection to be redirected to the agent.
  base::RunLoop run_loop;
  component_state_->outgoing_directory()->AddPublicService(
      std::make_unique<vfs::Service>(
          [quit_closure = run_loop.QuitClosure()](
              zx::channel channel, async_dispatcher_t* dispatcher) mutable {
            std::move(quit_closure).Run();
          }),
      fuchsia::media::Audio::Name_);

  ExecuteJavaScript("connectMicrophone();");

  // Will quit once AudioCapturer is connected.
  run_loop.Run();
}

TEST_F(CastRunnerIntegrationTest, CameraRedirect) {
  GURL app_url = test_server_.GetURL("/camera.html");
  auto app_config =
      FakeApplicationConfigManager::CreateConfig(kTestAppId, app_url);

  fuchsia::web::PermissionDescriptor canera_permission;
  canera_permission.set_type(fuchsia::web::PermissionType::CAMERA);
  app_config.mutable_permissions()->push_back(std::move(canera_permission));
  app_config_manager_.AddAppConfig(std::move(app_config));

  CreateComponentContextAndStartComponent();

  // Expect fuchsia.camera3.DeviceWatcher connection to be redirected to the
  // agent.
  base::RunLoop run_loop;
  component_state_->outgoing_directory()->AddPublicService(
      std::make_unique<vfs::Service>(
          [quit_closure = run_loop.QuitClosure()](
              zx::channel channel, async_dispatcher_t* dispatcher) mutable {
            std::move(quit_closure).Run();
          }),
      fuchsia::camera3::DeviceWatcher::Name_);

  ExecuteJavaScript("connectCamera();");

  // Will quit once camara3::DeviceWatcher is connected.
  run_loop.Run();
}

class HeadlessCastRunnerIntegrationTest : public CastRunnerIntegrationTest {
 public:
  HeadlessCastRunnerIntegrationTest()
      : CastRunnerIntegrationTest(/*enable_headless=*/true,
                                  /*enable_vulkan=*/false) {}
};

// A basic integration test ensuring a basic cast request launches the right
// URL in the Chromium service.
TEST_F(HeadlessCastRunnerIntegrationTest, Headless) {
  const char kAnimationPath[] = "/css_animation.html";
  const GURL animation_url = test_server_.GetURL(kAnimationPath);
  app_config_manager_.AddApp(kTestAppId, animation_url);

  CreateComponentContextAndStartComponent();
  auto tokens = scenic::ViewTokenPair::New();

  // Create a view.
  auto view_provider =
      component_services_client_->Connect<fuchsia::ui::app::ViewProvider>();
  view_provider->CreateView(std::move(tokens.view_holder_token.value), {}, {});

  api_bindings_.RunUntilMessagePortReceived("animation_finished");

  // Verify that dropped "view" EventPair is handled properly.
  tokens.view_token.value.reset();
  api_bindings_.RunUntilMessagePortReceived("view_hidden");
}

// Isolated *and* headless? Doesn't sound like much fun!
TEST_F(HeadlessCastRunnerIntegrationTest, IsolatedAndHeadless) {
  const GURL kContentDirectoryUrl("fuchsia-dir://testdata/empty.html");

  RegisterAppWithTestData(kContentDirectoryUrl);
  CreateComponentContextAndStartComponent();
  CheckAppUrl(kContentDirectoryUrl);
}

// Verifies that the Context can establish a connection to the Agent's
// MetricsRecorder service.
TEST_F(CastRunnerIntegrationTest, LegacyMetricsRedirect) {
  GURL app_url = test_server_.GetURL(kBlankAppUrl);
  app_config_manager_.AddApp(kTestAppId, app_url);

  auto component_url = base::StringPrintf("cast:%s", kTestAppId);
  CreateComponentContext(component_url);
  EXPECT_NE(component_context_, nullptr);

  StartCastComponent(component_url);

  // Wait until we see the CastRunner connect to the LegacyMetrics service.
  base::RunLoop run_loop;
  component_state_created_callback_ = base::BindOnce(
      [](FakeComponentState** component_state,
         base::RepeatingClosure quit_closure) {
        (*component_state)
            ->outgoing_directory()
            ->AddPublicService(
                std::make_unique<vfs::Service>(
                    [quit_closure](zx::channel, async_dispatcher_t*) {
                      quit_closure.Run();
                    }),
                fuchsia::legacymetrics::MetricsRecorder::Name_);
      },
      base::Unretained(&component_state_), run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(CastRunnerIntegrationTest, WebGLContextAbsentWithoutVulkanFeature) {
  const char kTestPath[] = "/webgl_presence.html";
  const GURL test_url = test_server_.GetURL(kTestPath);
  app_config_manager_.AddApp(kTestAppId, test_url);

  CreateComponentContextAndStartComponent();

  EXPECT_EQ(ExecuteJavaScript("document.title"), "absent");
}

TEST_F(CastRunnerIntegrationTest,
       WebGLContextAbsentWithoutVulkanFeature_IsolatedRunner) {
  const GURL kContentDirectoryUrl("fuchsia-dir://testdata/webgl_presence.html");

  RegisterAppWithTestData(kContentDirectoryUrl);
  CreateComponentContextAndStartComponent();
  CheckAppUrl(kContentDirectoryUrl);

  EXPECT_EQ(ExecuteJavaScript("document.title"), "absent");
}

#if defined(ARCH_CPU_ARM_FAMILY)
// TODO(crbug.com/1058247): Support Vulkan in tests on ARM64.
#define MAYBE_VulkanCastRunnerIntegrationTest \
  DISABLED_VulkanCastRunnerIntegrationTest
#else
#define MAYBE_VulkanCastRunnerIntegrationTest VulkanCastRunnerIntegrationTest
#endif

class MAYBE_VulkanCastRunnerIntegrationTest : public CastRunnerIntegrationTest {
 public:
  MAYBE_VulkanCastRunnerIntegrationTest()
      : CastRunnerIntegrationTest(/*enable_headless=*/false,
                                  /*enable_vulkan=*/true) {}
};

TEST_F(MAYBE_VulkanCastRunnerIntegrationTest,
       WebGLContextPresentWithVulkanFeature) {
  const char kTestPath[] = "/webgl_presence.html";
  const GURL test_url = test_server_.GetURL(kTestPath);
  app_config_manager_.AddApp(kTestAppId, test_url);

  CreateComponentContextAndStartComponent();

  EXPECT_EQ(ExecuteJavaScript("document.title"), "present");
}

TEST_F(MAYBE_VulkanCastRunnerIntegrationTest,
       WebGLContextPresentWithVulkanFeature_IsolatedRunner) {
  const GURL kContentDirectoryUrl("fuchsia-dir://testdata/webgl_presence.html");

  RegisterAppWithTestData(kContentDirectoryUrl);
  CreateComponentContextAndStartComponent();
  CheckAppUrl(kContentDirectoryUrl);

  EXPECT_EQ(ExecuteJavaScript("document.title"), "present");
}
