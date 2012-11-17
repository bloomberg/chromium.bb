// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <webkit/webkitwebview.h>

#include "base/at_exit.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "net/base/net_util.h"
#include "remoting/base/common_resources.h"
#include "remoting/base/resources.h"
#include "remoting/base/string_resources.h"
#include "remoting/host/setup/host_starter.h"
#include "remoting/host/setup/oauth_helper.h"
#include "remoting/host/setup/pin_validator.h"
#include "remoting/host/url_request_context.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/scoped_gobject.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

namespace remoting {

class LinuxHostSetupWizard {
 public:
  enum Pages {
    kAuthPage = 0,
    kPinPage = 1,
    kProgressPage = 2,
    kStartedPage = 3,
    kErrorPage = 4,
    kStoppedPage = 5,
  };

  LinuxHostSetupWizard();
  ~LinuxHostSetupWizard();

  void Init();
  void Run();

 private:
  CHROMEGTK_CALLBACK_1(LinuxHostSetupWizard, gboolean, OnDelete, GdkEvent*);
  CHROMEGTK_CALLBACK_1(LinuxHostSetupWizard, void, OnAuthUriChanged,
                       GParamSpec*);
  CHROMEGTK_CALLBACK_0(LinuxHostSetupWizard, void, OnPinOkClicked);

  GtkWidget* AddPage(int expected_index);
  GtkWidget* AddTextFieldWithLabel(GtkContainer* container,
                                   const std::string& message);
  void AddButtonsBox(GtkContainer* container, GCallback ok_callback);

  void ShowAuthenticationPrompt();
  void ShowProgressMessage(const std::string& message);
  void ShowErrorMessage(const std::string& message);

  void ShowMessageBox(const std::string& message);

  void OnHostStarted(HostStarter::Result result);

  MessageLoopForUI main_message_loop_;
  base::RunLoop run_loop_;

  scoped_ptr<base::Thread> io_thread_;

  scoped_refptr<URLRequestContextGetter> url_context_getter_;

  GtkWidget* main_window_;
  GtkWidget* notebook_;

  // TODO(sergeyu): Replace auth code with authentication.
  GtkWidget* auth_web_view_;
  GtkWidget* pin_entry_;
  GtkWidget* pin_repeat_entry_;
  GtkWidget* progress_message_;
  GtkWidget* error_message_;

  std::string auth_code_;

  scoped_ptr<HostStarter> starter_;

  DISALLOW_COPY_AND_ASSIGN(LinuxHostSetupWizard);
};

LinuxHostSetupWizard::LinuxHostSetupWizard() {
}

LinuxHostSetupWizard::~LinuxHostSetupWizard() {
  gtk_widget_destroy(main_window_);
}

void LinuxHostSetupWizard::Init() {
  io_thread_.reset(new base::Thread("IOThread"));
  io_thread_->StartWithOptions(
      base::Thread::Options(MessageLoop::TYPE_IO, 0));

  url_context_getter_ = new URLRequestContextGetter(
      main_message_loop_.message_loop_proxy(),
      io_thread_->message_loop_proxy());

  main_window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g_signal_connect(main_window_, "delete-event",
                   G_CALLBACK(OnDeleteThunk), this);
  gtk_window_set_title(
      GTK_WINDOW(main_window_),
      l10n_util::GetStringUTF8(IDR_PRODUCT_NAME).c_str());
  gtk_window_set_resizable(GTK_WINDOW(main_window_), FALSE);

  gfx::Image logo = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
    IDR_PRODUCT_LOGO_32);
  ui::ScopedGObject<GdkPixbuf>::Type pixbuf(logo.ToGdkPixbuf());
  gtk_window_set_icon(GTK_WINDOW(main_window_), pixbuf.get());

  notebook_ = gtk_notebook_new();
  gtk_container_add(GTK_CONTAINER(main_window_), notebook_);
  gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook_), FALSE);
  gtk_notebook_set_show_border(GTK_NOTEBOOK(notebook_), FALSE);

  GtkWidget* auth_page = AddPage(kAuthPage);
  auth_web_view_ = webkit_web_view_new();
  gtk_container_add(GTK_CONTAINER(auth_page), auth_web_view_);
  g_signal_connect(auth_web_view_, "notify::uri",
                   G_CALLBACK(OnAuthUriChangedThunk), this);

  GtkWidget* pin_page = AddPage(kPinPage);
  pin_entry_ = AddTextFieldWithLabel(
      GTK_CONTAINER(pin_page),
      l10n_util::GetStringUTF8(IDR_ASK_PIN_DIALOG_LABEL));
  pin_repeat_entry_ = AddTextFieldWithLabel(
      GTK_CONTAINER(pin_page),
      l10n_util::GetStringUTF8(IDR_ASK_PIN_DIALOG_CONFIRM_LABEL));
  AddButtonsBox(GTK_CONTAINER(pin_page), G_CALLBACK(OnPinOkClickedThunk));

  GtkWidget* progress_page = AddPage(kProgressPage);
  progress_message_ = gtk_label_new("");
  gtk_container_add(GTK_CONTAINER(progress_page), progress_message_);

  GtkWidget* started_page = AddPage(kStartedPage);
  GtkWidget* started_message = gtk_label_new(
      l10n_util::GetStringUTF8(IDR_HOST_SETUP_STARTED).c_str());
  gtk_container_add(GTK_CONTAINER(started_page), started_message);

  GtkWidget* error_page = AddPage(kErrorPage);
  error_message_ = gtk_label_new("");
  gtk_container_add(GTK_CONTAINER(error_page), error_message_);

  GtkWidget* stopped_page = AddPage(kStoppedPage);
  GtkWidget* stopped_message = gtk_label_new(
      l10n_util::GetStringUTF8(IDR_HOST_NOT_STARTED).c_str());
  gtk_container_add(GTK_CONTAINER(stopped_page), stopped_message);

  gtk_widget_show_all(main_window_);

  ShowAuthenticationPrompt();
}

