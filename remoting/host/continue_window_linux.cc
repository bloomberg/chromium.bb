// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "remoting/base/string_resources.h"
#include "remoting/host/continue_window.h"
#include "ui/base/glib/glib_signal.h"
#include "ui/base/l10n/l10n_util.h"

namespace remoting {

class ContinueWindowGtk : public ContinueWindow {
 public:
  ContinueWindowGtk();
  virtual ~ContinueWindowGtk();

 protected:
  // ContinueWindow overrides.
  virtual void ShowUi() OVERRIDE;
  virtual void HideUi() OVERRIDE;

 private:
  void CreateWindow();

  CHROMEG_CALLBACK_1(ContinueWindowGtk, void, OnResponse, GtkDialog*, int);

  GtkWidget* continue_window_;

  DISALLOW_COPY_AND_ASSIGN(ContinueWindowGtk);
};

ContinueWindowGtk::ContinueWindowGtk()
    : continue_window_(NULL) {
}

ContinueWindowGtk::~ContinueWindowGtk() {
  if (continue_window_) {
    gtk_widget_destroy(continue_window_);
    continue_window_ = NULL;
  }
}

void ContinueWindowGtk::ShowUi() {
  DCHECK(CalledOnValidThread());
  DCHECK(!continue_window_);

  CreateWindow();
  gtk_window_set_urgency_hint(GTK_WINDOW(continue_window_), TRUE);
  gtk_window_present(GTK_WINDOW(continue_window_));
}

void ContinueWindowGtk::HideUi() {
  DCHECK(CalledOnValidThread());

  if (continue_window_) {
    gtk_widget_destroy(continue_window_);
    continue_window_ = NULL;
  }
}

void ContinueWindowGtk::CreateWindow() {
  DCHECK(CalledOnValidThread());
  DCHECK(!continue_window_);

  continue_window_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_PRODUCT_NAME).c_str(),
      NULL,
      static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      l10n_util::GetStringUTF8(IDS_STOP_SHARING_BUTTON).c_str(),
      GTK_RESPONSE_CANCEL,
      l10n_util::GetStringUTF8(IDS_CONTINUE_BUTTON).c_str(),
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
      gtk_label_new(l10n_util::GetStringUTF8(IDS_CONTINUE_PROMPT).c_str());
  gtk_label_set_line_wrap(GTK_LABEL(text_label), TRUE);
  // TODO(lambroslambrou): Fix magic numbers, as in disconnect_window_gtk.cc.
  gtk_misc_set_padding(GTK_MISC(text_label), 12, 12);
  gtk_container_add(GTK_CONTAINER(content_area), text_label);

  gtk_widget_show_all(content_area);
}

void ContinueWindowGtk::OnResponse(GtkDialog* dialog, int response_id) {
  DCHECK(CalledOnValidThread());

  if (response_id == GTK_RESPONSE_OK) {
    ContinueSession();
  } else {
    DisconnectSession();
  }

  HideUi();
}

// static
scoped_ptr<HostWindow> HostWindow::CreateContinueWindow() {
  return scoped_ptr<HostWindow>(new ContinueWindowGtk());
}

}  // namespace remoting
