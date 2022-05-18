// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_OS_FEEDBACK_DELEGATE_H_
#define ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_OS_FEEDBACK_DELEGATE_H_

#include <string>

#include "ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace ash {

using GetScreenshotPngCallback =
    base::OnceCallback<void(const std::vector<uint8_t>&)>;
using SendReportCallback =
    base::OnceCallback<void(os_feedback_ui::mojom::SendReportStatus)>;

// A delegate which exposes browser functionality from //chrome to the OS
// Feedback UI.
class OsFeedbackDelegate {
 public:
  virtual ~OsFeedbackDelegate() = default;

  // Gets the application locale so that suggested help contents can display
  // localized titles when available.
  virtual std::string GetApplicationLocale() = 0;
  // Returns the last active page url before the feedback tool is opened if any.
  virtual absl::optional<GURL> GetLastActivePageUrl() = 0;
  // Returns the normalized email address of the signed-in user associated with
  // the browser context, if any.
  virtual absl::optional<std::string> GetSignedInUserEmail() const = 0;
  // Return the screenshot of the primary display in PNG format. It was taken
  // right before the feedback tool is launched.
  virtual void GetScreenshotPng(GetScreenshotPngCallback callback) = 0;
  // Collect data and send the report to Google.
  virtual void SendReport(os_feedback_ui::mojom::ReportPtr report,
                          SendReportCallback callback) = 0;
};

}  // namespace ash

#endif  // ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_OS_FEEDBACK_DELEGATE_H_
