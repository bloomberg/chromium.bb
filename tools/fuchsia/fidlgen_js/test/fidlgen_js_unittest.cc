// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/fidl/cpp/binding.h>
#include <lib/fidl/cpp/internal/pending_response.h>
#include <lib/fidl/cpp/internal/weak_stub_controller.h>

#include "base/bind.h"
#include "base/fuchsia/async_dispatcher.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/test/test_timeouts.h"
#include "gin/converter.h"
#include "gin/modules/console.h"
#include "gin/object_template_builder.h"
#include "gin/public/isolate_holder.h"
#include "gin/shell_runner.h"
#include "gin/test/v8_test.h"
#include "gin/try_catch.h"
#include "tools/fuchsia/fidlgen_js/fidl/fidljstest/cpp/fidl.h"
#include "tools/fuchsia/fidlgen_js/runtime/zircon.h"
#include "v8/include/v8.h"

static const char kRuntimeFile[] =
    "/pkg/tools/fuchsia/fidlgen_js/runtime/fidl.mjs";
static const char kTestBindingFile[] =
    "/pkg/tools/fuchsia/fidlgen_js/fidl/fidljstest/js/fidl.js";

class FidlGenJsTestShellRunnerDelegate : public gin::ShellRunnerDelegate {
 public:
  FidlGenJsTestShellRunnerDelegate() {}

  v8::Local<v8::ObjectTemplate> GetGlobalTemplate(
      gin::ShellRunner* runner,
      v8::Isolate* isolate) override {
    v8::Local<v8::ObjectTemplate> templ =
        gin::ObjectTemplateBuilder(isolate).Build();
    gin::Console::Register(isolate, templ);
    return templ;
  }

  void UnhandledException(gin::ShellRunner* runner,
                          gin::TryCatch& try_catch) override {
    LOG(ERROR) << try_catch.GetStackTrace();
    ADD_FAILURE();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FidlGenJsTestShellRunnerDelegate);
};

using FidlGenJsTest = gin::V8Test;

TEST_F(FidlGenJsTest, BasicJSSetup) {
  v8::Isolate* isolate = instance_->isolate();

  std::string source = "log('this is a log'); this.stuff = 'HAI';";
  FidlGenJsTestShellRunnerDelegate delegate;
  gin::ShellRunner runner(&delegate, isolate);
  gin::Runner::Scope scope(&runner);
  runner.Run(source, "test.js");

  std::string result;
  EXPECT_TRUE(gin::Converter<std::string>::FromV8(
      isolate, runner.global()->Get(gin::StringToV8(isolate, "stuff")),
      &result));
  EXPECT_EQ("HAI", result);
}

void LoadAndSource(gin::ShellRunner* runner, const base::FilePath& filename) {
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(filename, &contents));

  runner->Run(contents, filename.MaybeAsASCII());
}

class BindingsSetupHelper {
 public:
  explicit BindingsSetupHelper(v8::Isolate* isolate)
      : isolate_(isolate),
        handle_scope_(isolate),
        delegate_(),
        runner_(&delegate_, isolate),
        scope_(&runner_),
        zx_bindings_(
            std::make_unique<fidljs::ZxBindings>(isolate, runner_.global())) {
    // TODO(scottmg): Figure out how to set up v8 import hooking and make
    // fidl_Xyz into $fidl.Xyz. Manually inject the runtime support js files
    // for now. https://crbug.com/883496.
    LoadAndSource(&runner_, base::FilePath(kRuntimeFile));
    LoadAndSource(&runner_, base::FilePath(kTestBindingFile));

    zx_status_t status = zx::channel::create(0, &server_, &client_);
    EXPECT_EQ(status, ZX_OK);

    runner_.global()->Set(gin::StringToSymbol(isolate, "testHandle"),
                          gin::ConvertToV8(isolate, client_.get()));
  }

  template <class T>
  T Get(const std::string& name) {
    T t;
    EXPECT_TRUE(gin::Converter<T>::FromV8(
        isolate_, runner_.global()->Get(gin::StringToV8(isolate_, name)), &t));
    return t;
  }

  void DestroyBindingsForTesting() { zx_bindings_.reset(); }

  zx::channel& server() { return server_; }
  zx::channel& client() { return client_; }
  gin::ShellRunner& runner() { return runner_; }

 private:
  v8::Isolate* isolate_;
  v8::HandleScope handle_scope_;
  FidlGenJsTestShellRunnerDelegate delegate_;
  gin::ShellRunner runner_;
  gin::Runner::Scope scope_;
  std::unique_ptr<fidljs::ZxBindings> zx_bindings_;
  zx::channel server_;
  zx::channel client_;

  DISALLOW_COPY_AND_ASSIGN(BindingsSetupHelper);
};

