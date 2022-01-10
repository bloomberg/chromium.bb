// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/receiving_ui_handler_registry.h"

#include <vector>

#include "base/memory/singleton.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/receiving_ui_handler.h"
#include "chrome/browser/share/share_features.h"
#include "components/send_tab_to_self/features.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC) || \
    defined(OS_WIN) || defined(OS_FUCHSIA)
#include "chrome/browser/send_tab_to_self/desktop_notification_handler.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_icon_controller.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/android/send_tab_to_self/android_notification_handler.h"
#endif

namespace send_tab_to_self {

ReceivingUiHandlerRegistry::ReceivingUiHandlerRegistry() {}
ReceivingUiHandlerRegistry::~ReceivingUiHandlerRegistry() {}

// static
ReceivingUiHandlerRegistry* ReceivingUiHandlerRegistry::GetInstance() {
  return base::Singleton<ReceivingUiHandlerRegistry>::get();
}

// Instantiates all the handlers relevant to this platform.
void ReceivingUiHandlerRegistry::InstantiatePlatformSpecificHandlers(
    Profile* profile) {
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC) || \
    defined(OS_WIN) || defined(OS_FUCHSIA)

  // If STTS 2.0 is enabled the handler will be created when the toolbar
  // button registers itself as the delegate.
  if (!base::FeatureList::IsEnabled(kSendTabToSelfV2) &&
      !share::AreUpcomingSharingFeaturesEnabled()) {
    applicable_handlers_.push_back(
        std::make_unique<send_tab_to_self::DesktopNotificationHandler>(
            profile));
  }
#elif defined(OS_ANDROID)
  applicable_handlers_.push_back(
      std::make_unique<AndroidNotificationHandler>(profile));
#endif
}

SendTabToSelfToolbarIconController*
ReceivingUiHandlerRegistry::GetToolbarButtonControllerForProfile(
    Profile* profile) {
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC) || \
    defined(OS_WIN) || defined(OS_FUCHSIA)
  for (const std::unique_ptr<ReceivingUiHandler>& handler :
       applicable_handlers_) {
    auto* button_controller =
        static_cast<SendTabToSelfToolbarIconController*>(handler.get());
    if (button_controller && button_controller->profile() == profile) {
      return button_controller;
    }
  }

  applicable_handlers_.push_back(
      std::make_unique<SendTabToSelfToolbarIconController>(profile));
  auto* button_controller = static_cast<SendTabToSelfToolbarIconController*>(
      applicable_handlers_.back().get());
  return button_controller;
#elif defined(OS_ANDROID)
  return nullptr;
#else
#error Unknown platform.
#endif
}

AndroidNotificationHandler*
ReceivingUiHandlerRegistry::GetAndroidNotificationHandlerForProfile(
    Profile* profile) {
#if defined(OS_ANDROID)
  for (const std::unique_ptr<ReceivingUiHandler>& handler :
       applicable_handlers_) {
    auto* notification_handler =
        static_cast<AndroidNotificationHandler*>(handler.get());
    if (notification_handler && notification_handler->profile() == profile) {
      return notification_handler;
    }
  }
#endif
  return nullptr;
}

const std::vector<std::unique_ptr<ReceivingUiHandler>>&
ReceivingUiHandlerRegistry::GetHandlers() const {
  return applicable_handlers_;
}

void ReceivingUiHandlerRegistry::OnProfileShutdown(Profile* profile) {
  // Remove all handlers for |profile|.
  applicable_handlers_.erase(
      base::ranges::remove_if(
          applicable_handlers_,
          [=](const std::unique_ptr<ReceivingUiHandler>& handler) {
            return handler->profile() == profile;
          }),
      applicable_handlers_.end());
}

}  // namespace send_tab_to_self