void LinuxHostSetupWizard::Run() {
  gtk_widget_show_all(main_window_);
  run_loop_.Run();
}

GtkWidget* LinuxHostSetupWizard::AddPage(int expected_index) {
  GtkWidget* page = gtk_vbox_new(FALSE, 8);

  int page_index =
      gtk_notebook_append_page(GTK_NOTEBOOK(notebook_), page, NULL);
  DCHECK_EQ(page_index, expected_index);

  return page;
}

GtkWidget* LinuxHostSetupWizard::AddTextFieldWithLabel(
    GtkContainer* container,
    const std::string& message) {
  GtkWidget* box = gtk_hbox_new(FALSE, 8);
  gtk_container_add(container, box);

  GtkWidget* label = gtk_label_new(message.c_str());
  gtk_container_add(GTK_CONTAINER(box), label);

  GtkWidget* entry = gtk_entry_new();
  gtk_container_add(GTK_CONTAINER(box), entry);

  return entry;
}

void LinuxHostSetupWizard::AddButtonsBox(GtkContainer* container,
                                         GCallback ok_callback) {
  GtkWidget* box = gtk_hbox_new(FALSE, 8);
  gtk_container_add(container, box);

  GtkWidget* button =
      gtk_button_new_with_label(l10n_util::GetStringUTF8(IDR_OK).c_str());
  gtk_container_add(GTK_CONTAINER(box), button);
  g_signal_connect(button, "clicked", ok_callback, this);
}

void LinuxHostSetupWizard::ShowAuthenticationPrompt() {
  std::string url = GetOauthStartUrl(GetDefaultOauthRedirectUrl());
  webkit_web_view_load_uri(WEBKIT_WEB_VIEW(auth_web_view_), url.c_str());
  gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_), kAuthPage);
}

void LinuxHostSetupWizard::ShowProgressMessage(const std::string& message) {
  gtk_label_set_text(GTK_LABEL(progress_message_), message.c_str());
  gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_), kProgressPage);
}

void LinuxHostSetupWizard::ShowErrorMessage(const std::string& message) {
  gtk_label_set_text(GTK_LABEL(error_message_), message.c_str());
  gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_), kErrorPage);
}

void LinuxHostSetupWizard::ShowMessageBox(const std::string& message) {
  GtkWidget* dialog = gtk_message_dialog_new(
      GTK_WINDOW(main_window_), static_cast<GtkDialogFlags>(
          GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
      GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", message.c_str());
  g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
  gtk_widget_show_all(dialog);
}

gboolean LinuxHostSetupWizard::OnDelete(GtkWidget* window, GdkEvent* event) {
  run_loop_.Quit();
  return TRUE;
}

void LinuxHostSetupWizard::OnAuthUriChanged(GtkWidget* web_view,
                                            GParamSpec* param_spec) {
  std::string url(webkit_web_view_get_uri(WEBKIT_WEB_VIEW(auth_web_view_)));
  std::string auth_code = GetOauthCodeInUrl(url, GetDefaultOauthRedirectUrl());
  if (!auth_code.empty()) {
    auth_code_ = auth_code;
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_), kPinPage);
  }
}

void LinuxHostSetupWizard::OnPinOkClicked(GtkWidget* button) {
  std::string host_name = net::GetHostName();
  std::string pin = gtk_entry_get_text(GTK_ENTRY(pin_entry_));
  std::string pin_repeat = gtk_entry_get_text(GTK_ENTRY(pin_repeat_entry_));

  if (!IsPinValid(pin)) {
    ShowMessageBox(l10n_util::GetStringUTF8(IDR_INVALID_PIN));
    return;
  }

  if (pin != pin_repeat) {
    ShowMessageBox(l10n_util::GetStringUTF8(IDR_PINS_NOT_EQUAL));
    return;
  }

  starter_ = HostStarter::Create(url_context_getter_.get());

  starter_->StartHost(host_name, pin, false, auth_code_,
                      GetDefaultOauthRedirectUrl(),
                      base::Bind(&LinuxHostSetupWizard::OnHostStarted,
                                 base::Unretained(this)));
  ShowProgressMessage(l10n_util::GetStringUTF8(IDR_HOST_SETUP_STARTING));
}

void LinuxHostSetupWizard::OnHostStarted(HostStarter::Result result) {
  switch (result) {
    case HostStarter::START_COMPLETE:
      gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_), kStartedPage);
      break;

    case HostStarter::NETWORK_ERROR:
    case HostStarter::OAUTH_ERROR:
      ShowErrorMessage(l10n_util::GetStringUTF8(
          IDR_HOST_SETUP_REGISTRATION_FAILED));
      break;

    case HostStarter::START_ERROR:
      ShowErrorMessage(l10n_util::GetStringUTF8(IDR_HOST_SETUP_HOST_FAILED));
      break;
  }
}

}  // namespace remoting

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  base::AtExitManager at_exit_manager;
  ui::RegisterPathProvider();

  remoting::LoadResources(std::string());

  InitLogging(NULL,
              logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
              logging::DONT_LOCK_LOG_FILE,
              logging::APPEND_TO_OLD_LOG_FILE,
              logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);

  gtk_init(&argc, &argv);

  remoting::LinuxHostSetupWizard setup_ui;
  setup_ui.Init();
  setup_ui.Run();

  return 0;
}
