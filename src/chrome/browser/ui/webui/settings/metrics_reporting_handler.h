// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_METRICS_REPORTING_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_METRICS_REPORTING_HANDLER_H_

#if defined(GOOGLE_CHROME_BUILD) && !defined(OS_CHROMEOS)

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/policy/core/common/policy_service.h"
#include "components/prefs/pref_member.h"

namespace base {
class DictionaryValue;
}

namespace settings {

class MetricsReportingHandler : public SettingsPageUIHandler {
 public:
  MetricsReportingHandler();
  ~MetricsReportingHandler() override;

  // SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 protected:
  // Handler for "getMetricsReporting" message. No arguments. Protected for
  // testing.
  void HandleGetMetricsReporting(const base::ListValue* args);

 private:
  // Describes the state of metrics reporting in a base::DictionaryValue.
  // Friends with ChromeMetricsServiceAccessor.
  std::unique_ptr<base::DictionaryValue> CreateMetricsReportingDict();

  // Handler for "setMetricsReportingEnabled" message. Passed a single,
  // |enabled| boolean argument.
  void HandleSetMetricsReportingEnabled(const base::ListValue* args);

  // Called when the policies that affect whether metrics reporting is managed
  // change.
  void OnPolicyChanged(const base::Value* current_policy,
                       const base::Value* previous_policy);

  // Called when the local state pref controlling metrics reporting changes.
  void OnPrefChanged(const std::string& pref_name);

  // Sends a "metrics-reporting-change" WebUI listener event to the page.
  void SendMetricsReportingChange();

  // Used to track pref changes that affect whether metrics reporting is
  // enabled.
  std::unique_ptr<BooleanPrefMember> pref_member_;

  // Used to track policy changes that affect whether metrics reporting is
  // enabled or managed.
  std::unique_ptr<policy::PolicyChangeRegistrar> policy_registrar_;

  DISALLOW_COPY_AND_ASSIGN(MetricsReportingHandler);
};

}  // namespace settings

#endif  // defined(GOOGLE_CHROME_BUILD) && !defined(OS_CHROMEOS)

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_METRICS_REPORTING_HANDLER_H_
