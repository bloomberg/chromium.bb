// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_USAGE_ENABLER_WEB_STATE_LIST_WEB_USAGE_ENABLER_H_
#define IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_USAGE_ENABLER_WEB_STATE_LIST_WEB_USAGE_ENABLER_H_

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

// A service that observes a WebStateList and enables or disables web usage for
// WebStates that are added or removed.  This can be used to easily enable or
// disable web usage for all the WebStates in a WebStateList.
class WebStateListWebUsageEnabler : public KeyedService {
 public:
  explicit WebStateListWebUsageEnabler() = default;

  // The WebStateList whose web usage are being updated.
  virtual void SetWebStateList(WebStateList* web_state_list) = 0;

  // Sets the WebUsageEnabled property for all WebStates in the list.  When new
  // WebStates are added to the list, their web usage will be set to the
  // |web_usage_enabled| as well.  Defaults to false.
  virtual bool IsWebUsageEnabled() const = 0;
  virtual void SetWebUsageEnabled(bool web_usage_enabled) = 0;

  // When TriggersInitialLoad() is true, the enabler will kick off the initial
  // load for WebStates added to the list if enabling their web usage.  Defaults
  // to true.
  virtual bool TriggersInitialLoad() const = 0;
  virtual void SetTriggersInitialLoad(bool triggers_initial_load) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebStateListWebUsageEnabler);
};

#endif  // IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_USAGE_ENABLER_WEB_STATE_LIST_WEB_USAGE_ENABLER_H_