TEST_F(FidlGenJsTest, CreateChannelPair) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  std::string source = R"(
    var result = zx.channelCreate();
    this.result_status = result.status;
    this.result_h1 = result.first;
    this.result_h2 = result.second;
    if (result.status == zx.ZX_OK) {
      zx.handleClose(result.first);
      zx.handleClose(result.second);
    }
  )";

  helper.runner().Run(source, "test.js");

  auto status = helper.Get<zx_status_t>("result_status");
  EXPECT_EQ(status, ZX_OK);

  auto h1 = helper.Get<zx_handle_t>("result_h1");
  EXPECT_NE(h1, ZX_HANDLE_INVALID);

  auto h2 = helper.Get<zx_handle_t>("result_h2");
  EXPECT_NE(h2, ZX_HANDLE_INVALID);
}

class TestolaImpl : public fidljstest::Testola {
 public:
  TestolaImpl() = default;
  ~TestolaImpl() override {}

  void DoSomething() override { was_do_something_called_ = true; }

  void PrintInt(int32_t number) override { received_int_ = number; }

  void PrintMsg(fidl::StringPtr message) override {
    std::string as_str = message.get();
    received_msg_ = as_str;
  }

  void VariousArgs(fidljstest::Blorp blorp,
                   fidl::StringPtr msg,
                   fidl::VectorPtr<uint32_t> stuff) override {
    std::string msg_as_str = msg.get();
    std::vector<uint32_t> stuff_as_vec = stuff.get();
    various_blorp_ = blorp;
    various_msg_ = msg_as_str;
    various_stuff_ = stuff_as_vec;
  }

  void WithResponse(int32_t a, int32_t b, WithResponseCallback sum) override {
    sum(a + b);
  }

  bool was_do_something_called() const { return was_do_something_called_; }
  int32_t received_int() const { return received_int_; }
  const std::string& received_msg() const { return received_msg_; }

  fidljstest::Blorp various_blorp() const { return various_blorp_; }
  const std::string& various_msg() const { return various_msg_; }
  const std::vector<uint32_t>& various_stuff() const { return various_stuff_; }

 private:
  bool was_do_something_called_ = false;
  int32_t received_int_ = -1;
  std::string received_msg_;
  fidljstest::Blorp various_blorp_;
  std::string various_msg_;
  std::vector<uint32_t> various_stuff_;

  DISALLOW_COPY_AND_ASSIGN(TestolaImpl);
};

TEST_F(FidlGenJsTest, RawReceiveFidlMessage) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  // Send the data from the JS side into the channel.
  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);
    proxy.DoSomething();
  )";
  helper.runner().Run(source, "test.js");

  // Read it out, decode, and confirm it was dispatched.
  TestolaImpl testola_impl;
  fidljstest::Testola_Stub stub(&testola_impl);
  uint8_t data[1024];
  zx_handle_t handles[1];
  uint32_t actual_bytes, actual_handles;
  ASSERT_EQ(helper.server().read(0, data, base::size(data), &actual_bytes,
                                 handles, base::size(handles), &actual_handles),
            ZX_OK);
  EXPECT_EQ(actual_bytes, 16u);
  EXPECT_EQ(actual_handles, 0u);

  fidl::Message message(
      fidl::BytePart(data, actual_bytes, actual_bytes),
      fidl::HandlePart(handles, actual_handles, actual_handles));
  stub.Dispatch_(std::move(message), fidl::internal::PendingResponse());

  EXPECT_TRUE(testola_impl.was_do_something_called());
}

TEST_F(FidlGenJsTest, RawReceiveFidlMessageWithSimpleArg) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  // Send the data from the JS side into the channel.
  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);
    proxy.PrintInt(12345);
  )";
  helper.runner().Run(source, "test.js");

  // Read it out, decode, and confirm it was dispatched.
  TestolaImpl testola_impl;
  fidljstest::Testola_Stub stub(&testola_impl);
  uint8_t data[1024];
  zx_handle_t handles[1];
  uint32_t actual_bytes, actual_handles;
  ASSERT_EQ(helper.server().read(0, data, base::size(data), &actual_bytes,
                                 handles, base::size(handles), &actual_handles),
            ZX_OK);
  // 24 rather than 20 because everything's 8 aligned.
  EXPECT_EQ(actual_bytes, 24u);
  EXPECT_EQ(actual_handles, 0u);

  fidl::Message message(
      fidl::BytePart(data, actual_bytes, actual_bytes),
      fidl::HandlePart(handles, actual_handles, actual_handles));
  stub.Dispatch_(std::move(message), fidl::internal::PendingResponse());

  EXPECT_EQ(testola_impl.received_int(), 12345);
}

