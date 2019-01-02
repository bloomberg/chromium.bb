// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/waitable_event.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "chrome/chrome_cleaner/interfaces/json_parser.mojom.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/json_parser/sandbox_setup_hooks.h"
#include "chrome/chrome_cleaner/json_parser/sandbox_target_hooks.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "testing/multiprocess_func_list.h"

using base::WaitableEvent;

namespace chrome_cleaner {

namespace {

const char kTestJsonKey[] = "name";
const char kTestJsonValue[] = "Jason";
const char kTestJsonText[] = "{ \"name\": \"Jason\" }";
const char kInvalidJsonText[] = "{ name: jason }";

class JsonParserSandboxSetupTest : public base::MultiProcessTest {
 public:
  JsonParserSandboxSetupTest()
      : json_parser_ptr_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {}

  void SetUp() override {
    mojo_task_runner_ = MojoTaskRunner::Create();
    JsonParserSandboxSetupHooks setup_hooks(
        mojo_task_runner_.get(), base::BindOnce([] {
          FAIL() << "JsonParser sandbox connection error";
        }));
    ASSERT_EQ(RESULT_CODE_SUCCESS,
              StartSandboxTarget(MakeCmdLine("JsonParserSandboxTargetMain"),
                                 &setup_hooks, SandboxType::kTest));
    json_parser_ptr_ = setup_hooks.TakeJsonParserPtr();
  }

 protected:
  scoped_refptr<MojoTaskRunner> mojo_task_runner_;
  UniqueJsonParserPtr json_parser_ptr_;
};

void ParseCallbackExpectedKeyValue(const std::string& expected_key,
                                   const std::string& expected_value,
                                   WaitableEvent* done,
                                   base::Optional<base::Value> value,
                                   const base::Optional<std::string>& error) {
  ASSERT_FALSE(error.has_value());
  ASSERT_TRUE(value.has_value());
  ASSERT_TRUE(value->is_dict());
  const base::DictionaryValue* dict;
  ASSERT_TRUE(value->GetAsDictionary(&dict));

  std::string string_value;
  ASSERT_TRUE(dict->GetString(expected_key, &string_value));
  EXPECT_EQ(expected_value, string_value);
  done->Signal();
}

void ParseCallbackExpectedError(WaitableEvent* done,
                                base::Optional<base::Value> value,
                                const base::Optional<std::string>& error) {
  ASSERT_TRUE(error.has_value());
  EXPECT_FALSE(error->empty());
  done->Signal();
}

}  // namespace

MULTIPROCESS_TEST_MAIN(JsonParserSandboxTargetMain) {
  sandbox::TargetServices* sandbox_target_services =
      sandbox::SandboxFactory::GetTargetServices();
  CHECK(sandbox_target_services);

  EXPECT_EQ(RESULT_CODE_SUCCESS,
            RunJsonParserSandbox(*base::CommandLine::ForCurrentProcess(),
                                 sandbox_target_services));

  return ::testing::Test::HasNonfatalFailure();
}

TEST_F(JsonParserSandboxSetupTest, ParseJsonSandboxed) {
  WaitableEvent done(WaitableEvent::ResetPolicy::MANUAL,
                     WaitableEvent::InitialState::NOT_SIGNALED);

  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](mojom::JsonParserPtr* json_parser_ptr, WaitableEvent* done) {
            (*json_parser_ptr)
                ->Parse(kTestJsonText,
                        base::BindOnce(&ParseCallbackExpectedKeyValue,
                                       kTestJsonKey, kTestJsonValue, done));
          },
          json_parser_ptr_.get(), &done));
  EXPECT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));
}

TEST_F(JsonParserSandboxSetupTest, ParseInvalidJsonSandboxed) {
  WaitableEvent done(WaitableEvent::ResetPolicy::MANUAL,
                     WaitableEvent::InitialState::NOT_SIGNALED);

  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](mojom::JsonParserPtr* json_parser_ptr, WaitableEvent* done) {
            (*json_parser_ptr)
                ->Parse(kInvalidJsonText,
                        base::BindOnce(&ParseCallbackExpectedError, done));
          },
          json_parser_ptr_.get(), &done));
  EXPECT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));
}

}  // namespace chrome_cleaner
