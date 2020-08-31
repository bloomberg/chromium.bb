// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_CONTROLLER_EMITTER_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_CONTROLLER_EMITTER_H_

#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/autocomplete_controller.h"

#if !defined(OS_IOS)
#include "content/public/browser/browser_context.h"
#endif  // !defined(OS_IOS)

// This KeyedService is meant to observe multiple AutocompleteController
// instances and forward the notifications to its own observers.
// Its main purpose is to act as a bridge between the chrome://omnibox WebUI
// handler, and the many usages of AutocompleteController (Views, NTP, Android).
class OmniboxControllerEmitter : public KeyedService,
                                 public AutocompleteController::Observer {
 public:
#if !defined(OS_IOS)
  static OmniboxControllerEmitter* GetForBrowserContext(
      content::BrowserContext* browser_context);
#endif  // !defined(OS_IOS)

  OmniboxControllerEmitter();
  ~OmniboxControllerEmitter() override;

  // Add/remove observer.
  void AddObserver(AutocompleteController::Observer* observer);
  void RemoveObserver(AutocompleteController::Observer* observer);

  // AutocompleteController::Observer:
  void OnStart(AutocompleteController* controller,
               const AutocompleteInput& input) override;
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;

 private:
  base::ObserverList<AutocompleteController::Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxControllerEmitter);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_CONTROLLER_EMITTER_H_
