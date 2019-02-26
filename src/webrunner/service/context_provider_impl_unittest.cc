// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/service/context_provider_impl.h"

#include <lib/fdio/util.h>
#include <lib/fidl/cpp/binding.h>
#include <zircon/processargs.h>

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "base/base_paths_fuchsia.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/service_directory.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/test/multiprocess_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "webrunner/fidl/chromium/web/cpp/fidl_test_base.h"
#include "webrunner/service/common.h"
#include "webrunner/test/fake_context.h"

namespace webrunner {
namespace {

constexpr char kTestDataFileIn[] = "DataFileIn";
constexpr char kTestDataFileOut[] = "DataFileOut";
constexpr char kUrl[] = "chrome://:emorhc";
constexpr char kTitle[] = "Palindrome";

MULTIPROCESS_TEST_MAIN(SpawnContextServer) {
  base::MessageLoopForIO message_loop;

  base::FilePath data_dir;
  CHECK(base::PathService::Get(base::DIR_APP_DATA, &data_dir));
  if (!data_dir.empty()) {
    if (base::PathExists(data_dir.AppendASCII(kTestDataFileIn))) {
      auto out_file = data_dir.AppendASCII(kTestDataFileOut);
      EXPECT_EQ(base::WriteFile(out_file, nullptr, 0), 0);
    }
  }

  zx::channel context_handle(zx_take_startup_handle(kContextRequestHandleId));
  CHECK(context_handle);

  FakeContext context;
  fidl::Binding<chromium::web::Context> context_binding(
      &context, fidl::InterfaceRequest<chromium::web::Context>(
                    std::move(context_handle)));

  // When a Frame's NavigationEventObserver is bound, immediately broadcast a
  // navigation event to its listeners.
  context.set_on_create_frame_callback(
      base::BindRepeating([](FakeFrame* frame) {
        frame->set_on_set_observer_callback(base::BindOnce(
            [](FakeFrame* frame) {
              chromium::web::NavigationEvent event;
              event.url = kUrl;
              event.title = kTitle;
              frame->observer()->OnNavigationStateChanged(std::move(event),
                                                          []() {});
            },
            frame));
      }));

  // Quit the process when the context is destroyed.
  base::RunLoop run_loop;
  context_binding.set_error_handler(
      [&run_loop](zx_status_t status) { run_loop.Quit(); });
  run_loop.Run();

  return 0;
}

}  // namespace

class ContextProviderImplTest : public base::MultiProcessTest {
 public:
  ContextProviderImplTest() : provider_(ContextProviderImpl::CreateForTest()) {
    provider_->SetLaunchCallbackForTests(base::BindRepeating(
        &base::SpawnMultiProcessTestChild, "SpawnContextServer"));
    provider_->Bind(provider_ptr_.NewRequest());
  }

  ~ContextProviderImplTest() override {
    provider_ptr_.Unbind();
    base::RunLoop().RunUntilIdle();
  }

  // Check if a Context is responsive by creating a Frame from it and then
  // listening for an event.
  void CheckContextResponsive(
      fidl::InterfacePtr<chromium::web::Context>* context) {
    // Call a Context method and wait for it to invoke an observer call.
    base::RunLoop run_loop;
    context->set_error_handler([&run_loop](zx_status_t status) {
      ADD_FAILURE();
      run_loop.Quit();
    });

    chromium::web::FramePtr frame_ptr;
    frame_ptr.set_error_handler([&run_loop](zx_status_t status) {
      ADD_FAILURE();
      run_loop.Quit();
    });
    (*context)->CreateFrame(frame_ptr.NewRequest());

    // Create a Frame and expect to see a navigation event.
    CapturingNavigationEventObserver change_observer(run_loop.QuitClosure());
    fidl::Binding<chromium::web::NavigationEventObserver>
        change_observer_binding(&change_observer);
    frame_ptr->SetNavigationEventObserver(change_observer_binding.NewBinding());
    run_loop.Run();

    EXPECT_EQ(change_observer.captured_event().url, kUrl);
    EXPECT_EQ(change_observer.captured_event().title, kTitle);
  }

  chromium::web::CreateContextParams BuildCreateContextParams() {
    zx::channel client_channel;
    zx::channel server_channel;
    zx_status_t result =
        zx::channel::create(0, &client_channel, &server_channel);
    ZX_CHECK(result == ZX_OK, result) << "zx_channel_create()";
    result = fdio_service_connect("/svc/.", server_channel.release());
    ZX_CHECK(result == ZX_OK, result) << "Failed to open /svc";

    chromium::web::CreateContextParams output;
    output.service_directory = std::move(client_channel);
    return output;
  }

