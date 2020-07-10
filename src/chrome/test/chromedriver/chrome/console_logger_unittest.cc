// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/console_logger.h"

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/queue.h"
#include "base/format_macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/log.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_devtools_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeDevToolsClient : public StubDevToolsClient {
 public:
  explicit FakeDevToolsClient(const std::string& id)
      : id_(id), listener_(nullptr) {}
  ~FakeDevToolsClient() override {}

  std::string PopSentCommand() {
    std::string command;
    if (!sent_command_queue_.empty()) {
      command = sent_command_queue_.front();
      sent_command_queue_.pop();
    }
    return command;
  }

  Status TriggerEvent(const std::string& method,
                    const base::DictionaryValue& params) {
    return listener_->OnEvent(this, method, params);
  }

  // Overridden from DevToolsClient:
  Status ConnectIfNecessary() override { return listener_->OnConnected(this); }

  Status SendCommandAndGetResult(
      const std::string& method,
      const base::DictionaryValue& params,
      std::unique_ptr<base::DictionaryValue>* result) override {
    if (method == "Log.enable") {
      // FakeDevToolsClient emulates Chrome 53 and earlier versions, which do
      // not implement the Log.enable command. ConsoleLogger functionality for
      // Chrome 54+ is tested by end-to-end tests in run_py_tests.py.
      return Status(kUnknownError, "unhandled inspector error: "
          "{\"code\":-32601,\"message\":\"'Log.enable' wasn't found\"}");
    }
    sent_command_queue_.push(method);
    return Status(kOk);
  }

  void AddListener(DevToolsEventListener* listener) override {
    CHECK(!listener_);
    listener_ = listener;
  }

  const std::string& GetId() override { return id_; }

 private:
  const std::string id_;  // WebView id.
  base::queue<std::string> sent_command_queue_;  // Commands that were sent.
  DevToolsEventListener* listener_;  // The fake allows only one event listener.
};

struct LogEntry {
  const base::Time timestamp;
  const Log::Level level;
  const std::string source;
  const std::string message;

  LogEntry(const base::Time& timestamp,
           Log::Level level,
           const std::string& source,
           const std::string& message)
      : timestamp(timestamp), level(level), source(source), message(message) {}
};

class FakeLog : public Log {
 public:
  void AddEntryTimestamped(const base::Time& timestamp,
                           Level level,
                           const std::string& source,
                           const std::string& message) override;

  bool Emptied() const override;

  const std::vector<std::unique_ptr<LogEntry>>& GetEntries() {
    return entries_;
  }

 private:
  std::vector<std::unique_ptr<LogEntry>> entries_;
};

void FakeLog::AddEntryTimestamped(const base::Time& timestamp,
                                  Level level,
                                  const std::string& source,
                                  const std::string& message) {
  entries_.push_back(
      std::make_unique<LogEntry>(timestamp, level, source, message));
}

bool FakeLog::Emptied() const {
  return true;
}

void ValidateLogEntry(const LogEntry *entry,
                      Log::Level expected_level,
                      const std::string& expected_source,
                      const std::string& expected_message) {
  EXPECT_EQ(expected_level, entry->level);
  EXPECT_EQ(expected_source, entry->source);
  EXPECT_LT(0, entry->timestamp.ToTimeT());
  EXPECT_EQ(expected_message, entry->message);
}

void ConsoleLogParams(base::DictionaryValue* out_params,
                      const char* source,
                      const char* url,
                      const char* level,
                      int line,
                      int column,
                      const char* text) {
  if (source)
    out_params->SetString("message.source", source);
  if (url)
    out_params->SetString("message.url", url);
  if (level)
    out_params->SetString("message.level", level);
  if (line != -1)
    out_params->SetInteger("message.line", line);
  if (column != -1)
    out_params->SetInteger("message.column", column);
  if (text)
    out_params->SetString("message.text", text);
}

}  // namespace

