// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_NETWORK_LOGS_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_NETWORK_LOGS_MESSAGE_HANDLER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

class NetworkLogsMessageHandler : public content::WebUIMessageHandler {
 public:
  NetworkLogsMessageHandler();
  ~NetworkLogsMessageHandler() override;
  NetworkLogsMessageHandler(const NetworkLogsMessageHandler&) = delete;
  NetworkLogsMessageHandler& operator=(const NetworkLogsMessageHandler&) =
      delete;

 private:
  // WebUIMessageHandler
  void RegisterMessages() override;

  void Respond(const std::string& callback_id,
               const std::string& result,
               bool is_error);
  void OnStoreLogs(const base::ListValue* list);
  void OnWriteSystemLogs(const std::string& callback_id,
                         base::Value&& options,
                         absl::optional<base::FilePath> syslogs_path);
  void MaybeWriteDebugLogs(const std::string& callback_id,
                           base::Value&& options);
  void OnWriteDebugLogs(const std::string& callback_id,
                        base::Value&& options,
                        absl::optional<base::FilePath> logs_path);
  void MaybeWritePolicies(const std::string& callback_id,
                          base::Value&& options);
  void OnWritePolicies(const std::string& callback_id, bool result);
  void OnWriteSystemLogsCompleted(const std::string& callback_id);
  void OnSetShillDebugging(const base::ListValue* list);
  void OnSetShillDebuggingCompleted(const std::string& callback_id,
                                    bool succeeded);

  base::FilePath out_dir_;
  base::WeakPtrFactory<NetworkLogsMessageHandler> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_NETWORK_LOGS_MESSAGE_HANDLER_H_
