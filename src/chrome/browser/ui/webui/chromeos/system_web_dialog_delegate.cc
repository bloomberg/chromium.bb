// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"

#include <list>

#include "ash/public/cpp/shell_window_ids.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/chrome_web_dialog_view.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/browser_context.h"
#include "ui/aura/window.h"

namespace chromeos {

namespace {

// Track all open system web dialog instances. This should be a small list.
std::list<SystemWebDialogDelegate*>* GetInstances() {
  static base::NoDestructor<std::list<SystemWebDialogDelegate*>> instances;
  return instances.get();
}

}  // namespace

// static
SystemWebDialogDelegate* SystemWebDialogDelegate::FindInstance(
    const std::string& id) {
  auto* instances = GetInstances();
  auto iter =
      std::find_if(instances->begin(), instances->end(),
                   [id](SystemWebDialogDelegate* i) { return i->Id() == id; });
  return iter == instances->end() ? nullptr : *iter;
}

SystemWebDialogDelegate::SystemWebDialogDelegate(const GURL& gurl,
                                                 const base::string16& title)
    : gurl_(gurl), title_(title), modal_type_(ui::MODAL_TYPE_NONE) {
  switch (session_manager::SessionManager::Get()->session_state()) {
    // Normally system dialogs are not modal.
    case session_manager::SessionState::UNKNOWN:
    case session_manager::SessionState::LOGGED_IN_NOT_ACTIVE:
    case session_manager::SessionState::ACTIVE:
      break;
    // These states use an overlay so dialogs must be modal.
    case session_manager::SessionState::OOBE:
    case session_manager::SessionState::LOGIN_PRIMARY:
    case session_manager::SessionState::LOCKED:
    case session_manager::SessionState::LOGIN_SECONDARY:
      modal_type_ = ui::MODAL_TYPE_SYSTEM;
      break;
  }
  GetInstances()->push_back(this);
}

SystemWebDialogDelegate::~SystemWebDialogDelegate() {
  base::EraseIf(*GetInstances(),
                [this](SystemWebDialogDelegate* i) { return i == this; });
}

const std::string& SystemWebDialogDelegate::Id() {
  return gurl_.spec();
}

void SystemWebDialogDelegate::Focus() {
  // Focusing a modal dialog does not make it the topmost dialog and does not
  // enable interaction. It does however remove focus from the current dialog,
  // preventing interaction with any dialog. TODO(stevenjb): Investigate and
  // fix, https://crbug.com/914133.
  if (modal_type_ == ui::MODAL_TYPE_NONE)
    dialog_window()->Focus();
}

ui::ModalType SystemWebDialogDelegate::GetDialogModalType() const {
  return modal_type_;
}

base::string16 SystemWebDialogDelegate::GetDialogTitle() const {
  return title_;
}

GURL SystemWebDialogDelegate::GetDialogContentURL() const {
  return gurl_;
}

void SystemWebDialogDelegate::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {}

void SystemWebDialogDelegate::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDialogWidth, kDialogHeight);
}

std::string SystemWebDialogDelegate::GetDialogArgs() const {
  return std::string();
}

void SystemWebDialogDelegate::OnDialogShown(
    content::WebUI* webui,
    content::RenderViewHost* render_view_host) {
  webui_ = webui;
}

void SystemWebDialogDelegate::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void SystemWebDialogDelegate::OnCloseContents(content::WebContents* source,
                                              bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool SystemWebDialogDelegate::ShouldShowDialogTitle() const {
  return !title_.empty();
}

void SystemWebDialogDelegate::ShowSystemDialog(gfx::NativeWindow parent) {
  content::BrowserContext* browser_context =
      ProfileManager::GetActiveUserProfile();
  views::Widget::InitParams extra_params;
  // If unparented and not modal, keep it on top (see header comment).
  if (!parent && GetDialogModalType() == ui::MODAL_TYPE_NONE)
    extra_params.keep_on_top = true;
  dialog_window_ = chrome::ShowWebDialogWithParams(parent, browser_context,
                                                   this, &extra_params);
}

}  // namespace chromeos
