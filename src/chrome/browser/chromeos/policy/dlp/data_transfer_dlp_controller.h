// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DATA_TRANSFER_DLP_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DATA_TRANSFER_DLP_CONTROLLER_H_

#include "base/callback.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_clipboard_notifier.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_drag_drop_notifier.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"

namespace ui {
class DataTransferEndpoint;
}

namespace policy {

// DataTransferDlpController is responsible for preventing leaks of confidential
// data through clipboard data read or drag-and-drop by controlling read
// operations according to the rules of the Data leak prevention policy set by
// the admin.
class DataTransferDlpController : public ui::DataTransferPolicyController {
 public:
  // Creates an instance of the class.
  // Indicates that restricting clipboard content and drag-n-drop is required.
  // It's guaranteed that `dlp_rules_manager` controls the lifetime of
  // DataTransferDlpController and outlives it.
  static void Init(const DlpRulesManager& dlp_rules_manager);

  DataTransferDlpController(const DataTransferDlpController&) = delete;
  void operator=(const DataTransferDlpController&) = delete;

  // ui::DataTransferPolicyController:
  bool IsClipboardReadAllowed(const ui::DataTransferEndpoint* const data_src,
                              const ui::DataTransferEndpoint* const data_dst,
                              const absl::optional<size_t> size) override;
  void PasteIfAllowed(const ui::DataTransferEndpoint* const data_src,
                      const ui::DataTransferEndpoint* const data_dst,
                      const absl::optional<size_t> size,
                      content::RenderFrameHost* rfh,
                      base::OnceCallback<void(bool)> callback) override;
  void DropIfAllowed(const ui::DataTransferEndpoint* data_src,
                     const ui::DataTransferEndpoint* data_dst,
                     base::OnceClosure drop_cb) override;

 protected:
  explicit DataTransferDlpController(const DlpRulesManager& dlp_rules_manager);
  ~DataTransferDlpController() override;

 private:
  virtual void NotifyBlockedPaste(
      const ui::DataTransferEndpoint* const data_src,
      const ui::DataTransferEndpoint* const data_dst);

  virtual void WarnOnPaste(const ui::DataTransferEndpoint* const data_src,
                           const ui::DataTransferEndpoint* const data_dst);

  virtual void WarnOnBlinkPaste(const ui::DataTransferEndpoint* const data_src,
                                const ui::DataTransferEndpoint* const data_dst,
                                content::WebContents* web_contents,
                                base::OnceCallback<void(bool)> paste_cb);

  virtual bool ShouldPasteOnWarn(
      const ui::DataTransferEndpoint* const data_dst);

  virtual bool ShouldCancelOnWarn(
      const ui::DataTransferEndpoint* const data_dst);

  virtual void NotifyBlockedDrop(
      const ui::DataTransferEndpoint* const data_src,
      const ui::DataTransferEndpoint* const data_dst);

  virtual void WarnOnDrop(const ui::DataTransferEndpoint* const data_src,
                          const ui::DataTransferEndpoint* const data_dst,
                          base::OnceClosure drop_cb);

  bool ShouldSkipReporting(const ui::DataTransferEndpoint* const data_src,
                           const ui::DataTransferEndpoint* const data_dst,
                           base::TimeTicks curr_time);

  void ReportEvent(const ui::DataTransferEndpoint* const data_src,
                   const ui::DataTransferEndpoint* const data_dst,
                   const std::string& src_pattern,
                   const std::string& dst_pattern,
                   DlpRulesManager::Level level,
                   bool is_clipboard_event);

  void MaybeReportEvent(const ui::DataTransferEndpoint* const data_src,
                        const ui::DataTransferEndpoint* const data_dst,
                        const std::string& src_pattern,
                        const std::string& dst_pattern,
                        DlpRulesManager::Level level,
                        bool is_clipboard_event);

  // The solution for the issue of sending multiple reporting events for a
  // single user action. When a user triggers a paste (for instance by pressing
  // ctrl+V) clipboard API receives multiple mojo calls. For each call we check
  // if restricted data is being accessed. However, there is no way to identify
  // if those API calls come from the same user action or not. So after
  // reporting one event, we skip reporting for a short time.
  struct LastReportedEndpoints {
    LastReportedEndpoints();
    ~LastReportedEndpoints();
    absl::optional<ui::DataTransferEndpoint> data_src;
    absl::optional<ui::DataTransferEndpoint> data_dst;
    base::TimeTicks time;
  } last_reported_;

  const DlpRulesManager& dlp_rules_manager_;
  DlpClipboardNotifier clipboard_notifier_;
  DlpDragDropNotifier drag_drop_notifier_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DATA_TRANSFER_DLP_CONTROLLER_H_