TEST_F(FidlGenJsTest, RawReceiveFidlMessageWithStringArg) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  // Send the data from the JS side into the channel.
  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);
    proxy.PrintMsg('Ça c\'est a 你好 from deep in JS');
  )";
  helper.runner().Run(source, "test.js");

  // Read it out, decode, and confirm it was dispatched.
  TestolaImpl testola_impl;
  fidljstest::Testola_Stub stub(&testola_impl);
  uint8_t data[1024];
  zx_handle_t handles[1];
  uint32_t actual_bytes, actual_handles;
  ASSERT_EQ(helper.server().read(0, data, base::size(data), &actual_bytes,
                                 handles, base::size(handles), &actual_handles),
            ZX_OK);
  EXPECT_EQ(actual_handles, 0u);

  fidl::Message message(
      fidl::BytePart(data, actual_bytes, actual_bytes),
      fidl::HandlePart(handles, actual_handles, actual_handles));
  stub.Dispatch_(std::move(message), fidl::internal::PendingResponse());

  EXPECT_EQ(testola_impl.received_msg(), "Ça c'est a 你好 from deep in JS");
}

TEST_F(FidlGenJsTest, RawReceiveFidlMessageWithMultipleArgs) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  // Send the data from the JS side into the channel.
  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);
    proxy.VariousArgs(Blorp.GAMMA, 'zippy zap', [ 999, 987, 123456 ]);
  )";
  helper.runner().Run(source, "test.js");

  // Read it out, decode, and confirm it was dispatched.
  TestolaImpl testola_impl;
  fidljstest::Testola_Stub stub(&testola_impl);
  uint8_t data[1024];
  zx_handle_t handles[1];
  uint32_t actual_bytes, actual_handles;
  ASSERT_EQ(helper.server().read(0, data, base::size(data), &actual_bytes,
                                 handles, base::size(handles), &actual_handles),
            ZX_OK);
  EXPECT_EQ(actual_handles, 0u);

  fidl::Message message(
      fidl::BytePart(data, actual_bytes, actual_bytes),
      fidl::HandlePart(handles, actual_handles, actual_handles));
  stub.Dispatch_(std::move(message), fidl::internal::PendingResponse());

  EXPECT_EQ(testola_impl.various_blorp(), fidljstest::Blorp::GAMMA);
  EXPECT_EQ(testola_impl.various_msg(), "zippy zap");
  ASSERT_EQ(testola_impl.various_stuff().size(), 3u);
  EXPECT_EQ(testola_impl.various_stuff()[0], 999u);
  EXPECT_EQ(testola_impl.various_stuff()[1], 987u);
  EXPECT_EQ(testola_impl.various_stuff()[2], 123456u);
}

TEST_F(FidlGenJsTest, RawWithResponse) {
  base::AsyncDispatcher dispatcher;

  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  TestolaImpl testola_impl;
  fidl::Binding<fidljstest::Testola> binding(&testola_impl);
  binding.Bind(std::move(helper.server()), &dispatcher);

  // Send the data from the JS side into the channel.
  std::string source = R"(
      var proxy = new TestolaProxy();
      proxy.$bind(testHandle);
      this.sum_result = -1;
      proxy.WithResponse(72, 99)
           .then(sum => {
              this.sum_result = sum;
            })
           .catch((e) => log('HOT GARBAGE: ' + e));
    )";
  helper.runner().Run(source, "test.js");

  // Run the dispatcher until the response or timeout.
  while (helper.Get<int>("sum_result") == -1) {
    ASSERT_EQ(dispatcher.DispatchOrWaitUntil(zx_deadline_after(
                  ZX_MSEC(TestTimeouts::action_timeout().InMilliseconds()))),
              ZX_OK);
  }

  // Confirm that the response was received with the correct value.
  auto sum_result = helper.Get<int>("sum_result");
  EXPECT_EQ(sum_result, 72 + 99);
}

TEST_F(FidlGenJsTest, NoResponseBeforeTearDown) {
  base::AsyncDispatcher dispatcher;

  v8::Isolate* isolate = instance_->isolate();

  BindingsSetupHelper helper(isolate);

  TestolaImpl testola_impl;
  fidl::Binding<fidljstest::Testola> binding(&testola_impl);
  binding.Bind(std::move(helper.server()), &dispatcher);

  // Send the data from the JS side into the channel.
  std::string source = R"(
      var proxy = new TestolaProxy();
      proxy.$bind(testHandle);
      this.resolved = false;
      this.rejected = false;
      this.excepted = false;
      proxy.WithResponse(1, 2)
           .then(sum => {
              this.resolved = true;
            }, () => {
              this.rejected = true;
            })
           .catch((e) => {
             log('HOT GARBAGE: ' + e);
             this.excepted = true;
           })
    )";
  helper.runner().Run(source, "test.js");

  // Run the dispatcher, to read and queue the response.
  EXPECT_EQ(dispatcher.DispatchOrWaitUntil(ZX_TIME_INFINITE), ZX_OK);

  // This causes outstanding waits to be canceled.
  helper.DestroyBindingsForTesting();

  EXPECT_FALSE(helper.Get<bool>("resolved"));
  EXPECT_TRUE(helper.Get<bool>("rejected"));
  EXPECT_FALSE(helper.Get<bool>("excepted"));
}

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
