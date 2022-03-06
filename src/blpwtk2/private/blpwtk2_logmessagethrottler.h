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

#ifndef INCLUDED_BLPWTK2_LOG_MESSAGE_THROTTLER_H
#define INCLUDED_BLPWTK2_LOG_MESSAGE_THROTTLER_H

#include <blpwtk2_config.h>
#include "blpwtk2/public/blpwtk2_toolkitcreateparams.h"

#include <memory>
#include <string>

namespace blpwtk2 {

class LogMessageThrottlerImpl;

// LogMessageThrottler apply different strategy for logging messages.
class LogMessageThrottler {
 public:
  LogMessageThrottler(
      ToolkitCreateParams::LogThrottleType throttleType,
      ToolkitCreateParams::LogMessageHandler logHandler,
      ToolkitCreateParams::ConsoleLogMessageHandler consoleHander);

  ~LogMessageThrottler();

  // return true if the message is actually written out
  bool writeLog(int severity,
                const char* file,
                int line,
                size_t message_start,
                const std::string& str);

  // return true if the message is actually written out
  bool writeConsole(int severity,
                    const std::string& file,
                    int line,
                    int column,
                    const std::string& message,
                    const std::string& stack_trace);

 private:
  std::unique_ptr<LogMessageThrottlerImpl> impl_;
};

}  // namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_LOG_MESSAGE_THROTTLER_H