TEST(ConsoleLogger, ConsoleMessages) {
  FakeDevToolsClient client("webview");
  FakeLog log;
  ConsoleLogger logger(&log);

  client.AddListener(&logger);
  logger.OnConnected(&client);
  EXPECT_EQ("Console.enable", client.PopSentCommand());
  EXPECT_TRUE(client.PopSentCommand().empty());

  base::DictionaryValue params1;  // All fields are set.
  ConsoleLogParams(&params1, "source1", "url1", "verbose", 10, 1, "text1");
  ASSERT_EQ(kOk, client.TriggerEvent("Console.messageAdded", params1).code());
  // Ignored -- wrong method.
  ASSERT_EQ(kOk, client.TriggerEvent("Console.gaga", params1).code());

  base::DictionaryValue params2;  // All optionals are not set.
  ConsoleLogParams(&params2, "source2", nullptr, "log", -1, -1, "text2");
  ASSERT_EQ(kOk, client.TriggerEvent("Console.messageAdded", params2).code());

  base::DictionaryValue params3;  // Line without column, no source.
  ConsoleLogParams(&params3, nullptr, "url3", "warning", 30, -1, "text3");
  ASSERT_EQ(kOk, client.TriggerEvent("Console.messageAdded", params3).code());

  base::DictionaryValue params4;  // Column without line.
  ConsoleLogParams(&params4, "source4", "url4", "error", -1, 4, "text4");
  ASSERT_EQ(kOk, client.TriggerEvent("Console.messageAdded", params4).code());

  base::DictionaryValue params5;  // Bad level name.
  ConsoleLogParams(&params5, "source5", "url5", "gaga", 50, 5, "ulala");
  ASSERT_EQ(kOk, client.TriggerEvent("Console.messageAdded", params5).code());

  base::DictionaryValue params6;  // Unset level.
  ConsoleLogParams(&params6, "source6", "url6", nullptr, 60, 6, nullptr);
  ASSERT_EQ(kOk, client.TriggerEvent("Console.messageAdded", params6).code());

  base::DictionaryValue params7;  // No text.
  ConsoleLogParams(&params7, "source7", "url7", "log", -1, -1, nullptr);
  ASSERT_EQ(kOk, client.TriggerEvent("Console.messageAdded", params7).code());

  base::DictionaryValue params8;  // No message object.
  params8.SetInteger("gaga", 8);
  ASSERT_EQ(kOk, client.TriggerEvent("Console.messageAdded", params8).code());

  EXPECT_TRUE(client.PopSentCommand().empty());  // No other commands sent.

  ASSERT_EQ(8u, log.GetEntries().size());
  ValidateLogEntry(log.GetEntries()[0].get(), Log::kDebug, "source1",
                   "url1 10:1 text1");
  ValidateLogEntry(log.GetEntries()[1].get(), Log::kInfo, "source2",
                   "source2 - text2");
  ValidateLogEntry(log.GetEntries()[2].get(), Log::kWarning, "",
                   "url3 30 text3");
  ValidateLogEntry(log.GetEntries()[3].get(), Log::kError, "source4",
                   "url4 - text4");
  ValidateLogEntry(
      log.GetEntries()[4].get(), Log::kWarning, "",
      "{\"message\":{\"column\":5,\"level\":\"gaga\",\"line\":50,"
      "\"source\":\"source5\",\"text\":\"ulala\",\"url\":\"url5\"}}");
  ValidateLogEntry(log.GetEntries()[5].get(), Log::kWarning, "",
                   "{\"message\":{\"column\":6,\"line\":60,"
                   "\"source\":\"source6\",\"url\":\"url6\"}}");
  ValidateLogEntry(log.GetEntries()[6].get(), Log::kWarning, "",
                   "{\"message\":{\"level\":\"log\","
                   "\"source\":\"source7\",\"url\":\"url7\"}}");
  ValidateLogEntry(log.GetEntries()[7].get(), Log::kWarning, "",
                   "{\"gaga\":8}");
}
