// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/metrics/histograms_internals_ui.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/values.h"
#include "content/browser/metrics/histogram_synchronizer.h"
#include "content/browser/metrics/histograms_monitor.h"
#include "content/grit/content_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"

namespace content {
namespace {

const char kHistogramsUIJs[] = "histograms_internals.js";
const char kHistogramsUICss[] = "histograms_internals.css";
const char kHistogramsUIRequestHistograms[] = "requestHistograms";
const char kHistogramsUIStartMonitoring[] = "startMonitoring";
const char kHistogramsUIFetchDiff[] = "fetchDiff";

// Stores the information received from Javascript side.
struct JsParams {
  std::string callback_id;
  std::string query;
};

WebUIDataSource* CreateHistogramsHTMLSource() {
  WebUIDataSource* source = WebUIDataSource::Create(kChromeUIHistogramHost);

  source->AddResourcePath(kHistogramsUIJs, IDR_HISTOGRAMS_INTERNALS_JS);
  source->AddResourcePath(kHistogramsUICss, IDR_HISTOGRAMS_INTERNALS_CSS);
  source->SetDefaultResource(IDR_HISTOGRAMS_INTERNALS_HTML);
  return source;
}

// This class receives javascript messages from the renderer.
// Note that the WebUI infrastructure runs on the UI thread, therefore all of
// this class's methods are expected to run on the UI thread.
class HistogramsMessageHandler : public WebUIMessageHandler {
 public:
  HistogramsMessageHandler();

  HistogramsMessageHandler(const HistogramsMessageHandler&) = delete;
  HistogramsMessageHandler& operator=(const HistogramsMessageHandler&) = delete;

  ~HistogramsMessageHandler() override;

  // WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  void HandleRequestHistograms(const base::ListValue* args);
  void HandleStartMoninoring(const base::ListValue* args);
  void HandleFetchDiff(const base::ListValue* args);

  // Calls AllowJavascript() and unpacks the passed params.
  JsParams AllowJavascriptAndUnpackParams(const base::ListValue& args);

  HistogramsMonitor histogram_monitor_;
};

HistogramsMessageHandler::HistogramsMessageHandler() {}

HistogramsMessageHandler::~HistogramsMessageHandler() {}

JsParams HistogramsMessageHandler::AllowJavascriptAndUnpackParams(
    const base::ListValue& args) {
  base::Value::ConstListView args_list = args.GetList();
  AllowJavascript();
  JsParams params;
  if (args_list.size() > 0u && args_list[0].is_string())
    params.callback_id = args_list[0].GetString();
  if (args_list.size() > 1u && args_list[1].is_string())
    params.query = args_list[1].GetString();
  return params;
}

void HistogramsMessageHandler::HandleRequestHistograms(
    const base::ListValue* args) {
  base::StatisticsRecorder::ImportProvidedHistograms();
  HistogramSynchronizer::FetchHistograms();
  JsParams params = AllowJavascriptAndUnpackParams(*args);
  base::ListValue histograms_list;
  for (base::HistogramBase* histogram :
       base::StatisticsRecorder::Sort(base::StatisticsRecorder::WithName(
           base::StatisticsRecorder::GetHistograms(), params.query))) {
    base::Value histogram_dict = histogram->ToGraphDict();
    if (!histogram_dict.DictEmpty())
      histograms_list.Append(std::move(histogram_dict));
  }

  ResolveJavascriptCallback(base::Value(params.callback_id),
                            std::move(histograms_list));
}

void HistogramsMessageHandler::HandleStartMoninoring(
    const base::ListValue* args) {
  JsParams params = AllowJavascriptAndUnpackParams(*args);
  histogram_monitor_.StartMonitoring(params.query);
  ResolveJavascriptCallback(base::Value(params.callback_id),
                            base::Value("Success"));
}

void HistogramsMessageHandler::HandleFetchDiff(const base::ListValue* args) {
  JsParams params = AllowJavascriptAndUnpackParams(*args);
  base::ListValue histograms_list = histogram_monitor_.GetDiff();
  ResolveJavascriptCallback(base::Value(params.callback_id),
                            std::move(histograms_list));
}

void HistogramsMessageHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // We can use base::Unretained() here, as both the callback and this class are
  // owned by HistogramsInternalsUI.
  web_ui()->RegisterDeprecatedMessageCallback(
      kHistogramsUIRequestHistograms,
      base::BindRepeating(&HistogramsMessageHandler::HandleRequestHistograms,
                          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      kHistogramsUIStartMonitoring,
      base::BindRepeating(&HistogramsMessageHandler::HandleStartMoninoring,
                          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      kHistogramsUIFetchDiff,
      base::BindRepeating(&HistogramsMessageHandler::HandleFetchDiff,
                          base::Unretained(this)));
}

}  // namespace

HistogramsInternalsUI::HistogramsInternalsUI(WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<HistogramsMessageHandler>());

  // Set up the chrome://histograms/ source.
  BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  WebUIDataSource::Add(browser_context, CreateHistogramsHTMLSource());
}

}  // namespace content
