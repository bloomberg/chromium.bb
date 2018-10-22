// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_PROTOCOL_UTILS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_PROTOCOL_UTILS_H_

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/assistant_script.h"
#include "components/autofill_assistant/browser/service.pb.h"

class GURL;

namespace autofill_assistant {
// Autofill assistant protocol related convenient utils.
class AssistantProtocolUtils {
 public:
  // Create getting autofill assistant scripts request for the given
  // |url|.
  static std::string CreateGetScriptsRequest(const GURL& url);

  using AssistantScripts =
      std::map<AssistantScript*, std::unique_ptr<AssistantScript>>;
  // Parse assistant scripts from the given |response|, which should not be an
  // empty string.
  // Parsed assistant scripts are returned through |assistant_scripts|, which
  // should not be nullptr. Return false if parse failed, otherwise return true.
  static bool ParseAssistantScripts(
      const std::string& response,
      std::map<AssistantScript*, std::unique_ptr<AssistantScript>>*
          assistant_scripts);

  // Create initial request to get script actions for the given |script_path|.
  static std::string CreateInitialScriptActionsRequest(
      const std::string& script_path);

  // Create request to get next sequence of actions for a script.
  static std::string CreateNextScriptActionsRequest(
      const std::string& previous_server_payload,
      const std::vector<ProcessedActionProto>& processed_actions);

  // Parse actions from the given |response|, which should not be an empty
  // string.
  // Pass in nullptr for |return_server_payload| to indicate no need to return
  // server payload. Parsed actions are returned through |actions|, which should
  // not be nullptr. Return false if parse failed, otherwise return true.
  static bool ParseActions(const std::string& response,
                           std::string* return_server_payload,
                           std::deque<std::unique_ptr<Action>>* actions);

 private:
  // To avoid instantiate this class by accident.
  AssistantProtocolUtils() = delete;
  ~AssistantProtocolUtils() = delete;
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_PROTOCOL_UTILS_H_
