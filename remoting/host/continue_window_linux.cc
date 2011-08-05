// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/continue_window.h"

#include <gtk/gtk.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "remoting/host/chromoting_host.h"
#include "ui/base/gtk/gtk_signal.h"

namespace remoting {

class ContinueWindowLinux : public remoting::ContinueWindow {
 public:
  ContinueWindowLinux();
  virtual ~ContinueWindowLinux();

  virtual void Show(remoting::ChromotingHost* host) OVERRIDE;
  virtual void Hide() OVERRIDE;

 private:
  CHROMEGTK_CALLBACK_1(ContinueWindowLinux, void, OnResponse, int);

  void CreateWindow();

  ChromotingHost* host_;
  GtkWidget* continue_window_;

  DISALLOW_COPY_AND_ASSIGN(ContinueWindowLinux);
};

ContinueWindowLinux::ContinueWindowLinux()
    : host_(NULL),
      continue_window_(NULL) {
}

ContinueWindowLinux::~ContinueWindowLinux() {
}

void ContinueWindowLinux::CreateWindow() {
  if (continue_window_) return;

  continue_window_ = gtk_dialog_new_with_buttons(
      kTitle,
      NULL,
      static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      kCancelButtonText, GTK_RESPONSE_CANCEL,
      kDefaultButtonText, GTK_RESPONSE_OK,
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

  GtkWidget* text_label = gtk_label_new(kMessage);
  gtk_label_set_line_wrap(GTK_LABEL(text_label), TRUE);
  // TODO(lambroslambrou): Fix magic numbers, as in disconnect_window_linux.cc.
  gtk_misc_set_padding(GTK_MISC(text_label), 12, 12);
  gtk_container_add(GTK_CONTAINER(content_area), text_label);

  gtk_widget_show_all(content_area);
}

void ContinueWindowLinux::Show(remoting::ChromotingHost* host) {
  host_ = host;
  CreateWindow();
  gtk_window_set_urgency_hint(GTK_WINDOW(continue_window_), TRUE);
  gtk_window_present(GTK_WINDOW(continue_window_));
}

void ContinueWindowLinux::Hide() {
  if (continue_window_) {
    gtk_widget_destroy(continue_window_);
    continue_window_ = NULL;
  }
}

void ContinueWindowLinux::OnResponse(GtkWidget* dialog, int response_id) {
  if (response_id == GTK_RESPONSE_OK) {
    host_->PauseSession(false);
  } else {
    host_->Shutdown(NULL);
  }
  Hide();
}

ContinueWindow* ContinueWindow::Create() {
  return new ContinueWindowLinux();
}

}  // namespace remoting
