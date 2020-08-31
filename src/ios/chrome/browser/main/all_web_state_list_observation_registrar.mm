// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/all_web_state_list_observation_registrar.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/main/browser_list.h"
#include "ios/chrome/browser/main/browser_list_factory.h"

AllWebStateListObservationRegistrar::AllWebStateListObservationRegistrar(
    ChromeBrowserState* browser_state,
    std::unique_ptr<WebStateListObserver> web_state_list_observer,
    Mode mode)
    : browser_list_(BrowserListFactory::GetForBrowserState(browser_state)),
      web_state_list_observer_(std::move(web_state_list_observer)),
      scoped_observer_(
          std::make_unique<ScopedObserver<WebStateList, WebStateListObserver>>(
              web_state_list_observer_.get())),
      mode_(mode) {
  browser_list_->AddObserver(this);

  // There may already be browsers in |browser_list| when this object is
  // created. Register as an observer for (mode permitting) both the regular and
  // incognito browsers' WebStateLists.
  if (mode_ & Mode::REGULAR) {
    for (Browser* browser : browser_list_->AllRegularBrowsers()) {
      scoped_observer_->Add(browser->GetWebStateList());
    }
  }

  if (mode_ & Mode::INCOGNITO) {
    for (Browser* browser : browser_list_->AllIncognitoBrowsers()) {
      scoped_observer_->Add(browser->GetWebStateList());
    }
  }
}

AllWebStateListObservationRegistrar::AllWebStateListObservationRegistrar(
    ChromeBrowserState* browser_state,
    std::unique_ptr<WebStateListObserver> web_state_list_observer)
    : AllWebStateListObservationRegistrar(browser_state,
                                          std::move(web_state_list_observer),
                                          Mode::ALL) {}

AllWebStateListObservationRegistrar::~AllWebStateListObservationRegistrar() {
  // If the browser state has already shut down, |browser_list_| should be
  // nullptr; otherwise, stop observing it.
  if (browser_list_)
    browser_list_->RemoveObserver(this);
}

void AllWebStateListObservationRegistrar::OnBrowserAdded(
    const BrowserList* browser_list,
    Browser* browser) {
  if (mode_ & Mode::REGULAR)
    scoped_observer_->Add(browser->GetWebStateList());
}

void AllWebStateListObservationRegistrar::OnIncognitoBrowserAdded(
    const BrowserList* browser_list,
    Browser* browser) {
  if (mode_ & Mode::INCOGNITO)
    scoped_observer_->Add(browser->GetWebStateList());
}

void AllWebStateListObservationRegistrar::OnBrowserRemoved(
    const BrowserList* browser_list,
    Browser* browser) {
  if (mode_ & Mode::REGULAR)
    scoped_observer_->Remove(browser->GetWebStateList());
}

void AllWebStateListObservationRegistrar::OnIncognitoBrowserRemoved(
    const BrowserList* browser_list,
    Browser* browser) {
  if (mode_ & Mode::INCOGNITO)
    scoped_observer_->Remove(browser->GetWebStateList());
}

void AllWebStateListObservationRegistrar::OnBrowserListShutdown(
    BrowserList* browser_list) {
  DCHECK_EQ(browser_list, browser_list_);
  // Stop observing all observed web state lists.
  if (mode_ & Mode::REGULAR) {
    for (Browser* browser : browser_list_->AllRegularBrowsers()) {
      scoped_observer_->Remove(browser->GetWebStateList());
    }
  }

  if (mode_ & Mode::INCOGNITO) {
    for (Browser* browser : browser_list_->AllIncognitoBrowsers()) {
      scoped_observer_->Remove(browser->GetWebStateList());
    }
  }
  // Stop observimg the browser list, and clear |browser_list_|.
  browser_list_->RemoveObserver(this);
  browser_list_ = nullptr;
}
