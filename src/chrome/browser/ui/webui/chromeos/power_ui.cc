// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/power_ui.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/power/power_data_collector.h"
#include "chrome/browser/ash/power/process_data_collector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {

namespace {

const char kRequestBatteryChargeDataCallback[] = "requestBatteryChargeData";
const char kRequestCpuIdleDataCallback[] = "requestCpuIdleData";
const char kRequestCpuFreqDataCallback[] = "requestCpuFreqData";
const char kRequestProcessUsageDataCallback[] = "requestProcessUsageData";

class PowerMessageHandler : public content::WebUIMessageHandler {
 public:
  PowerMessageHandler();
  ~PowerMessageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  void OnGetBatteryChargeData(const base::ListValue* value);
  void OnGetCpuIdleData(const base::ListValue* value);
  void OnGetCpuFreqData(const base::ListValue* value);
  void OnGetProcessUsageData(const base::ListValue* value);
  void GetJsStateOccupancyData(
      const std::vector<CpuDataCollector::StateOccupancySampleDeque>& data,
      const std::vector<std::string>& state_names,
      base::ListValue* js_data);
  void GetJsSystemResumedData(base::ListValue* value);
};

PowerMessageHandler::PowerMessageHandler() {
}

PowerMessageHandler::~PowerMessageHandler() {
}

void PowerMessageHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      kRequestBatteryChargeDataCallback,
      base::BindRepeating(&PowerMessageHandler::OnGetBatteryChargeData,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      kRequestCpuIdleDataCallback,
      base::BindRepeating(&PowerMessageHandler::OnGetCpuIdleData,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      kRequestCpuFreqDataCallback,
      base::BindRepeating(&PowerMessageHandler::OnGetCpuFreqData,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      kRequestProcessUsageDataCallback,
      base::BindRepeating(&PowerMessageHandler::OnGetProcessUsageData,
                          base::Unretained(this)));
}

void PowerMessageHandler::OnGetBatteryChargeData(const base::ListValue* args) {
  AllowJavascript();

  const base::circular_deque<PowerDataCollector::PowerSupplySample>&
      power_supply = PowerDataCollector::Get()->power_supply_data();
  base::ListValue js_power_supply_data;
  for (size_t i = 0; i < power_supply.size(); ++i) {
    const PowerDataCollector::PowerSupplySample& sample = power_supply[i];
    std::unique_ptr<base::DictionaryValue> element(new base::DictionaryValue);
    element->SetDoubleKey("batteryPercent", sample.battery_percent);
    element->SetDoubleKey("batteryDischargeRate",
                          sample.battery_discharge_rate);
    element->SetBoolean("externalPower", sample.external_power);
    element->SetDoubleKey("time", sample.time.ToJsTime());

    js_power_supply_data.Append(std::move(element));
  }

  base::ListValue js_system_resumed_data;
  GetJsSystemResumedData(&js_system_resumed_data);

  base::DictionaryValue data;
  data.SetKey("powerSupplyData", std::move(js_power_supply_data));
  data.SetKey("systemResumedData", std::move(js_system_resumed_data));
  const base::Value& callback_id = args->GetList()[0];
  ResolveJavascriptCallback(callback_id, data);
}

void PowerMessageHandler::OnGetCpuIdleData(const base::ListValue* args) {
  AllowJavascript();

  const CpuDataCollector& cpu_data_collector =
      PowerDataCollector::Get()->cpu_data_collector();

  const std::vector<CpuDataCollector::StateOccupancySampleDeque>& idle_data =
      cpu_data_collector.cpu_idle_state_data();
  const std::vector<std::string>& idle_state_names =
      cpu_data_collector.cpu_idle_state_names();
  base::ListValue js_idle_data;
  GetJsStateOccupancyData(idle_data, idle_state_names, &js_idle_data);

  base::ListValue js_system_resumed_data;
  GetJsSystemResumedData(&js_system_resumed_data);

  base::DictionaryValue data;
  data.SetKey("idleStateData", std::move(js_idle_data));
  data.SetKey("systemResumedData", std::move(js_system_resumed_data));
  const base::Value& callback_id = args->GetList()[0];
  ResolveJavascriptCallback(callback_id, data);
}

void PowerMessageHandler::OnGetCpuFreqData(const base::ListValue* args) {
  AllowJavascript();

  const CpuDataCollector& cpu_data_collector =
      PowerDataCollector::Get()->cpu_data_collector();

  const std::vector<CpuDataCollector::StateOccupancySampleDeque>& freq_data =
      cpu_data_collector.cpu_freq_state_data();
  const std::vector<std::string>& freq_state_names =
      cpu_data_collector.cpu_freq_state_names();
  base::ListValue js_freq_data;
  GetJsStateOccupancyData(freq_data, freq_state_names, &js_freq_data);

  base::ListValue js_system_resumed_data;
  GetJsSystemResumedData(&js_system_resumed_data);

  base::DictionaryValue data;
  data.SetKey("freqStateData", std::move(js_freq_data));
  data.SetKey("systemResumedData", std::move(js_system_resumed_data));
  const base::Value& callback_id = args->GetList()[0];
  ResolveJavascriptCallback(callback_id, data);
}

void PowerMessageHandler::OnGetProcessUsageData(const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(1U, args->GetList().size());

  const base::Value& callback_id = args->GetList()[0];

  const std::vector<ProcessDataCollector::ProcessUsageData>& process_list =
      ProcessDataCollector::Get()->GetProcessUsages();

  base::ListValue js_process_usages;
  for (const auto& process_info : process_list) {
    std::unique_ptr<base::DictionaryValue> element =
        std::make_unique<base::DictionaryValue>();
    element->SetInteger("pid", process_info.process_data.pid);
    element->SetString("name", process_info.process_data.name);
    element->SetString("cmdline", process_info.process_data.cmdline);
    element->SetInteger("type",
                        static_cast<int>(process_info.process_data.type));
    element->SetDoubleKey("powerUsageFraction",
                          process_info.power_usage_fraction);
    js_process_usages.Append(std::move(element));
  }

  ResolveJavascriptCallback(callback_id, js_process_usages);
}

void PowerMessageHandler::GetJsSystemResumedData(base::ListValue *data) {
  DCHECK(data);

  const base::circular_deque<PowerDataCollector::SystemResumedSample>&
      system_resumed = PowerDataCollector::Get()->system_resumed_data();
  for (size_t i = 0; i < system_resumed.size(); ++i) {
    const PowerDataCollector::SystemResumedSample& sample = system_resumed[i];
    std::unique_ptr<base::DictionaryValue> element(new base::DictionaryValue);
    element->SetDoubleKey("sleepDuration",
                          sample.sleep_duration.InMillisecondsF());
    element->SetDoubleKey("time", sample.time.ToJsTime());

    data->Append(std::move(element));
  }
}

void PowerMessageHandler::GetJsStateOccupancyData(
    const std::vector<CpuDataCollector::StateOccupancySampleDeque>& data,
    const std::vector<std::string>& state_names,
    base::ListValue *js_data) {
  for (unsigned int cpu = 0; cpu < data.size(); ++cpu) {
    const CpuDataCollector::StateOccupancySampleDeque& sample_deque = data[cpu];
    std::unique_ptr<base::ListValue> js_sample_list(new base::ListValue);
    for (unsigned int i = 0; i < sample_deque.size(); ++i) {
      const CpuDataCollector::StateOccupancySample& sample = sample_deque[i];
      std::unique_ptr<base::DictionaryValue> js_sample(
          new base::DictionaryValue);
      js_sample->SetDoubleKey("time", sample.time.ToJsTime());
      js_sample->SetBoolean("cpuOnline", sample.cpu_online);

      base::DictionaryValue state_dict;
      for (size_t index = 0; index < sample.time_in_state.size(); ++index) {
        state_dict.SetDoubleKey(state_names[index],
                                sample.time_in_state[index].InMillisecondsF());
      }
      js_sample->SetKey("timeInState", std::move(state_dict));

      js_sample_list->Append(std::move(js_sample));
    }
    js_data->Append(std::move(js_sample_list));
  }
}

}  // namespace

PowerUI::PowerUI(content::WebUI* web_ui) : content::WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<PowerMessageHandler>());

  content::WebUIDataSource* html =
      content::WebUIDataSource::Create(chrome::kChromeUIPowerHost);

  static constexpr webui::LocalizedString kStrings[] = {
      {"titleText", IDS_ABOUT_POWER_TITLE},
      {"showButton", IDS_ABOUT_POWER_SHOW_BUTTON},
      {"hideButton", IDS_ABOUT_POWER_HIDE_BUTTON},
      {"reloadButton", IDS_ABOUT_POWER_RELOAD_BUTTON},
      {"notEnoughDataAvailableYet", IDS_ABOUT_POWER_NOT_ENOUGH_DATA},
      {"systemSuspended", IDS_ABOUT_POWER_SYSTEM_SUSPENDED},
      {"invalidData", IDS_ABOUT_POWER_INVALID},
      {"offlineText", IDS_ABOUT_POWER_OFFLINE},
      {"batteryChargeSectionTitle",
       IDS_ABOUT_POWER_BATTERY_CHARGE_SECTION_TITLE},
      {"batteryChargePercentageHeader",
       IDS_ABOUT_POWER_BATTERY_CHARGE_PERCENTAGE_HEADER},
      {"batteryDischargeRateHeader",
       IDS_ABOUT_POWER_BATTERY_DISCHARGE_RATE_HEADER},
      {"dischargeRateLegendText", IDS_ABOUT_POWER_DISCHARGE_RATE_LEGEND_TEXT},
      {"movingAverageLegendText", IDS_ABOUT_POWER_MOVING_AVERAGE_LEGEND_TEXT},
      {"binnedAverageLegendText", IDS_ABOUT_POWER_BINNED_AVERAGE_LEGEND_TEXT},
      {"averageOverText", IDS_ABOUT_POWER_AVERAGE_OVER_TEXT},
      {"samplesText", IDS_ABOUT_POWER_AVERAGE_SAMPLES_TEXT},
      {"cpuIdleSectionTitle", IDS_ABOUT_POWER_CPU_IDLE_SECTION_TITLE},
      {"idleStateOccupancyPercentageHeader",
       IDS_ABOUT_POWER_CPU_IDLE_STATE_OCCUPANCY_PERCENTAGE},
      {"cpuFreqSectionTitle", IDS_ABOUT_POWER_CPU_FREQ_SECTION_TITLE},
      {"frequencyStateOccupancyPercentageHeader",
       IDS_ABOUT_POWER_CPU_FREQ_STATE_OCCUPANCY_PERCENTAGE},
  };
  html->AddLocalizedStrings(kStrings);

  html->UseStringsJs();

  html->AddResourcePath("power.css", IDR_ABOUT_POWER_CSS);
  html->AddResourcePath("power.js", IDR_ABOUT_POWER_JS);
  html->SetDefaultResource(IDR_ABOUT_POWER_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, html);
}

PowerUI::~PowerUI() {
}

}  // namespace chromeos
