// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/continue_window.h"

#include <gtk/gtk.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "remoting/host/ui_strings.h"
#include "ui/base/gtk/gtk_signal.h"

namespace remoting {

class ContinueWindowGtk : public remoting::ContinueWindow {
 public:
  explicit ContinueWindowGtk(const UiStrings* ui_strings);
  virtual ~ContinueWindowGtk();

  virtual void Show(const ContinueSessionCallback& callback) OVERRIDE;
  virtual void Hide() OVERRIDE;

 private:
  CHROMEGTK_CALLBACK_1(ContinueWindowGtk, void, OnResponse, int);

  void CreateWindow();

  ContinueSessionCallback callback_;
  GtkWidget* continue_window_;

  // Points to the localized strings.
  const UiStrings* ui_strings_;

  DISALLOW_COPY_AND_ASSIGN(ContinueWindowGtk);
};

ContinueWindowGtk::ContinueWindowGtk(const UiStrings* ui_strings)
    : continue_window_(NULL),
      ui_strings_(ui_strings) {
}

ContinueWindowGtk::~ContinueWindowGtk() {
}

void ContinueWindowGtk::CreateWindow() {
  if (continue_window_)
    return;

  continue_window_ = gtk_dialog_new_with_buttons(
      UTF16ToUTF8(ui_strings_->product_name).c_str(),
      NULL,
      static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      UTF16ToUTF8(ui_strings_->stop_sharing_button_text).c_str(),
      GTK_RESPONSE_CANCEL,
      UTF16ToUTF8(ui_strings_->continue_button_text).c_str(),
      GTK_RESPONSE_OK,
      NULL);

  gtk_dialog_set_default_response(GTK_DIALOG(continue_window_),
                                  GTK_RESPONSE_OK);
  gtk_window_set_resizable(GTK_WINDOW(continue_window_), FALSE);

  // Set always-on-top, otherwise this window tends to be obscured by the
  // DisconnectWindow.
  gtk_window_set_keep_above(GTK_WINDOW(continue_window_), TRUE);

  g_signal_connect(continue_window_, "response",
                   G_CALLBACK(OnResponseThunk), this);

  GtkWidget* content_area =
      gtk_dialog_get_content_area(GTK_DIALOG(continue_window_));

  GtkWidget* text_label =
      gtk_label_new(UTF16ToUTF8(ui_strings_->continue_prompt).c_str());
  gtk_label_set_line_wrap(GTK_LABEL(text_label), TRUE);
  // TODO(lambroslambrou): Fix magic numbers, as in disconnect_window_gtk.cc.
  gtk_misc_set_padding(GTK_MISC(text_label), 12, 12);
  gtk_container_add(GTK_CONTAINER(content_area), text_label);

  gtk_widget_show_all(content_area);
}

void ContinueWindowGtk::Show(const ContinueSessionCallback& callback) {
  callback_ = callback;
  CreateWindow();
  gtk_window_set_urgency_hint(GTK_WINDOW(continue_window_), TRUE);
  gtk_window_present(GTK_WINDOW(continue_window_));
}

void ContinueWindowGtk::Hide() {
  if (continue_window_) {
    gtk_widget_destroy(continue_window_);
    continue_window_ = NULL;
  }
}

void ContinueWindowGtk::OnResponse(GtkWidget* dialog, int response_id) {
  callback_.Run(response_id == GTK_RESPONSE_OK);
  Hide();
}

scoped_ptr<ContinueWindow> ContinueWindow::Create(const UiStrings* ui_strings) {
  return scoped_ptr<ContinueWindow>(new ContinueWindowGtk(ui_strings));
}

}  // namespace remoting
