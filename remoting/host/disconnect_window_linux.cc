// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/disconnect_window.h"

#include <gtk/gtk.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "remoting/host/chromoting_host.h"
#include "ui/base/gtk/gtk_signal.h"

namespace remoting {

class DisconnectWindowLinux : public DisconnectWindow {
 public:
  DisconnectWindowLinux();
  virtual ~DisconnectWindowLinux();

  virtual void Show(ChromotingHost* host,
                    const std::string& username) OVERRIDE;
  virtual void Hide() OVERRIDE;

 private:
  CHROMEGTK_CALLBACK_1(DisconnectWindowLinux, void, OnResponse, int);

  void CreateWindow();

  ChromotingHost* host_;
  GtkWidget* disconnect_window_;
  GtkWidget* user_label_;

  DISALLOW_COPY_AND_ASSIGN(DisconnectWindowLinux);
};

DisconnectWindowLinux::DisconnectWindowLinux()
    : host_(NULL),
      disconnect_window_(NULL) {
}

DisconnectWindowLinux::~DisconnectWindowLinux() {
}

void DisconnectWindowLinux::CreateWindow() {
  if (disconnect_window_) return;

  std::string disconnect_button(kDisconnectButton);
  disconnect_button += kDisconnectKeysLinux;
  disconnect_window_ = gtk_dialog_new_with_buttons(
      kTitle,
      NULL,
      GTK_DIALOG_NO_SEPARATOR,
      disconnect_button.c_str(), GTK_RESPONSE_OK,
      NULL);

  GtkWindow* window = GTK_WINDOW(disconnect_window_);
  gtk_window_set_resizable(window, FALSE);
  // Try to keep the window always visible.
  gtk_window_stick(window);
  gtk_window_set_keep_above(window, TRUE);
  // Utility windows have no minimize button or taskbar presence.
  gtk_window_set_type_hint(window, GDK_WINDOW_TYPE_HINT_UTILITY);
  gtk_window_set_deletable(window, FALSE);

  g_signal_connect(disconnect_window_, "response",
                   G_CALLBACK(OnResponseThunk), this);

  GtkWidget* content_area =
      gtk_dialog_get_content_area(GTK_DIALOG(disconnect_window_));

  GtkWidget* username_row = gtk_hbox_new(FALSE, 0);
  // TODO(lambroslambrou): Replace the magic number with an appropriate
  // constant from a header file (such as chrome/browser/ui/gtk/gtk_util.h
  // but check_deps disallows its #inclusion here).
  gtk_container_set_border_width(GTK_CONTAINER(username_row), 12);
  gtk_container_add(GTK_CONTAINER(content_area), username_row);

  GtkWidget* share_label = gtk_label_new(kSharingWith);
  gtk_container_add(GTK_CONTAINER(username_row), share_label);

  user_label_ = gtk_label_new(NULL);
  gtk_container_add(GTK_CONTAINER(username_row), user_label_);

  gtk_widget_show_all(content_area);
}

void DisconnectWindowLinux::Show(ChromotingHost* host,
                                 const std::string& username) {
  host_ = host;
  CreateWindow();
  gtk_label_set_text(GTK_LABEL(user_label_), username.c_str());
  gtk_window_present(GTK_WINDOW(disconnect_window_));
}

void DisconnectWindowLinux::Hide() {
  if (disconnect_window_) {
    gtk_widget_destroy(disconnect_window_);
    disconnect_window_ = NULL;
  }
}

void DisconnectWindowLinux::OnResponse(GtkWidget* dialog, int response_id) {
  // |response_id| is ignored, because there is only one button, and pressing
  // it should have the same effect as closing the window (if the window Close
  // button were visible).
  CHECK(host_);

  host_->Shutdown(NULL);
  Hide();
}

DisconnectWindow* DisconnectWindow::Create() {
  return new DisconnectWindowLinux;
}

}  // namespace remoting
