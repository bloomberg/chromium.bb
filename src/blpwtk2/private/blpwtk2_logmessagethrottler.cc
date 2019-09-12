/*
 * Copyright (C) 2019 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "blpwtk2_logmessagethrottler.h"

#include "blpwtk2/private/blpwtk2_statics.h"
#include "blpwtk2/public/blpwtk2_stringref.h"

#include <base/logging.h>
#include <base/message_loop/message_loop_current.h>
#include <base/optional.h>
#include <base/time/time.h>

#include <deque>

namespace blpwtk2 {
static ToolkitCreateParams::LogMessageSeverity decodeLogSeverity(int severity) {
  switch (severity) {
    case logging::LOG_INFO:
      return ToolkitCreateParams::kSeverityInfo;
    case logging::LOG_WARNING:
      return ToolkitCreateParams::kSeverityWarning;
    case logging::LOG_ERROR:
      return ToolkitCreateParams::kSeverityError;
    case logging::LOG_FATAL:
      return ToolkitCreateParams::kSeverityFatal;
    default:
      return ToolkitCreateParams::kSeverityVerbose;
  }
}

class LogMessageThrottlerImpl {
 public:
  LogMessageThrottlerImpl(
      ToolkitCreateParams::LogMessageHandler logHandler,
      ToolkitCreateParams::ConsoleLogMessageHandler consoleHander)
      : logHandler_(logHandler), consoleHander_(consoleHander) {}

  virtual ~LogMessageThrottlerImpl() = default;

  bool acceptWriteLog() const { return logHandler_ != nullptr; }

  bool acceptWriteConsole() const { return consoleHander_ != nullptr; }

  virtual bool writeLog(int severity,
                        const char* file,
                        int line,
                        size_t message_start,
                        const std::string& str) = 0;

  virtual bool writeConsole(int severity,
                            const std::string& file,
                            int line,
                            int column,
                            const std::string& message,
                            const std::string& stack_trace) = 0;

 protected:
  using WriteFunc = std::function<void()>;
  ToolkitCreateParams::LogMessageHandler logHandler_;
  ToolkitCreateParams::ConsoleLogMessageHandler consoleHander_;
};

// NoThrottleLogMessageThrottler does not throttle any messages
class NoThrottleLogMessageThrottler : public LogMessageThrottlerImpl {
 public:
  using LogMessageThrottlerImpl::LogMessageThrottlerImpl;

  ~NoThrottleLogMessageThrottler() final = default;

  bool writeLog(int severity,
                const char* file,
                int line,
                size_t message_start,
                const std::string& str) final;

  bool writeConsole(int severity,
                    const std::string& file,
                    int line,
                    int column,
                    const std::string& message,
                    const std::string& stack_trace) final;
};

// WarningLogMessageThrottler checks warning, error, fatal severity messages.
// It uses a non-cache leaky bucket algorithm for throttling.
// In case of a log message should be throttled, WarningLogMessageThrottler
// simple does NOT write it out without caching. It does not cache the messages
// to avoid complications of timer-based update and flushing issues during
// process crash.
class WarningLogMessageThrottler : public LogMessageThrottlerImpl {
 public:
  WarningLogMessageThrottler(
      ToolkitCreateParams::LogMessageHandler logHandler,
      ToolkitCreateParams::ConsoleLogMessageHandler consoleHander);
  ~WarningLogMessageThrottler() final;

  bool writeLog(int severity,
                const char* file,
                int line,
                size_t message_start,
                const std::string& str) final;

  bool writeConsole(int severity,
                    const std::string& file,
                    int line,
                    int column,
                    const std::string& message,
                    const std::string& stack_trace) final;

 private:
  // return write out or not
  bool leakyBucket(int severity,
                  WriteFunc msg_write_func,
                  WriteFunc throttle_msg_write_func,
                  std::queue<base::Time>& time_queue,
                  std::size_t& num_msg_throttled) const;

  static constexpr int throttle_rate_per_minute_{10};
  static constexpr int throttle_bucket_minutes_{2};
  static constexpr int throttle_bucket_seconds_{throttle_bucket_minutes_ * 60};
  static constexpr int max_queue_size_{throttle_rate_per_minute_ *
                                       throttle_bucket_minutes_};

  std::queue<base::Time> log_times_;
  std::queue<base::Time> console_times_;
  std::size_t num_log_throttled_{0};
  std::size_t num_console_throttled_{0};
};

LogMessageThrottler::LogMessageThrottler(
    ToolkitCreateParams::LogThrottleType throttleType,
    ToolkitCreateParams::LogMessageHandler logHandler,
    ToolkitCreateParams::ConsoleLogMessageHandler consoleHander) {
  switch (throttleType) {
    case ToolkitCreateParams::LogThrottleType::kNoThrottle:
      impl_ = std::make_unique<NoThrottleLogMessageThrottler>(logHandler,
                                                              consoleHander);
      break;
    default:
      impl_ = std::make_unique<WarningLogMessageThrottler>(logHandler,
                                                           consoleHander);
  }
}

LogMessageThrottler::~LogMessageThrottler() = default;

bool LogMessageThrottler::writeLog(int severity,
                                   const char* file,
                                   int line,
                                   size_t message_start,
                                   const std::string& str) {
  if (impl_->acceptWriteLog()) {
    return impl_->writeLog(severity, file, line, message_start, str);
  }
  return false;
}

bool LogMessageThrottler::writeConsole(int severity,
                                       const std::string& file,
                                       int line,
                                       int column,
                                       const std::string& message,
                                       const std::string& stack_trace) {
  if (impl_->acceptWriteConsole()) {
    return impl_->writeConsole(severity, file, line, column, message,
                               stack_trace);
  }
  return false;
}

//=================================================================

bool NoThrottleLogMessageThrottler::writeLog(int severity,
                                             const char* file,
                                             int line,
                                             size_t message_start,
                                             const std::string& str) {
  (*logHandler_)(decodeLogSeverity(severity), file, line,
                 str.c_str() + message_start);
  return true;
}

bool NoThrottleLogMessageThrottler::writeConsole(
    int severity,
    const std::string& file,
    int line,
    int column,
    const std::string& message,
    const std::string& stack_trace) {
  (*consoleHander_)(decodeLogSeverity(severity),
                    StringRef(file.data(), file.length()), line, column,
                    StringRef(message.data(), message.length()),
                    StringRef(stack_trace.data(), stack_trace.length()));
  return true;
}

WarningLogMessageThrottler::WarningLogMessageThrottler(
    ToolkitCreateParams::LogMessageHandler logHandler,
    ToolkitCreateParams::ConsoleLogMessageHandler consoleHander)
    : LogMessageThrottlerImpl(logHandler, consoleHander) {}

WarningLogMessageThrottler::~WarningLogMessageThrottler() = default;

bool WarningLogMessageThrottler::writeLog(int severity,
                                          const char* file,
                                          int line,
                                          size_t message_start,
                                          const std::string& str) {
  auto msg_func = [=]() {
    (*logHandler_)(decodeLogSeverity(severity), file, line,
                   str.c_str() + message_start);
  };
  // Also add an error message to indicate some messages are throttled
  auto throttle_msg_func = [=]() {
    (*logHandler_)(
        ToolkitCreateParams::kSeverityError, __FILE__, __LINE__,
        (std::to_string(num_log_throttled_) + " messages have been throttled")
            .c_str());
  };
  return leakyBucket(severity, std::move(msg_func), std::move(throttle_msg_func),
                    log_times_, num_log_throttled_);
}

bool WarningLogMessageThrottler::writeConsole(int severity,
                                              const std::string& file,
                                              int line,
                                              int column,
                                              const std::string& message,
                                              const std::string& stack_trace) {
  auto msg_func = [=]() {
    (*consoleHander_)(decodeLogSeverity(severity),
                      StringRef(file.data(), file.length()), line, column,
                      StringRef(message.data(), message.length()),
                      StringRef(stack_trace.data(), stack_trace.length()));
  };
  // Also add an error message to indicate some messages are throttled
  auto throttle_msg_func = [=]() {
    (*consoleHander_)(ToolkitCreateParams::kSeverityError, StringRef(__FILE__),
                      __LINE__, 0,
                      StringRef(std::to_string(num_console_throttled_) +
                                " messages have been throttled"),
                      StringRef());
  };
  return leakyBucket(severity, std::move(msg_func), std::move(throttle_msg_func),
                    console_times_, num_console_throttled_);
}

bool WarningLogMessageThrottler::leakyBucket(
    int severity,
    WriteFunc msg_write_func,
    WriteFunc throttle_msg_write_func,
    std::queue<base::Time>& time_queue,
    std::size_t& num_msg_throttled) const {
  bool elapse_long_enough = false;
  bool is_serious =
      (severity >= logging::LOG_WARNING && severity <= logging::LOG_FATAL);
  base::Optional<base::Time> opt_time;

  if (is_serious || num_msg_throttled) {
    opt_time = base::Time::Now();
    elapse_long_enough =
        (!time_queue.empty()) &&
        (*opt_time - time_queue.front()).InSeconds() > throttle_bucket_seconds_;
  }
  bool acceptable = true;

  if (is_serious) {
    acceptable = elapse_long_enough || (time_queue.size() < max_queue_size_);
    if (acceptable) {
      if (time_queue.size() >= max_queue_size_) {
        time_queue.pop();
      }
      time_queue.push(*opt_time);
    } else {
      ++num_msg_throttled;
    }
  }
  if (elapse_long_enough && num_msg_throttled) {
    throttle_msg_write_func();
    num_msg_throttled = 0;
  }
  if (acceptable) {
    msg_write_func();
  }
  return acceptable;
}

}  // namespace blpwtk2
