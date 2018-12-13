// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/app/cast/cast_runner.h"

#include <lib/fidl/cpp/binding.h>
#include <lib/zx/channel.h>
#include <zircon/status.h>

#include "base/fuchsia/component_context.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/service_directory.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrunner/app/cast/application_config_manager/test/fake_application_config_manager.h"
#include "webrunner/fidl/chromium/web/cpp/fidl.h"
#include "webrunner/test/fake_context.h"

namespace castrunner {

class CastRunnerTest : public testing::Test {
 public:
  CastRunnerTest()
      : fake_context_binding_(&fake_context_, fake_context_ptr_.NewRequest()) {
    // Create a new ServiceDirectory, and a scoped default ComponentContext
    // connected to it, for the test to use to drive the CastRunner.
    zx::channel service_directory_request, service_directory_client;
    zx_status_t status = zx::channel::create(0, &service_directory_client,
                                             &service_directory_request);
    ZX_CHECK(status == ZX_OK, status) << "zx_channel_create";

    service_directory_ = std::make_unique<base::fuchsia::ServiceDirectory>(
        std::move(service_directory_request));
    scoped_default_component_context_ =
        std::make_unique<base::fuchsia::ScopedDefaultComponentContext>(
            std::move(service_directory_client));

    // Create the AppConfigManager.
    app_config_manager_ =
        std::make_unique<castrunner::test::FakeApplicationConfigManager>(
            &test_server_);
    app_config_binding_ = std::make_unique<
        fidl::Binding<chromium::cast::ApplicationConfigManager>>(
        app_config_manager_.get());
    chromium::cast::ApplicationConfigManagerPtr app_config_manager_interface;
    app_config_binding_->Bind(app_config_manager_interface.NewRequest());

    // Create the CastRunner, published into |service_directory_|.
    cast_runner_ = std::make_unique<CastRunner>(
        service_directory_.get(), std::move(fake_context_ptr_),
        std::move(app_config_manager_interface),
        until_runner_idle_loop_.QuitClosure());

    // Connect to the CastRunner's fuchsia.sys.Runner interface.
    cast_runner_ptr_ = base::fuchsia::ComponentContext::GetDefault()
                           ->ConnectToService<fuchsia::sys::Runner>();
    cast_runner_ptr_.set_error_handler([this](zx_status_t status) {
      ADD_FAILURE() << "CastRunner closed channel.";
      until_runner_idle_loop_.Quit();
    });
  }

  void SetUp() override { ASSERT_TRUE(test_server_.Start()); }

  zx::channel StartCastComponent(
      const base::StringPiece& cast_url,
      fidl::InterfaceRequest<fuchsia::sys::ComponentController>
          component_controller_request) {
    fuchsia::sys::LaunchInfo launch_info;
    launch_info.url.reset(cast_url.as_string());

    // Create a channel to pass to the Runner, through which to expose the new
    // component's ServiceDirectory.
    zx::channel service_directory_client;
    zx_status_t status = zx::channel::create(0, &service_directory_client,
                                             &launch_info.directory_request);
    ZX_CHECK(status == ZX_OK, status) << "zx_channel_create";

    fuchsia::sys::StartupInfo startup_info;
    startup_info.launch_info = std::move(launch_info);

    // The FlatNamespace vectors must be non-null, but may be empty.
    startup_info.flat_namespace.paths.resize(0);
    startup_info.flat_namespace.directories.resize(0);

    fuchsia::sys::Package package;
    package.resolved_url.reset(cast_url.as_string());

    cast_runner_ptr_->StartComponent(std::move(package),
                                     std::move(startup_info),
                                     std::move(component_controller_request));
    return service_directory_client;
  }

  void RunUntilCastRunnerIsIdle() { until_runner_idle_loop_.Run(); }

 protected:
  base::MessageLoopForIO message_loop_;
  base::RunLoop until_runner_idle_loop_;

  // Test server.
  net::EmbeddedTestServer test_server_;

  // Test AppConfigManager and its binding.
  std::unique_ptr<castrunner::test::FakeApplicationConfigManager>
      app_config_manager_;
  std::unique_ptr<fidl::Binding<chromium::cast::ApplicationConfigManager>>
      app_config_binding_;

  // Temporarily holds the InterfacePtr to the FakeContext, until it is passed
  // to the CastRunner.
  chromium::web::ContextPtr fake_context_ptr_;

  // Fake web::Context, and binding to the client CastRunner.
  webrunner::FakeContext fake_context_;
  fidl::Binding<chromium::web::Context> fake_context_binding_;

  // ServiceDirectory into which the CastRunner will publish itself.
  std::unique_ptr<base::fuchsia::ServiceDirectory> service_directory_;
  std::unique_ptr<base::fuchsia::ScopedDefaultComponentContext>
      scoped_default_component_context_;

  std::unique_ptr<CastRunner> cast_runner_;
  fuchsia::sys::RunnerPtr cast_runner_ptr_;

  DISALLOW_COPY_AND_ASSIGN(CastRunnerTest);
};

TEST_F(CastRunnerTest, TeardownOnClientUnbind) {
  // Disconnect from the CastRunner and wait for it to terminate.
  cast_runner_ptr_.Unbind();
  RunUntilCastRunnerIsIdle();
}

TEST_F(CastRunnerTest, TeardownOnComponentControllerUnbind) {
  // Create a ComponentController pointer, to manage the component lifetime.
  fuchsia::sys::ComponentControllerPtr component_controller_ptr;

  // Launch the test-app component, passing a ComponentController request.
  std::string url("cast:");
  url += castrunner::test::FakeApplicationConfigManager::kTestCastAppId;
  base::fuchsia::ComponentContext component_services(
      StartCastComponent(url, component_controller_ptr.NewRequest()));

  // Pump the message-loop to process StartComponent(). If the call is rejected
  // then the ComponentControllerPtr's error-handler will be invoked at this
  // point.
  component_controller_ptr.set_error_handler([](zx_status_t status) {
    ZX_LOG(FATAL, status) << "Component launch failed";
  });
  base::RunLoop().RunUntilIdle();

  // Disconnect ComponentController and expect the CastRunner will terminate.
  component_controller_ptr.Unbind();
  RunUntilCastRunnerIsIdle();
}

}  // namespace castrunner
