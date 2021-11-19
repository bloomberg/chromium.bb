// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PROTOCOL_UTILS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PROTOCOL_UTILS_H_

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_parameters.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/trigger_scripts/trigger_script.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace autofill_assistant {
// Autofill assistant protocol related convenient utils.
class ProtocolUtils {
 public:
  // Create getting autofill assistant scripts request for the given
  // |url|.
  static std::string CreateGetScriptsRequest(
      const GURL& url,
      const ClientContextProto& client_context,
      const ScriptParameters& script_parameters);

  // Convert |script_proto| to a script struct and if the script is valid, add
  // it to |scripts|.
  static void AddScript(const SupportedScriptProto& script_proto,
                        std::vector<std::unique_ptr<Script>>* scripts);

  // Create initial request to get script actions for the given |script_path|.
  //
  // TODO(b/806868): Remove the script payload from initial requests once the
  // server has transitioned to global payloads.
  static std::string CreateInitialScriptActionsRequest(
      const std::string& script_path,
      const GURL& url,
      const std::string& global_payload,
      const std::string& script_payload,
      const ClientContextProto& client_context,
      const ScriptParameters& script_parameters,
      const absl::optional<ScriptStoreConfig>& script_store_config);

  // Create request to get next sequence of actions for a script.
  static std::string CreateNextScriptActionsRequest(
      const std::string& global_payload,
      const std::string& script_payload,
      const std::vector<ProcessedActionProto>& processed_actions,
      const RoundtripTimingStats& timing_stats,
      const ClientContextProto& client_context);

  // Create request to get the available trigger scripts for |url|.
  static std::string CreateGetTriggerScriptsRequest(
      const GURL& url,
      const ClientContextProto& client_context,
      const ScriptParameters& script_parameters);

  // Create an action from the |action|.
  static std::unique_ptr<Action> CreateAction(ActionDelegate* delegate,
                                              const ActionProto& action);

  // Parse actions from the given |response|, which can be an empty string.
  //
  // Pass in nullptr for |return_global_payload| or |return_script_payload| to
  // indicate no need to return that payload. Parsed actions are returned
  // through |actions|, which should not be nullptr. Optionally, parsed scripts
  // are returned through |scripts| and used to update the list of cached
  // scripts. The bool |should_update_scripts| makes clear the destinction
  // between an empty list of |scripts| or the scripts field not even set in the
  // proto. Return false if parse failed, otherwise return true.
  static bool ParseActions(ActionDelegate* delegate,
                           const std::string& response,
                           std::string* return_global_payload,
                           std::string* return_script_payload,
                           std::vector<std::unique_ptr<Action>>* actions,
                           std::vector<std::unique_ptr<Script>>* scripts,
                           bool* should_update_scripts);

  // Parse trigger scripts from the given |response| and insert them into
  // |trigger_scripts|. Returns false if parsing failed or the proto contained
  // invalid values.
  static bool ParseTriggerScripts(
      const std::string& response,
      std::vector<std::unique_ptr<TriggerScript>>* trigger_scripts,
      std::vector<std::string>* additional_allowed_domains,
      int* trigger_condition_check_interval_ms,
      absl::optional<int>* trigger_condition_timeout_ms,
      absl::optional<std::unique_ptr<ScriptParameters>>* script_parameters);

 private:
  // Checks that the |trigger_condition| is well-formed (e.g. does not contain
  // regexes that cannot be compiled).
  static bool ValidateTriggerCondition(
      const TriggerScriptConditionProto& trigger_condition);
  FRIEND_TEST_ALL_PREFIXES(ProtocolUtilsTest,
                           ValidateTriggerConditionsSimpleConditions);
  FRIEND_TEST_ALL_PREFIXES(ProtocolUtilsTest,
                           ValidateTriggerConditionsComplexConditions);

  // To avoid instantiate this class by accident.
  ProtocolUtils() = delete;
  ~ProtocolUtils() = delete;
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PROTOCOL_UTILS_H_
