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

const char kContinueWindowTitle[] = "Remoting";
const char kContinueWindowText[] =
    "You are currently sharing this machine with another user. "
    "Please confirm that you want to continue sharing.";
const char kContinueWindowButtonText[] = "Yes, continue sharing";

class ContinueWindowLinux : public remoting::ContinueWindow {
 public:
  ContinueWindowLinux();
  virtual ~ContinueWindowLinux();

  virtual void Show(remoting::ChromotingHost* host) OVERRIDE;
  virtual void Hide() OVERRIDE;

 private:
  CHROMEG_CALLBACK_0(ContinueWindowLinux, void, OnOkClicked, GtkButton*);

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

  continue_window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  GtkWindow* window = GTK_WINDOW(continue_window_);
  gtk_window_set_title(window, kContinueWindowTitle);
  gtk_window_set_resizable(window, FALSE);

  g_signal_connect(window, "delete-event",
                   G_CALLBACK(gtk_widget_hide_on_delete), NULL);

  GtkWidget* main_area = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(continue_window_), main_area);

  GtkWidget* text_row = gtk_hbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(main_area), text_row);

  GtkWidget* text_label = gtk_label_new(kContinueWindowText);
  gtk_label_set_line_wrap(GTK_LABEL(text_label), TRUE);
  gtk_container_add(GTK_CONTAINER(text_row), text_label);

  GtkWidget* button_box = gtk_hbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(main_area), button_box);

  GtkWidget* ok_button = gtk_button_new_with_label(kContinueWindowButtonText);
  gtk_box_pack_start(GTK_BOX(button_box), ok_button,
                     TRUE, FALSE, 0);

  g_signal_connect(ok_button, "clicked",
                   G_CALLBACK(OnOkClickedThunk), this);

  gtk_widget_show_all(main_area);
}

void ContinueWindowLinux::Show(remoting::ChromotingHost* host) {
  host_ = host;
  CreateWindow();
  gtk_window_present(GTK_WINDOW(continue_window_));
}

void ContinueWindowLinux::Hide() {
  CHECK(continue_window_);

  gtk_widget_hide(continue_window_);
}

void ContinueWindowLinux::OnOkClicked(GtkButton* sender) {
  CHECK(host_);

  host_->PauseSession(false);
  Hide();
}

ContinueWindow* ContinueWindow::Create() {
  return new ContinueWindowLinux();
}

}  // namespace remoting
