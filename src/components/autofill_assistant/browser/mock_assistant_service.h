// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_ASSISTANT_SERVICE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_ASSISTANT_SERVICE_H_

#include <string>

#include "components/autofill_assistant/browser/assistant_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockAssistantService : public AssistantService {
 public:
  MockAssistantService();
  ~MockAssistantService() override;

  void GetAssistantScriptsForUrl(const GURL& url,
                                 ResponseCallback callback) override {
    // Transforming callback into a references allows using RunOnceCallback on
    // the argument.
    OnGetAssistantScriptsForUrl(url, callback);
  }
  MOCK_METHOD2(OnGetAssistantScriptsForUrl,
               void(const GURL& url, ResponseCallback& callback));

  void GetAssistantActions(const std::string& script_path,
                           ResponseCallback callback) override {
    OnGetAssistantActions(script_path, callback);
  }
  MOCK_METHOD2(OnGetAssistantActions,
               void(const std::string& script_path,
                    ResponseCallback& callback));

  void GetNextAssistantActions(
      const std::string& previous_server_payload,
      const std::vector<ProcessedActionProto>& processed_actions,
      ResponseCallback callback) override {
    OnGetNextAssistantActions(previous_server_payload, processed_actions,
                              callback);
  }
  MOCK_METHOD3(OnGetNextAssistantActions,
               void(const std::string& previous_server_payload,
                    const std::vector<ProcessedActionProto>& processed_actions,
                    ResponseCallback& callback));
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_ASSISTANT_SERVICE_H_
