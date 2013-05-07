// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_LIBJINGLE_OVERRIDES_LOGGING_LOG_MESSAGE_DELEGATE_H_
#define THIRD_PARTY_LIBJINGLE_OVERRIDES_LOGGING_LOG_MESSAGE_DELEGATE_H_

#include <string>

namespace talk_base {

// This interface is implemented by a handler in Chromium and used by the
// overridden logging.h/cc in libjingle. The purpose is to forward libjingle
// logging messages to Chromium (besides to the ordinary logging stream) that
// will be used for diagnostic purposes. LogMessage can be called on any thread
// used by libjingle.
class LogMessageDelegate {
 public:
  virtual void LogMessage(const std::string& message) = 0;

 protected:
  virtual ~LogMessageDelegate() {}
};

}  // namespace talk_base

#endif  // THIRD_PARTY_LIBJINGLE_OVERRIDES_LOGGING_LOG_MESSAGE_DELEGATE_H_
