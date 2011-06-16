// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/disconnect_window.h"

#include <gtk/gtk.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "remoting/host/chromoting_host.h"
#include "ui/base/gtk/gtk_signal.h"

namespace {

const char kDisconnectWindowTitle[] = "Remoting";
const char kDisconnectWindowShareText[] = "Sharing with: ";
const char kDisconnectWindowButtonText[] = "Disconnect";

class DisconnectWindowLinux : public remoting::DisconnectWindow {
 public:
  DisconnectWindowLinux();

  virtual void Show(remoting::ChromotingHost* host,
                    const std::string& username) OVERRIDE;
  virtual void Hide() OVERRIDE;

 private:
  CHROMEGTK_CALLBACK_1(DisconnectWindowLinux, gboolean, OnWindowDelete,
                       GdkEvent*);
  CHROMEG_CALLBACK_0(DisconnectWindowLinux, void, OnDisconnectClicked,
                     GtkButton*);

  void CreateWindow();

  remoting::ChromotingHost* host_;
  GtkWidget* disconnect_window_;
  GtkWidget* user_label_;

  DISALLOW_COPY_AND_ASSIGN(DisconnectWindowLinux);
};
}  // namespace

DisconnectWindowLinux::DisconnectWindowLinux()
    : host_(NULL),
      disconnect_window_(NULL) {
}

void DisconnectWindowLinux::CreateWindow() {
  if (disconnect_window_) return;

  disconnect_window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  GtkWindow* window = GTK_WINDOW(disconnect_window_);
  gtk_window_set_title(window, kDisconnectWindowTitle);
  gtk_window_set_resizable(window, FALSE);
  // Try to keep the window always visible.
  gtk_window_stick(window);
  gtk_window_set_keep_above(window, TRUE);
  // Utility windows have no minimize button or taskbar presence.
  gtk_window_set_type_hint(window, GDK_WINDOW_TYPE_HINT_UTILITY);
  gtk_window_set_deletable(window, FALSE);

  g_signal_connect(disconnect_window_, "delete-event",
                   G_CALLBACK(OnWindowDeleteThunk), this);

  GtkWidget* main_area = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(disconnect_window_), main_area);

  GtkWidget* username_row = gtk_hbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(main_area), username_row);

  GtkWidget* share_label = gtk_label_new(kDisconnectWindowShareText);
  gtk_container_add(GTK_CONTAINER(username_row), share_label);

  user_label_ = gtk_label_new(NULL);
  gtk_container_add(GTK_CONTAINER(username_row), user_label_);

  GtkWidget* disconnect_box = gtk_hbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(main_area), disconnect_box);

  GtkWidget* disconnect_button = gtk_button_new_with_label(
      kDisconnectWindowButtonText);
  gtk_box_pack_start(GTK_BOX(disconnect_box), disconnect_button,
                     TRUE, FALSE, 0);

  g_signal_connect(disconnect_button, "clicked",
                   G_CALLBACK(OnDisconnectClickedThunk), this);

  gtk_widget_show_all(main_area);
}

void DisconnectWindowLinux::Show(remoting::ChromotingHost* host,
                                 const std::string& username) {
  host_ = host;
  CreateWindow();
  gtk_label_set_text(GTK_LABEL(user_label_), username.c_str());
  gtk_window_present(GTK_WINDOW(disconnect_window_));
}

void DisconnectWindowLinux::Hide() {
  DCHECK(disconnect_window_);

  gtk_widget_hide(disconnect_window_);
}

gboolean DisconnectWindowLinux::OnWindowDelete(GtkWidget* widget,
                                               GdkEvent* event) {
  // Don't allow the window to be closed.
  return TRUE;
}

void DisconnectWindowLinux::OnDisconnectClicked(GtkButton* sender) {
  DCHECK(host_);
  host_->Shutdown();
}

remoting::DisconnectWindow* remoting::DisconnectWindow::Create() {
  return new DisconnectWindowLinux;
}
