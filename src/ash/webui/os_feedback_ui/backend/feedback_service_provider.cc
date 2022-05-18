// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/os_feedback_ui/backend/feedback_service_provider.h"

#include <utility>

#include "ash/webui/os_feedback_ui/backend/os_feedback_delegate.h"
#include "ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom.h"
#include "base/bind.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "url/gurl.h"

namespace ash {
namespace feedback {

using ::ash::os_feedback_ui::mojom::FeedbackContext;
using ::ash::os_feedback_ui::mojom::FeedbackContextPtr;
using ::ash::os_feedback_ui::mojom::ReportPtr;
using ::ash::os_feedback_ui::mojom::SendReportStatus;

FeedbackServiceProvider::FeedbackServiceProvider(
    std::unique_ptr<OsFeedbackDelegate> feedback_delegate)
    : feedback_delegate_(std::move(feedback_delegate)) {}

FeedbackServiceProvider::~FeedbackServiceProvider() = default;

void FeedbackServiceProvider::GetFeedbackContext(
    GetFeedbackContextCallback callback) {
  FeedbackContextPtr feedback_context = FeedbackContext::New();
  feedback_context->page_url = feedback_delegate_->GetLastActivePageUrl();
  feedback_context->email = feedback_delegate_->GetSignedInUserEmail();

  std::move(callback).Run(std::move(feedback_context));
}

void FeedbackServiceProvider::GetScreenshotPng(
    GetScreenshotPngCallback callback) {
  feedback_delegate_->GetScreenshotPng(std::move(callback));
}

void FeedbackServiceProvider::SendReport(ReportPtr report,
                                         SendReportCallback callback) {
  feedback_delegate_->SendReport(std::move(report), std::move(callback));
}

void FeedbackServiceProvider::BindInterface(
    mojo::PendingReceiver<os_feedback_ui::mojom::FeedbackServiceProvider>
        receiver) {
  receiver_.Bind(std::move(receiver));
}

}  // namespace feedback
}  // namespace ash
