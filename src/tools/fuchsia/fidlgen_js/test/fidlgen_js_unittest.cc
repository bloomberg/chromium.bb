// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
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

TEST_F(FidlGenJsTest, CreateChannelPair) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);

  FidlGenJsTestShellRunnerDelegate delegate;
  gin::ShellRunner runner(&delegate, isolate);
  gin::Runner::Scope scope(&runner);

  fidljs::InjectZxBindings(isolate, runner.global());

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

  runner.Run(source, "test.js");

  zx_status_t status = ZX_ERR_INTERNAL;
  EXPECT_TRUE(gin::Converter<zx_status_t>::FromV8(
      isolate, runner.global()->Get(gin::StringToV8(isolate, "result_status")),
      &status));
  EXPECT_EQ(status, ZX_OK);

  zx_handle_t handle = ZX_HANDLE_INVALID;

  EXPECT_TRUE(gin::Converter<zx_handle_t>::FromV8(
      isolate, runner.global()->Get(gin::StringToV8(isolate, "result_h1")),
      &handle));
  EXPECT_NE(handle, ZX_HANDLE_INVALID);

  EXPECT_TRUE(gin::Converter<zx_handle_t>::FromV8(
      isolate, runner.global()->Get(gin::StringToV8(isolate, "result_h2")),
      &handle));
  EXPECT_NE(handle, ZX_HANDLE_INVALID);
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

void LoadAndSource(gin::ShellRunner* runner, const base::FilePath& filename) {
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(filename, &contents));

  runner->Run(contents, filename.MaybeAsASCII());
}

class BindingsSetupHelper {
 public:
  explicit BindingsSetupHelper(v8::Isolate* isolate)
      : handle_scope_(isolate), delegate_(), runner_(&delegate_, isolate) {
    gin::Runner::Scope scope(&runner_);

    fidljs::InjectZxBindings(isolate, runner_.global());

    // TODO(crbug.com/883496): Figure out how to set up v8 import hooking and
    // make fidl_Xyz into $fidl.Xyz. Manually inject the runtime support js
    // files for now.
    LoadAndSource(&runner_, base::FilePath(kRuntimeFile));
    LoadAndSource(&runner_, base::FilePath(kTestBindingFile));

    zx_status_t status = zx::channel::create(0, &server_, &client_);
    EXPECT_EQ(status, ZX_OK);

    runner_.global()->Set(gin::StringToSymbol(isolate, "testHandle"),
                          gin::ConvertToV8(isolate, client_.get()));
  }

  zx::channel& server() { return server_; }
  zx::channel& client() { return client_; }
  gin::ShellRunner& runner() { return runner_; }

 private:
  v8::HandleScope handle_scope_;
  FidlGenJsTestShellRunnerDelegate delegate_;
  gin::ShellRunner runner_;
  zx::channel server_;
  zx::channel client_;

  DISALLOW_COPY_AND_ASSIGN(BindingsSetupHelper);
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

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
