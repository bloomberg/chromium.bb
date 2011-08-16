// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/disconnect_window.h"

#include <gtk/gtk.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/ui_strings.h"
#include "ui/base/gtk/gtk_signal.h"

namespace {
// The width in pixels at which the message will wrap. Given that the message
// contains an un-splittable email address, it's unlikely that a fixed width
// is going to look aesthetically pleasing in all languages.
// TODO(jamiewalch): Replace this with a layout that only uses a single line,
// and which is docked at the top or bottom of the host screen, as in our
// UI mocks.
const int kMessageWidth = 300;
}

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

  void CreateWindow(UiStrings* ui_strings);

  ChromotingHost* host_;
  GtkWidget* disconnect_window_;
  GtkWidget* message_;

  DISALLOW_COPY_AND_ASSIGN(DisconnectWindowLinux);
};

DisconnectWindowLinux::DisconnectWindowLinux()
    : host_(NULL),
      disconnect_window_(NULL) {
}

DisconnectWindowLinux::~DisconnectWindowLinux() {
}

void DisconnectWindowLinux::CreateWindow(UiStrings* ui_strings) {
  if (disconnect_window_) return;

  disconnect_window_ = gtk_dialog_new_with_buttons(
      ui_strings->product_name.c_str(),
      NULL,
      GTK_DIALOG_NO_SEPARATOR,
      ui_strings->disconnect_button_text_plus_shortcut.c_str(), GTK_RESPONSE_OK,
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

  GtkWidget* message_row = gtk_hbox_new(FALSE, 0);
  // TODO(lambroslambrou): Replace the magic number with an appropriate
  // constant from a header file (such as chrome/browser/ui/gtk/gtk_util.h
  // but check_deps disallows its #inclusion here).
  gtk_container_set_border_width(GTK_CONTAINER(message_row), 12);
  gtk_container_add(GTK_CONTAINER(content_area), message_row);

  message_ = gtk_label_new(NULL);
  gtk_widget_set_size_request(message_, kMessageWidth, -1);
  gtk_label_set_line_wrap(GTK_LABEL(message_), true);
  gtk_container_add(GTK_CONTAINER(message_row), message_);

  gtk_widget_show_all(content_area);
}

void DisconnectWindowLinux::Show(ChromotingHost* host,
                                 const std::string& /* unused */) {
  host_ = host;
  CreateWindow(host->ui_strings());
  gtk_label_set_text(GTK_LABEL(message_),
                     host->ui_strings()->disconnect_message.c_str());
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
