// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_icon_linux_wrapper.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/views/linux_ui/linux_ui.h"

#if defined(USE_DBUS)
#include "chrome/browser/ui/views/status_icons/status_icon_linux_dbus.h"
#endif

#if defined(USE_X11)
#include "chrome/browser/ui/views/status_icons/status_icon_linux_x11.h"
#endif

namespace {

constexpr base::Feature kEnableDbusAndX11StatusIcons{
    "EnableDbusAndX11StatusIcons", base::FEATURE_DISABLED_BY_DEFAULT};

// Prefix for app indicator ids
const char kAppIndicatorIdPrefix[] = "chrome_app_indicator_";

}  // namespace

StatusIconLinuxWrapper::StatusIconLinuxWrapper(
    std::unique_ptr<views::StatusIconLinux> status_icon)
    : status_icon_(std::move(status_icon)), menu_model_(nullptr) {
  status_icon_->set_delegate(this);
}

StatusIconLinuxWrapper::~StatusIconLinuxWrapper() {
  if (menu_model_)
    menu_model_->RemoveObserver(this);
}

void StatusIconLinuxWrapper::SetImage(const gfx::ImageSkia& image) {
  status_icon_->SetImage(image);
}

void StatusIconLinuxWrapper::SetToolTip(const base::string16& tool_tip) {
  status_icon_->SetToolTip(tool_tip);
}

void StatusIconLinuxWrapper::DisplayBalloon(
    const gfx::ImageSkia& icon,
    const base::string16& title,
    const base::string16& contents,
    const message_center::NotifierId& notifier_id) {
  notification_.DisplayBalloon(icon, title, contents, notifier_id);
}

void StatusIconLinuxWrapper::OnClick() {
  DispatchClickEvent();
}

bool StatusIconLinuxWrapper::HasClickAction() {
  return HasObservers();
}

void StatusIconLinuxWrapper::OnMenuStateChanged() {
  status_icon_->RefreshPlatformContextMenu();
}

std::unique_ptr<StatusIconLinuxWrapper>
StatusIconLinuxWrapper::CreateWrappedStatusIcon(
    const gfx::ImageSkia& image,
    const base::string16& tool_tip) {
  if (base::FeatureList::IsEnabled(kEnableDbusAndX11StatusIcons)) {
#if defined(USE_DBUS)
    return base::WrapUnique(new StatusIconLinuxWrapper(
        std::make_unique<StatusIconLinuxDbus>(image, tool_tip)));
#endif
    // TODO(thomasanderson): Set up a mechanism for replacing |status_icon_|
    // with a StatusIconLinuxX11 in case the system doesn't support DBus status
    // icons, which we can only discover asynchronously.
  }
  const views::LinuxUI* linux_ui = views::LinuxUI::instance();
  if (linux_ui) {
    auto status_icon =
        linux_ui->CreateLinuxStatusIcon(image, tool_tip, kAppIndicatorIdPrefix);
    if (status_icon) {
      return base::WrapUnique(
          new StatusIconLinuxWrapper(std::move(status_icon)));
    }
  }
  return nullptr;
}

void StatusIconLinuxWrapper::UpdatePlatformContextMenu(
    StatusIconMenuModel* model) {
  // If a menu already exists, remove ourself from its observer list.
  if (menu_model_)
    menu_model_->RemoveObserver(this);

  status_icon_->UpdatePlatformContextMenu(model);
  menu_model_ = model;

  if (model)
    model->AddObserver(this);
}
