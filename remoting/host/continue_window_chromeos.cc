// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "remoting/host/continue_window.h"

#include "base/bind.h"
#include "base/macros.h"
#include "remoting/base/string_resources.h"
#include "remoting/host/chromeos/message_box.h"
#include "ui/base/l10n/l10n_util.h"

namespace remoting {

namespace {

class ContinueWindowAura : public ContinueWindow {
 public:
  ContinueWindowAura();
  ~ContinueWindowAura() override;

  void OnMessageBoxResult(MessageBox::Result result);

 protected:
  // ContinueWindow interface.
  void ShowUi() override;
  void HideUi() override;

 private:
  std::unique_ptr<MessageBox> message_box_;
  DISALLOW_COPY_AND_ASSIGN(ContinueWindowAura);
};

ContinueWindowAura::ContinueWindowAura() = default;

ContinueWindowAura::~ContinueWindowAura() = default;

void ContinueWindowAura::OnMessageBoxResult(MessageBox::Result result) {
  if (result == MessageBox::OK) {
    ContinueSession();
  } else {
    DisconnectSession();
  }
}

void ContinueWindowAura::ShowUi() {
  message_box_ = std::make_unique<MessageBox>(
      l10n_util::GetStringUTF16(IDS_MODE_IT2ME),           // title
      l10n_util::GetStringUTF16(IDS_CONTINUE_PROMPT),      // dialog label
      l10n_util::GetStringUTF16(IDS_CONTINUE_BUTTON),      // ok label
      l10n_util::GetStringUTF16(IDS_STOP_SHARING_BUTTON),  // cancel label
      base::Bind(&ContinueWindowAura::OnMessageBoxResult,
                 base::Unretained(this)));
}

void ContinueWindowAura::HideUi() {
  message_box_.reset();
}

}  // namespace

// static
std::unique_ptr<HostWindow> HostWindow::CreateContinueWindow() {
  return std::make_unique<ContinueWindowAura>();
}

}  // namespace remoting