  // Checks that the Context channel was dropped.
  void CheckContextUnresponsive(
      fidl::InterfacePtr<chromium::web::Context>* context) {
    base::RunLoop run_loop;
    context->set_error_handler(
        [&run_loop](zx_status_t status) { run_loop.Quit(); });

    chromium::web::FramePtr frame;
    (*context)->CreateFrame(frame.NewRequest());

    // The error handler should be called here.
    run_loop.Run();
  }

 protected:
  base::MessageLoopForIO message_loop_;
  std::unique_ptr<ContextProviderImpl> provider_;
  chromium::web::ContextProviderPtr provider_ptr_;

 private:
  struct CapturingNavigationEventObserver
      : public chromium::web::NavigationEventObserver {
   public:
    explicit CapturingNavigationEventObserver(base::OnceClosure on_change_cb)
        : on_change_cb_(std::move(on_change_cb)) {}
    ~CapturingNavigationEventObserver() override = default;

    void OnNavigationStateChanged(
        chromium::web::NavigationEvent change,
        OnNavigationStateChangedCallback callback) override {
      captured_event_ = std::move(change);
      std::move(on_change_cb_).Run();
    }

    chromium::web::NavigationEvent captured_event() { return captured_event_; }

   private:
    base::OnceClosure on_change_cb_;
    chromium::web::NavigationEvent captured_event_;
  };

  DISALLOW_COPY_AND_ASSIGN(ContextProviderImplTest);
};

TEST_F(ContextProviderImplTest, LaunchContext) {
  // Connect to a new context process.
  fidl::InterfacePtr<chromium::web::Context> context;
  chromium::web::CreateContextParams create_params = BuildCreateContextParams();
  provider_ptr_->Create(std::move(create_params), context.NewRequest());
  CheckContextResponsive(&context);
}

TEST_F(ContextProviderImplTest, MultipleConcurrentClients) {
  // Bind a Provider connection, and create a Context from it.
  chromium::web::ContextProviderPtr provider_1_ptr;
  provider_->Bind(provider_1_ptr.NewRequest());
  chromium::web::ContextPtr context_1;
  provider_1_ptr->Create(BuildCreateContextParams(), context_1.NewRequest());

  // Do the same on another Provider connection.
  chromium::web::ContextProviderPtr provider_2_ptr;
  provider_->Bind(provider_2_ptr.NewRequest());
  chromium::web::ContextPtr context_2;
  provider_2_ptr->Create(BuildCreateContextParams(), context_2.NewRequest());

  CheckContextResponsive(&context_1);
  CheckContextResponsive(&context_2);

  // Ensure that the initial ContextProvider connection is still usable, by
  // creating and verifying another Context from it.
  chromium::web::ContextPtr context_3;
  provider_2_ptr->Create(BuildCreateContextParams(), context_3.NewRequest());
  CheckContextResponsive(&context_3);
}

TEST_F(ContextProviderImplTest, WithProfileDir) {
  base::ScopedTempDir profile_temp_dir;

  // Connect to a new context process.
  fidl::InterfacePtr<chromium::web::Context> context;
  chromium::web::CreateContextParams create_params = BuildCreateContextParams();

  // Setup data dir.
  EXPECT_TRUE(profile_temp_dir.CreateUniqueTempDir());
  ASSERT_EQ(
      base::WriteFile(profile_temp_dir.GetPath().AppendASCII(kTestDataFileIn),
                      nullptr, 0),
      0);

  // Pass a handle data dir to the context.
  create_params.data_directory.reset(
      base::fuchsia::GetHandleFromFile(
          base::File(profile_temp_dir.GetPath(),
                     base::File::FLAG_OPEN | base::File::FLAG_READ))
          .release());

  provider_ptr_->Create(std::move(create_params), context.NewRequest());

  CheckContextResponsive(&context);

  // Verify that the context process can write to the data dir.
  EXPECT_TRUE(base::PathExists(
      profile_temp_dir.GetPath().AppendASCII(kTestDataFileOut)));
}

TEST_F(ContextProviderImplTest, FailsDataDirectoryIsFile) {
  base::FilePath temp_file_path;

  // Connect to a new context process.
  fidl::InterfacePtr<chromium::web::Context> context;
  chromium::web::CreateContextParams create_params = BuildCreateContextParams();

  // Pass in a handle to a file instead of a directory.
  CHECK(base::CreateTemporaryFile(&temp_file_path));
  base::File temp_file(temp_file_path,
                       base::File::FLAG_OPEN | base::File::FLAG_READ);
  create_params.data_directory.reset(
      base::fuchsia::GetHandleFromFile(std::move(temp_file)).release());

  provider_ptr_->Create(std::move(create_params), context.NewRequest());

  CheckContextUnresponsive(&context);
}

}  // namespace webrunner
