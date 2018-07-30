// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/service/context_provider_impl.h"

#include <lib/fdio/util.h>
#include <lib/fidl/cpp/binding.h>
#include <zircon/processargs.h>

#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/fuchsia/file_utils.h"
#include "base/message_loop/message_loop.h"
#include "base/test/multiprocess_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "webrunner/service/common.h"

namespace webrunner {
namespace {

constexpr char kTestDataFileIn[] = "DataFileIn";
constexpr char kTestDataFileOut[] = "DataFileOut";

class TestFrameObserver : public chromium::web::FrameObserver {
 public:
  void OnNavigationStateChanged(
      chromium::web::NavigationStateChangeDetails change,
      OnNavigationStateChangedCallback callback) override {
    last_change_ = change;
    if (change_callback_)
      std::move(change_callback_).Run();
    callback();
  }

  void SetNextChangeCallback(base::OnceClosure callback) {
    change_callback_ = std::move(callback);
  }

  chromium::web::NavigationStateChangeDetails last_change() {
    return last_change_;
  }

 private:
  chromium::web::NavigationStateChangeDetails last_change_ = {};
  base::OnceClosure change_callback_;
};

class FakeContext : public chromium::web::Context {
 public:
  void CreateFrame(
      fidl::InterfaceHandle<chromium::web::FrameObserver> observer,
      fidl::InterfaceRequest<chromium::web::Frame> frame) override {
    chromium::web::NavigationStateChangeDetails details;
    details.url_changed = true;
    details.entry.url = "";
    details.entry.title = "";
    observer.Bind()->OnNavigationStateChanged(details, []() {});
  }
};

MULTIPROCESS_TEST_MAIN(SpawnContextServer) {
  base::MessageLoopForIO message_loop;

  base::FilePath data_dir =  GetWebContextDataDir();
  if (!data_dir.empty()) {
    EXPECT_TRUE(base::PathExists(data_dir.AppendASCII(kTestDataFileIn)));

    auto out_file = data_dir.AppendASCII(kTestDataFileOut);
    EXPECT_EQ(base::WriteFile(out_file, nullptr, 0), 0);
  }

  FakeContext fake_context;
  zx::channel context_handle{zx_take_startup_handle(kContextRequestHandleId)};
  CHECK(context_handle);
  fidl::Binding<chromium::web::Context> binding(&fake_context,
                                                std::move(context_handle));

  base::RunLoop run_loop;

  // Quit the process when the context is destroyed.
  binding.set_error_handler([&run_loop]() { run_loop.Quit(); });

  run_loop.Run();
  return 0;
}

}  // namespace

class ContextProviderImplTest : public base::MultiProcessTest {
 public:
  ContextProviderImplTest() {
    provider_.SetLaunchCallbackForTests(base::BindRepeating(
        &ContextProviderImplTest::LaunchProcess, base::Unretained(this)));
    provider_.Bind(provider_ptr_.NewRequest());
  }

  ~ContextProviderImplTest() override {
    provider_ptr_.Unbind();
    base::RunLoop().RunUntilIdle();
  }

  // Start a new child process whose main function is defined in
  // MULTIPROCESSING_TEST_MAIN(SpawnContextServer).
  base::Process LaunchProcess(base::CommandLine command,
                              const base::LaunchOptions& options) {
    command.AppendSwitchASCII(switches::kTestChildProcess,
                              "SpawnContextServer");
    base::Process context_process = base::LaunchProcess(command, options);
    EXPECT_TRUE(context_process.IsValid());
    return context_process;
  }

  void CheckContext(fidl::InterfacePtr<chromium::web::Context>* context) {
    // Call a Context method and wait for it to invoke an observer call.
    base::RunLoop run_loop;

    context->set_error_handler([&run_loop]() {
      ADD_FAILURE();
      run_loop.Quit();
    });

    TestFrameObserver frame_observer;
    fidl::Binding<chromium::web::FrameObserver> frame_observer_binding(
        &frame_observer);
    frame_observer_binding.set_error_handler([&run_loop]() {
      ADD_FAILURE();
      run_loop.Quit();
    });
    frame_observer.SetNextChangeCallback(run_loop.QuitClosure());

    chromium::web::FramePtr frame_ptr;
    frame_ptr.set_error_handler([&run_loop]() {
      ADD_FAILURE();
      run_loop.Quit();
    });

    (*context)->CreateFrame(frame_observer_binding.NewBinding(),
                            frame_ptr.NewRequest());


    run_loop.Run();

    EXPECT_TRUE(frame_observer.last_change().url_changed ||
                frame_observer.last_change().title_changed);
  }

 protected:
  base::MessageLoopForIO message_loop_;

  ContextProviderImpl provider_;
  chromium::web::ContextProviderPtr provider_ptr_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContextProviderImplTest);
};

TEST_F(ContextProviderImplTest, LaunchContext) {
  // Connect to a new context process.
  fidl::InterfacePtr<chromium::web::Context> context;
  chromium::web::CreateContextParams create_params;
  provider_ptr_->Create(std::move(create_params), context.NewRequest());

  CheckContext(&context);
}

// Verify that there can be more than one connection to the provider.
TEST_F(ContextProviderImplTest, MultipleClients) {
  chromium::web::ContextProviderPtr provider_1_ptr;
  provider_.Bind(provider_1_ptr.NewRequest());

  // Connect a second client.
  chromium::web::ContextProviderPtr provider_2_ptr;
  provider_.Bind(provider_2_ptr.NewRequest());
  fidl::InterfacePtr<chromium::web::Context> context;
  chromium::web::CreateContextParams create_params;
  provider_2_ptr->Create(std::move(create_params), context.NewRequest());

  TestFrameObserver frame_observer;
  fidl::Binding<chromium::web::FrameObserver> frame_observer_binding(
      &frame_observer);
  base::RunLoop run_loop;
  frame_observer.SetNextChangeCallback(run_loop.QuitClosure());
  chromium::web::FramePtr frame_ptr;
  context->CreateFrame(frame_observer_binding.NewBinding(),
                       frame_ptr.NewRequest());
  run_loop.Run();
}

TEST_F(ContextProviderImplTest, WithProfileDir) {
  base::ScopedTempDir profile_temp_dir;

  // Connect to a new context process.
  fidl::InterfacePtr<chromium::web::Context> context;
  chromium::web::CreateContextParams create_params;

  // Setup data dir.
  EXPECT_TRUE(profile_temp_dir.CreateUniqueTempDir());
  ASSERT_EQ(
      base::WriteFile(profile_temp_dir.GetPath().AppendASCII(kTestDataFileIn),
                      nullptr, 0),
      0);

  // Pass a handle data dir to the context.
  create_params.dataDirectory.reset(
      base::fuchsia::GetHandleFromFile(
          base::File(profile_temp_dir.GetPath(),
                     base::File::FLAG_OPEN | base::File::FLAG_READ))
          .release());

  provider_ptr_->Create(std::move(create_params), context.NewRequest());

  CheckContext(&context);

  // Verify that the context process can write data to the out dir.
  EXPECT_TRUE(base::PathExists(
      profile_temp_dir.GetPath().AppendASCII(kTestDataFileOut)));
}

}  // namespace webrunner
