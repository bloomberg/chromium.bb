// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_CAST_TRACKER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_CAST_TRACKER_H_

#include <string>

#include "base/macros.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"

class DevToolsClient;
class Status;

// Tracks the state of Cast sinks and issues.
class CastTracker : public DevToolsEventListener {
 public:
  explicit CastTracker(DevToolsClient* client);
  ~CastTracker() override;

  // DevToolsEventListener:
  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::DictionaryValue& params) override;

  const base::Value& sinks() const { return sinks_; }
  const base::Value& issue() const { return issue_; }

 private:
  base::Value sinks_;
  base::Value issue_;

  DISALLOW_COPY_AND_ASSIGN(CastTracker);
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_CAST_TRACKER_H_
