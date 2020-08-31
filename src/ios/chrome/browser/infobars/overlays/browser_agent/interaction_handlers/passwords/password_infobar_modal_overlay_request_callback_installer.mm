// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/passwords/password_infobar_modal_overlay_request_callback_installer.h"

#include "base/check.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/passwords/password_infobar_modal_interaction_handler.h"
#include "ios/chrome/browser/infobars/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/password_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/password_infobar_modal_overlay_responses.h"
#include "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_infobar_modal_responses::UpdateCredentialsInfo;
using password_infobar_modal_responses::NeverSaveCredentials;
using password_infobar_modal_responses::PresentPasswordSettings;

PasswordInfobarModalOverlayRequestCallbackInstaller::
    PasswordInfobarModalOverlayRequestCallbackInstaller(
        PasswordInfobarModalInteractionHandler* interaction_handler)
    : InfobarModalOverlayRequestCallbackInstaller(
          PasswordInfobarModalOverlayRequestConfig::RequestSupport(),
          interaction_handler),
      interaction_handler_(interaction_handler) {
  DCHECK(interaction_handler_);
}

PasswordInfobarModalOverlayRequestCallbackInstaller::
    ~PasswordInfobarModalOverlayRequestCallbackInstaller() = default;

#pragma mark - Private

void PasswordInfobarModalOverlayRequestCallbackInstaller::
    UpdateCredentialsCallback(OverlayRequest* request,
                              OverlayResponse* response) {
  InfoBarIOS* infobar = GetOverlayRequestInfobar(request);
  if (!infobar)
    return;
  UpdateCredentialsInfo* info = response->GetInfo<UpdateCredentialsInfo>();
  interaction_handler_->UpdateCredentials(GetOverlayRequestInfobar(request),
                                          info->username(), info->password());
}

void PasswordInfobarModalOverlayRequestCallbackInstaller::
    NeverSaveCredentialsCallback(OverlayRequest* request,
                                 OverlayResponse* response) {
  InfoBarIOS* infobar = GetOverlayRequestInfobar(request);
  if (!infobar)
    return;
  // Inform the interaction handler to never save credentials, then add the
  // infobar removal callback as a completion.  This causes the infobar and its
  // badge to be removed once the infobar modal's dismissal finishes.
  interaction_handler_->NeverSaveCredentials(GetOverlayRequestInfobar(request));
  request->GetCallbackManager()->AddCompletionCallback(
      base::BindOnce(&PasswordInfobarModalOverlayRequestCallbackInstaller::
                         RemoveInfobarCompletionCallback,
                     weak_factory_.GetWeakPtr(), request));
}

void PasswordInfobarModalOverlayRequestCallbackInstaller::
    PresentPasswordsSettingsCallback(OverlayRequest* request,
                                     OverlayResponse* response) {
  // Add a completion callback to trigger the presentation of the password
  // settings view once the infobar modal's dismissal finishes.
  request->GetCallbackManager()->AddCompletionCallback(
      base::BindOnce(&PasswordInfobarModalOverlayRequestCallbackInstaller::
                         PresentPasswordSettingsCompletionCallback,
                     weak_factory_.GetWeakPtr(), request));
}

void PasswordInfobarModalOverlayRequestCallbackInstaller::
    RemoveInfobarCompletionCallback(OverlayRequest* request,
                                    OverlayResponse* response) {
  InfoBarIOS* infobar = GetOverlayRequestInfobar(request);
  if (!infobar)
    return;
  InfoBarManagerImpl::FromWebState(request->GetQueueWebState())
      ->RemoveInfoBar(infobar);
}

void PasswordInfobarModalOverlayRequestCallbackInstaller::
    PresentPasswordSettingsCompletionCallback(OverlayRequest* request,
                                              OverlayResponse* response) {
  InfoBarIOS* infobar = GetOverlayRequestInfobar(request);
  if (!infobar)
    return;
  interaction_handler_->PresentPasswordsSettings(infobar);
}

#pragma mark - OverlayRequestCallbackInstaller

void PasswordInfobarModalOverlayRequestCallbackInstaller::
    InstallCallbacksInternal(OverlayRequest* request) {
  InfobarModalOverlayRequestCallbackInstaller::InstallCallbacksInternal(
      request);
  OverlayCallbackManager* manager = request->GetCallbackManager();
  manager->AddDispatchCallback(OverlayDispatchCallback(
      base::BindRepeating(&PasswordInfobarModalOverlayRequestCallbackInstaller::
                              UpdateCredentialsCallback,
                          weak_factory_.GetWeakPtr(), request),
      UpdateCredentialsInfo::ResponseSupport()));
  manager->AddDispatchCallback(OverlayDispatchCallback(
      base::BindRepeating(&PasswordInfobarModalOverlayRequestCallbackInstaller::
                              NeverSaveCredentialsCallback,
                          weak_factory_.GetWeakPtr(), request),
      NeverSaveCredentials::ResponseSupport()));
  manager->AddDispatchCallback(OverlayDispatchCallback(
      base::BindRepeating(&PasswordInfobarModalOverlayRequestCallbackInstaller::
                              PresentPasswordsSettingsCallback,
                          weak_factory_.GetWeakPtr(), request),
      PresentPasswordSettings::ResponseSupport()));
}
