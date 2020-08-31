// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/message_box.h"

#include <utility>

#include "base/macros.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace remoting {

// MessageBox::Core creates the dialog using the views::DialogWidget.  The
// DialogWidget is created by the caller but its lifetime is managed by the
// NativeWidget.  The DialogWidget communicates with the caller using the
//.DialogDelegateView interface, which must remain valid until DeleteDelegate()
// is called, at which the DialogDelegateView deletes itself.
//
// The Core class is introduced to abstract this awkward ownership model.  The
// Core and the MessageBox hold a raw references to each other, which is
// invalidated when either side are destroyed.
class MessageBox::Core : public views::DialogDelegateView {
 public:
  Core(const base::string16& title_label,
       const base::string16& message_label,
       const base::string16& ok_label,
       const base::string16& cancel_label,
       ResultCallback result_callback,
       MessageBox* message_box);

  // Mirrors the public MessageBox interface.
  void Show();
  void Hide();

  // views::DialogDelegateView:
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;
  views::View* GetContentsView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  void DeleteDelegate() override;

  // Called by MessageBox::Core when it is destroyed.
  void OnMessageBoxDestroyed();

 private:
  const base::string16 title_label_;
  ResultCallback result_callback_;
  MessageBox* message_box_;

  // Owned by the native widget hierarchy.
  views::MessageBoxView* message_box_view_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

MessageBox::Core::Core(const base::string16& title_label,
                       const base::string16& message_label,
                       const base::string16& ok_label,
                       const base::string16& cancel_label,
                       ResultCallback result_callback,
                       MessageBox* message_box)
    : title_label_(title_label),
      result_callback_(result_callback),
      message_box_(message_box),
      message_box_view_(new views::MessageBoxView(
          views::MessageBoxView::InitParams(message_label))) {
  DCHECK(message_box_);
  DialogDelegate::SetButtonLabel(ui::DIALOG_BUTTON_OK, ok_label);
  DialogDelegate::SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, cancel_label);

  auto run_callback = [](MessageBox::Core* core, Result result) {
    if (core->result_callback_)
      std::move(core->result_callback_).Run(result);
  };
  DialogDelegate::SetAcceptCallback(
      base::BindOnce(run_callback, base::Unretained(this), OK));
  DialogDelegate::SetCancelCallback(
      base::BindOnce(run_callback, base::Unretained(this), CANCEL));
  DialogDelegate::SetCloseCallback(
      base::BindOnce(run_callback, base::Unretained(this), CANCEL));
}

void MessageBox::Core::Show() {
  // The widget is owned by the NativeWidget.  See  comments in widget.h.
  views::Widget* widget =
      CreateDialogWidget(this, /* delegate */
                         nullptr /* parent window*/,
                         nullptr /* parent view */);

  if (widget) {
    widget->Show();
  }
}

void MessageBox::Core::Hide() {
  if (GetWidget()) {
    GetWidget()->Close();
  }
}

ui::ModalType MessageBox::Core::GetModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

base::string16 MessageBox::Core::GetWindowTitle() const {
  return title_label_;
}

views::View* MessageBox::Core::GetContentsView() {
  return message_box_view_;
}

views::Widget* MessageBox::Core::GetWidget() {
  return message_box_view_->GetWidget();
}

const views::Widget* MessageBox::Core::GetWidget() const {
  return message_box_view_->GetWidget();
}

void MessageBox::Core::DeleteDelegate() {
  if (message_box_) {
    message_box_->core_ = nullptr;
  }
  delete this;
}

void MessageBox::Core::OnMessageBoxDestroyed() {
  DCHECK(message_box_);
  message_box_ = nullptr;
  // The callback should not be invoked after MessageBox is destroyed.
  result_callback_.Reset();
}

MessageBox::MessageBox(const base::string16& title_label,
                       const base::string16& message_label,
                       const base::string16& ok_label,
                       const base::string16& cancel_label,
                       ResultCallback result_callback)
    : core_(new Core(title_label,
                     message_label,
                     ok_label,
                     cancel_label,
                     result_callback,
                     this)) {
  core_->Show();
}

MessageBox::~MessageBox() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (core_) {
    core_->OnMessageBoxDestroyed();
    core_->Hide();
    core_ = nullptr;
  }
}

}  // namespace remoting
